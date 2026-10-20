#include "libssh.h"
#include "tweetnacl.h"

static void ssh_inbound_lock_acquire(ssh_session_t* session) {
    while (__sync_lock_test_and_set(&session->inbound_lock, 1));
}

static void ssh_inbound_lock_release(ssh_session_t* session) {
    __sync_lock_release(&session->inbound_lock);
}

static void ssh_outbound_lock_acquire(ssh_session_t* session) {
    while (__sync_lock_test_and_set(&session->outbound_lock, 1));
}

static void ssh_outbound_lock_release(ssh_session_t* session) {
    __sync_lock_release(&session->outbound_lock);
}

void ssh_lock_acquire(ssh_session_t* session) {
    while (__sync_lock_test_and_set(&session->user_lock, 1));
}

void ssh_lock_release(ssh_session_t* session) {
    __sync_lock_release(&session->user_lock);
}

void ssh_dh_generate_keys(ssh_dh_ctx_t* ctx)
{
    if (!ctx) return;
    crypto_box_keypair(ctx->public_y, ctx->secret_x);
}

void ssh_dh_compute_shared_secret(ssh_dh_ctx_t* ctx)
{
    if (!ctx) return;
    crypto_scalarmult(ctx->shared_k, ctx->secret_x, ctx->client_e);
}

void ssh_init_session(ssh_session_t* session, int fd, ssh_read_fn r_fn, ssh_write_fn w_fn)
{
    if (!session) return;
    ssh_memset(session, 0, sizeof(ssh_session_t));
    session->client_fd = fd;
    session->read_hardware = r_fn;
    session->write_hardware = w_fn;
    session->state = SSH_STATE_EXCHANGE_BANNERS;
    crypto_sign_keypair(session->host_pubkey, session->host_privkey);
}

int ssh_send_packet(ssh_session_t *session, unsigned char msg_type, const unsigned char *payload, unsigned long payload_len)
{
    if (!session || !session->write_hardware)
        return -1;

    if (payload_len > 0 && !payload)
        return -1;

    unsigned long block_size = session->encrypted ? 16 : 8;
    unsigned long mac_len = session->encrypted ? 32 : 0;

    if (payload_len > SSH_MAX_PACKET_LEN - 64)
        return -1;

    unsigned long total_sem_padding = 4 + 1 + 1 + payload_len;
    unsigned long padding_len = block_size - (total_sem_padding % block_size);
    
    while (padding_len < SSH_MIN_PAD_LEN)
    {
        padding_len += block_size;
    }

    unsigned long packet_length_ul = 1 + 1 + payload_len + padding_len;
    unsigned long packet_bytes = 4 + packet_length_ul;

    if (packet_length_ul > 0xFFFFFFFFUL ||
        packet_bytes + mac_len > SSH_MAX_PACKET_LEN)
        return -1;

    ssh_outbound_lock_acquire(session);

    session->tx_buffer[0] = (unsigned char)(packet_length_ul >> 24);
    session->tx_buffer[1] = (unsigned char)(packet_length_ul >> 16);
    session->tx_buffer[2] = (unsigned char)(packet_length_ul >> 8);
    session->tx_buffer[3] = (unsigned char)packet_length_ul;

    session->tx_buffer[4] = (unsigned char)padding_len;
    session->tx_buffer[5] = msg_type;

    for (unsigned long i = 0; i < payload_len; i++)
        session->tx_buffer[6 + i] = payload[i];

    randombytes(
        &session->tx_buffer[6 + payload_len],
        padding_len);

    if (session->encrypted)
    {
        unsigned char mac[32];

        ssh_hmac_sha256_packet(
            session->outbound_mac_key,
            session->outbound_sequence,
            session->tx_buffer,
            packet_bytes,
            mac);

        aes128_ctr_crypt(
            session->outbound_key,
            session->outbound_iv,
            session->tx_buffer,
            session->tx_buffer,
            packet_bytes);

        for (unsigned long i = 0; i < sizeof(mac); i++)
            session->tx_buffer[packet_bytes + i] = mac[i];
    }

    session->outbound_sequence++;

    // Barreira de memória para garantir consistência de cache antes do I/O de hardware
    __asm__ __volatile__("" ::: "memory");

    unsigned long total_wire_bytes = packet_bytes + mac_len;
    unsigned long bytes_sent = 0;

    while (bytes_sent < total_wire_bytes)
    {
        long result = session->write_hardware(
            session->client_fd,
            &session->tx_buffer[bytes_sent],
            total_wire_bytes - bytes_sent);

        if (result <= 0 ||
            (unsigned long)result > total_wire_bytes - bytes_sent) {
                ssh_outbound_lock_release(session);
                return -3;
            }

        bytes_sent += (unsigned long)result;
    }

    ssh_outbound_lock_release(session);

    return (int)bytes_sent;
}


int ssh_build_kexdh_reply(ssh_session_t *session, unsigned char *response, unsigned long max_len, unsigned long *out_len)
{
    if (!session || !response || !out_len || max_len < 512)
        return -1;

    if (session->client_banner_len > sizeof(session->client_banner) ||
        session->client_kexinit_len > sizeof(session->client_kexinit) ||
        session->server_kexinit_len > sizeof(session->server_kexinit))
        return -1;

    unsigned char h_buffer[4096];
    unsigned long h_idx = 0;

    char *c_banner = session->client_banner;
    unsigned long c_banner_len = session->client_banner_len;

    while (c_banner_len > 0 &&
           (c_banner[c_banner_len - 1] == '\r' ||
            c_banner[c_banner_len - 1] == '\n'))
        c_banner_len--;

    char *s_banner = "SSH-2.0-SiriusSSH_0.1";
    unsigned long s_banner_len = 21;

    char *host_key_fmt = "ssh-ed25519";
    unsigned long host_key_fmt_len = 11;
    unsigned long blob_len = 4 + host_key_fmt_len + 4 + 32;

    // Tamanho necessário recalculado com base na especificação estrita
    unsigned long required =
        4 + c_banner_len +
        4 + s_banner_len +
        4 + session->client_kexinit_len +
        4 + session->server_kexinit_len +
        4 + blob_len +
        4 + 32 + // client_e (e)
        4 + 32 + // public_y (f)
        4 + 33;  // partilha_k max (K)

    if (required > sizeof(h_buffer))
        return -1;

    // 1. Client Banner
    h_buffer[h_idx++] = (unsigned char)(c_banner_len >> 24);
    h_buffer[h_idx++] = (unsigned char)(c_banner_len >> 16);
    h_buffer[h_idx++] = (unsigned char)(c_banner_len >> 8);
    h_buffer[h_idx++] = (unsigned char)c_banner_len;
    for (unsigned long i = 0; i < c_banner_len; i++)
        h_buffer[h_idx++] = (unsigned char)c_banner[i];

    // 2. Server Banner
    h_buffer[h_idx++] = (unsigned char)(s_banner_len >> 24);
    h_buffer[h_idx++] = (unsigned char)(s_banner_len >> 16);
    h_buffer[h_idx++] = (unsigned char)(s_banner_len >> 8);
    h_buffer[h_idx++] = (unsigned char)s_banner_len;
    for (unsigned long i = 0; i < s_banner_len; i++)
        h_buffer[h_idx++] = (unsigned char)s_banner[i];

    // 3. Client KEXINIT Payload
    h_buffer[h_idx++] = (unsigned char)(session->client_kexinit_len >> 24);
    h_buffer[h_idx++] = (unsigned char)(session->client_kexinit_len >> 16);
    h_buffer[h_idx++] = (unsigned char)(session->client_kexinit_len >> 8);
    h_buffer[h_idx++] = (unsigned char)session->client_kexinit_len;
    for (unsigned long i = 0; i < session->client_kexinit_len; i++)
        h_buffer[h_idx++] = session->client_kexinit[i];

    // 4. Server KEXINIT Payload
    h_buffer[h_idx++] = (unsigned char)(session->server_kexinit_len >> 24);
    h_buffer[h_idx++] = (unsigned char)(session->server_kexinit_len >> 16);
    h_buffer[h_idx++] = (unsigned char)(session->server_kexinit_len >> 8);
    h_buffer[h_idx++] = (unsigned char)session->server_kexinit_len;
    for (unsigned long i = 0; i < session->server_kexinit_len; i++)
        h_buffer[h_idx++] = session->server_kexinit[i];

    // 5. Server Public Host Key Blob (K_S)
    unsigned long blob_start = h_idx;
    h_buffer[h_idx++] = (unsigned char)(blob_len >> 24);
    h_buffer[h_idx++] = (unsigned char)(blob_len >> 16);
    h_buffer[h_idx++] = (unsigned char)(blob_len >> 8);
    h_buffer[h_idx++] = (unsigned char)blob_len;

    h_buffer[h_idx++] = 0; h_buffer[h_idx++] = 0; h_buffer[h_idx++] = 0;
    h_buffer[h_idx++] = (unsigned char)host_key_fmt_len;
    for (unsigned long i = 0; i < host_key_fmt_len; i++)
        h_buffer[h_idx++] = (unsigned char)host_key_fmt[i];

    h_buffer[h_idx++] = 0; h_buffer[h_idx++] = 0; h_buffer[h_idx++] = 0;
    h_buffer[h_idx++] = 32;
    for (int i = 0; i < 32; i++)
        h_buffer[h_idx++] = session->host_pubkey[i];

    // 6. Client Public Key (e)
    h_buffer[h_idx++] = 0; h_buffer[h_idx++] = 0; h_buffer[h_idx++] = 0;
    h_buffer[h_idx++] = 32;
    for (int i = 0; i < 32; i++)
        h_buffer[h_idx++] = session->dh_ctx.client_e[i];

    // 7. Server Public Key (f)
    h_buffer[h_idx++] = 0; h_buffer[h_idx++] = 0; h_buffer[h_idx++] = 0;
    h_buffer[h_idx++] = 32;
    for (int i = 0; i < 32; i++)
        h_buffer[h_idx++] = session->dh_ctx.public_y[i];

    // 8. Shared Secret (K) como mpint
    if (ssh_append_curve25519_mpint(h_buffer, sizeof(h_buffer), &h_idx, session->dh_ctx.shared_k) != 0)
        return -1;

    // Calcular Exchange Hash (H)
    unsigned char exchange_hash[32];
    if (ssh_crypto_hash_sha256(exchange_hash, h_buffer, h_idx) != 0)
        return -1;

    for(int i = 0; i < 32; i++) {
        session->exchange_hash[i] = exchange_hash[i];
    }

    if (!session->session_id_initialized)
    {
        for(int i = 0; i < 32; i++) {
        session->session_id[i] = exchange_hash[i];
    }
        session->session_id_initialized = 1;
    }

    if (ssh_derive_outbound_keys(
            session->dh_ctx.shared_k,session->exchange_hash,session->session_id,
            session->outbound_key,session->outbound_iv,session->outbound_mac_key) != 0)
        return -1;

     /*if (ssh_derive_inbound_keys(
            session->dh_ctx.shared_k, session->exchange_hash, session->session_id,
            session->inbound_key, session->inbound_iv, session->inbound_mac_key) != 0)
        return -1;*/

    // Assinar o Hash H
    unsigned char signed_msg[128];
    unsigned long long signed_len = 0;
    if (crypto_sign(signed_msg, &signed_len, exchange_hash, 32, session->host_privkey) != 0)
        return -1;

    if (signed_len != 96)
        return -1;

    // Testar assinatura internamente antes de enviar
    unsigned char test_msg[128];
    unsigned long long test_len = 0;
    if (crypto_sign_open(test_msg, &test_len, signed_msg, signed_len, session->host_pubkey) != 0)
        return -1;

    if (test_len != 32)
        return -1;

    for (unsigned long i = 0; i < 32; i++) {
        if (test_msg[i] != exchange_hash[i])
            return -1;
    }

    unsigned long idx = 0;

    // Enviar o K_S (Chave Pública do Servidor)
    for (unsigned long i = 0; i < (4 + blob_len); i++)
        response[idx++] = h_buffer[blob_start + i];

    // Enviar o f (Chave Pública Curve25519 do Servidor)
    response[idx++] = 0; response[idx++] = 0; response[idx++] = 0;
    response[idx++] = 32;
    for (int i = 0; i < 32; i++)
        response[idx++] = session->dh_ctx.public_y[i];

    // Enviar o Signature Blob
    unsigned long sig_blob_len = 4 + host_key_fmt_len + 4 + 64;
    response[idx++] = (unsigned char)(sig_blob_len >> 24);
    response[idx++] = (unsigned char)(sig_blob_len >> 16);
    response[idx++] = (unsigned char)(sig_blob_len >> 8);
    response[idx++] = (unsigned char)sig_blob_len;

    response[idx++] = 0; response[idx++] = 0; response[idx++] = 0;
    response[idx++] = (unsigned char)host_key_fmt_len;
    for (unsigned long i = 0; i < host_key_fmt_len; i++)
        response[idx++] = (unsigned char)host_key_fmt[i];

    response[idx++] = 0; response[idx++] = 0; response[idx++] = 0;
    response[idx++] = 64;
    for (int i = 0; i < 64; i++)
        response[idx++] = signed_msg[i];

    if (idx > max_len) 
        return -1;

    *out_len = idx;
    return 0;
}

int ssh_process_kexinit(ssh_session_t* session, const unsigned char* payload, unsigned long payload_len)
{
    if (!session) return -1;
    if (session->state != SSH_STATE_KEX_INIT) return -2;

    if (payload_len + 1 > sizeof(session->client_kexinit))
        return -3;

    session->client_kexinit[0] = SSH_MSG_KEXINIT;

    for (unsigned long i = 0; i < payload_len; i++)
        session->client_kexinit[i + 1] = payload[i];

    session->client_kexinit_len = payload_len + 1;

    unsigned char kexinit_payload[512];
    unsigned long idx = 0;

    for (int i = 0; i < 16; i++)
    {
        kexinit_payload[idx++] = (unsigned char)i;
    }

    char *kex_algo = "curve25519-sha256";
    unsigned long kex_len = 17;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = (unsigned char)kex_len;
    for (unsigned long i = 0; i < kex_len; i++)
        kexinit_payload[idx++] = (unsigned char)kex_algo[i];

    char *host_key = "ssh-ed25519";
    unsigned long host_len = 11;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = (unsigned char)host_len;
    for (unsigned long i = 0; i < host_len; i++)
        kexinit_payload[idx++] = (unsigned char)host_key[i];

    char *cipher = "aes128-ctr";
    unsigned long cipher_len = 10;
    for(unsigned long i=0; i < cipher_len; i++) 
        session->inbound_cipher_name[i] = (unsigned char)cipher[i];
    session->inbound_cipher_name[cipher_len] = '\0';
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = (unsigned char)cipher_len;
    for (unsigned long i = 0; i < cipher_len; i++)
        kexinit_payload[idx++] = (unsigned char)cipher[i];
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = (unsigned char)cipher_len;
    for (unsigned long i = 0; i < cipher_len; i++)
        kexinit_payload[idx++] = (unsigned char)cipher[i];
    char *mac = "hmac-sha2-256";
    unsigned long mac_len = 13;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = (unsigned char)mac_len;
    for (unsigned long i = 0; i < mac_len; i++)
        kexinit_payload[idx++] = (unsigned char)mac[i];
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = (unsigned char)mac_len;
    for (unsigned long i = 0; i < mac_len; i++)
        kexinit_payload[idx++] = (unsigned char)mac[i];
    char *comp = "none";
    unsigned long comp_len = 4;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = (unsigned char)comp_len;
    for (unsigned long i = 0; i < comp_len; i++)
        kexinit_payload[idx++] = (unsigned char)comp[i];
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = (unsigned char)comp_len;
    for (unsigned long i = 0; i < comp_len; i++)
        kexinit_payload[idx++] = (unsigned char)comp[i];
    unsigned long lang_len = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = (unsigned char)lang_len;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = (unsigned char)lang_len;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;
    kexinit_payload[idx++] = 0;

    if (idx + 1 > sizeof(session->server_kexinit))
        return -4;

    session->server_kexinit[0] = SSH_MSG_KEXINIT;

    for (unsigned long i = 0; i < idx; i++)
        session->server_kexinit[i + 1] = kexinit_payload[i];

    session->server_kexinit_len = idx + 1;

    int ret = ssh_send_packet(session, SSH_MSG_KEXINIT, kexinit_payload, idx);
    if (ret <= 0)
        return -3;
    session->state = SSH_STATE_KEX_DH;
    return 0;
}
int ssh_process_kexdh_init(ssh_session_t *session, const unsigned char *payload, unsigned long payload_len)
{
    if (!session || !payload)
        return -1;
    if (session->state != SSH_STATE_KEX_DH)
        return -2;
    if (payload_len < 4)
        return -3;
    unsigned long client_key_len =
        ((unsigned long)payload[0] << 24) |
        ((unsigned long)payload[1] << 16) |
        ((unsigned long)payload[2] << 8) |
        (unsigned long)payload[3];
    if (client_key_len != 32 || payload_len < 4 + client_key_len)
    {
        return -4;
    }
    for (int i = 0; i < 32; i++)
    {
        session->dh_ctx.client_e[i] = payload[4 + i];
    }

    ssh_dh_generate_keys(&session->dh_ctx);
    ssh_dh_compute_shared_secret(&session->dh_ctx);
    unsigned char response[512];
    unsigned long response_len = 0;
    if (ssh_build_kexdh_reply(session, response, sizeof(response), &response_len) != 0)
    {
        return -5;
    }

    int ret = ssh_send_packet(session, SSH_MSG_KEXDH_REPLY, response, response_len);
    if (ret <= 0)
        return -6;

    unsigned char dummy_payload = 0;
    ret = ssh_send_packet(session, SSH_MSG_NEWKEYS, &dummy_payload, 0);
    if (ret <= 0)
        return -7;

    session->state = SSH_STATE_EXPECT_NEWKEYS;
    return 0;
}

int ssh_process_newkeys(ssh_session_t *session, const unsigned char *payload, unsigned long payload_len)
{
    (void)payload;
    (void)payload_len;

    if (!session)
        return -1;
    
    if (session->state != SSH_STATE_EXPECT_NEWKEYS)
        return -2;

    ssh_outbound_lock_acquire(session);
    
    ssh_crypto_enable_inbound_decryption(session);

    session->encrypted = 1;

    session->state = SSH_STATE_AUTH_STAGE;

    ssh_outbound_lock_release(session);

    return 0;
}


int ssh_process_service_request(ssh_session_t *session,const unsigned char *payload,unsigned long payload_len)
{
    unsigned long service_len;
    unsigned char response[16];
    unsigned long offset = 0;
    const unsigned char service_name[] = "ssh-userauth";

    if (!session)
        return -1;

    if (session->state != SSH_STATE_AUTH_STAGE)
        return -2;

    if (!payload || payload_len < 4)
        return -3;

    service_len = ((unsigned long)payload[0] << 24) |
                  ((unsigned long)payload[1] << 16) |
                  ((unsigned long)payload[2] << 8) |
                  (unsigned long)payload[3];

    if (service_len != 12 || payload_len != 4 + service_len)
        return -4;

    for (unsigned long i = 0; i < service_len; i++)
    {
        if (payload[4 + i] != service_name[i])
            return -5;
    }

    response[offset++] = 0;
    response[offset++] = 0;
    response[offset++] = 0;
    response[offset++] = (unsigned char)service_len;

    for (unsigned long i = 0; i < service_len; i++)
        response[offset++] = service_name[i];

    int ret = ssh_send_packet(session, SSH_MSG_SERVICE_ACCEPT, response, offset);
    if (ret <= 0)
        return -6;
    
    return 0;
}

int ssh_process_userauth_request(ssh_session_t *session,const unsigned char *payload,unsigned long payload_len)
{
    unsigned long offset = 0;
    unsigned long username_len;
    unsigned long service_len;
    unsigned long method_len;
    unsigned long password_len;
    unsigned char response[32];
    unsigned long response_len = 0;
    const unsigned char auth_methods[] = "password";
    unsigned long auth_methods_len = sizeof(auth_methods) - 1;
    const unsigned char valid_username[] = "nelson";
    const unsigned char valid_password[] = "1234";
    int authenticated = 0;

    if (!session)
        return -1;

    if (!payload || payload_len < 4)
        return -2;

    username_len = ((unsigned long)payload[offset] << 24) |
                   ((unsigned long)payload[offset + 1] << 16) |
                   ((unsigned long)payload[offset + 2] << 8) |
                   (unsigned long)payload[offset + 3];
    offset += 4;

    if (username_len > payload_len - offset)
        return -3;

    if (username_len == sizeof(valid_username) - 1 &&
        ssh_memcmp(&payload[offset], valid_username, username_len) == 0)
        authenticated = 1;
    else
        authenticated = 0;

    offset += username_len;

    if (payload_len - offset < 4)
        return -4;

    service_len = ((unsigned long)payload[offset] << 24) |
                  ((unsigned long)payload[offset + 1] << 16) |
                  ((unsigned long)payload[offset + 2] << 8) |
                  (unsigned long)payload[offset + 3];
    offset += 4;

    if (service_len > payload_len - offset)
        return -5;

    if (service_len != 14 ||
        ssh_memcmp(&payload[offset], "ssh-connection", 14) != 0)
        return -6;

    offset += service_len;

    if (payload_len - offset < 4)
        return -7;

    method_len = ((unsigned long)payload[offset] << 24) |
                 ((unsigned long)payload[offset + 1] << 16) |
                 ((unsigned long)payload[offset + 2] << 8) |
                 (unsigned long)payload[offset + 3];
    offset += 4;

    if (method_len > payload_len - offset)
        return -8;

    if (method_len == 6 && ssh_memcmp(&payload[offset], "publickey", 6) == 0)
        return -9;

    if (method_len != 8 || ssh_memcmp(&payload[offset], "password", 8) != 0)
        goto authentication_failure;

    offset += method_len;

    if (payload_len - offset < 1)
        return -10;

    if (payload[offset++] != 0)
        goto authentication_failure;

    if (payload_len - offset < 4)
        return -11;

    password_len = ((unsigned long)payload[offset] << 24) |
                   ((unsigned long)payload[offset + 1] << 16) |
                   ((unsigned long)payload[offset + 2] << 8) |
                   (unsigned long)payload[offset + 3];
    offset += 4;

    if (password_len != payload_len - offset)
        return -12;

    if (password_len == sizeof(valid_password) - 1 &&
        ssh_memcmp(&payload[offset], valid_password, password_len) == 0 &&
        authenticated)
    {
        unsigned char empty_payload[1] = {0};

        session->state = SSH_STATE_AUTH_STAGE;

        int ret = ssh_send_packet(session,SSH_MSG_USERAUTH_SUCCESS,empty_payload,0);
        if (ret <= 0)
            return -13;

        return 0;
    }

authentication_failure:
    response_len = 0;

    response[response_len++] = (unsigned char)((auth_methods_len >> 24) & 0xFF);
    response[response_len++] = (unsigned char)((auth_methods_len >> 16) & 0xFF);
    response[response_len++] = (unsigned char)((auth_methods_len >> 8) & 0xFF);
    response[response_len++] = (unsigned char)(auth_methods_len & 0xFF);

    ssh_memcpy(&response[response_len], auth_methods, auth_methods_len);
    response_len += auth_methods_len;
    response[response_len++] = 0;

    int ret = ssh_send_packet(session,
                              SSH_MSG_USERAUTH_FAILURE,
                              response,
                              response_len);

    if (ret <= 0)
        return -14;

    return 0;
}

int ssh_process_channel_refresh(ssh_session_t *session, const unsigned char *payload, unsigned long payload_len)
{
    unsigned long offset = 0;
    unsigned int recipient_channel;
    unsigned long request_type_len;
    unsigned char want_reply;
    
    unsigned char response_payload[4];
    int ret;

    if (!session)
        return -1;

    if (!payload || payload_len < 9)
        return -2;

    recipient_channel = ((unsigned int)payload[offset] << 24) |
                        ((unsigned int)payload[offset + 1] << 16) |
                        ((unsigned int)payload[offset + 2] << 8) |
                        (unsigned int)payload[offset + 3];
    offset += 4;

    request_type_len = ((unsigned long)payload[offset] << 24) |
                       ((unsigned long)payload[offset + 1] << 16) |
                       ((unsigned long)payload[offset + 2] << 8) |
                       (unsigned long)payload[offset + 3];
    offset += 4;

    if (request_type_len > payload_len - offset)
        return -3;

    const unsigned char *request_type = &payload[offset];
    offset += request_type_len;

    if (payload_len - offset < 1)
        return -4;

    want_reply = payload[offset++];

    if (request_type_len == 5 && ssh_memcmp(request_type, "shell", 5) == 0)
    {
        session->state = SSH_STATE_CONNECTED_SHELL;

        if (want_reply)
        {
            response_payload[0] = (unsigned char)(recipient_channel >> 24);
            response_payload[1] = (unsigned char)(recipient_channel >> 16);
            response_payload[2] = (unsigned char)(recipient_channel >> 8);
            response_payload[3] = (unsigned char)recipient_channel;

            ret = ssh_send_packet(session, SSH_MSG_CHANNEL_SUCCESS, response_payload, 4);
            if (ret <= 0)
                return -5;
        }
        return 0;
    }
    else if (request_type_len == 13 && ssh_memcmp(request_type, "window-change", 13) == 0)
    {
        
        if (payload_len - offset < 8)
            return -6;
       
        return 0;
    }
    
    if (want_reply)
    {
        response_payload[0] = (unsigned char)(recipient_channel >> 24);
        response_payload[1] = (unsigned char)(recipient_channel >> 16);
        response_payload[2] = (unsigned char)(recipient_channel >> 8);
        response_payload[3] = (unsigned char)recipient_channel;

        ret = ssh_send_packet(session, SSH_MSG_CHANNEL_FAILURE, response_payload, 4);
        if (ret <= 0)
            return -7;
    }

    return 0;
}

int ssh_process_channel_open(ssh_session_t *session,const unsigned char *payload,unsigned long payload_len)
{
    unsigned long offset = 0;
    unsigned long channel_type_len;
    unsigned int remote_channel;
    unsigned int remote_window;
    unsigned int remote_max_packet;
    unsigned int local_channel = 0;
    unsigned char response[16];
    unsigned long response_len = 0;

    if (!session)
        return -1;

    if (!payload || payload_len < 4)
        return -2;

    channel_type_len = ((unsigned long)payload[offset] << 24) |
                       ((unsigned long)payload[offset + 1] << 16) |
                       ((unsigned long)payload[offset + 2] << 8) |
                       (unsigned long)payload[offset + 3];
    offset += 4;

    if (channel_type_len > payload_len - offset)
        return -3;

    if (channel_type_len != 7 ||
        ssh_memcmp(&payload[offset], "session", 7) != 0)
        return -4;

    offset += channel_type_len;

    if (payload_len - offset < 12)
        return -5;

    remote_channel = ((unsigned int)payload[offset] << 24) |
                     ((unsigned int)payload[offset + 1] << 16) |
                     ((unsigned int)payload[offset + 2] << 8) |
                     (unsigned int)payload[offset + 3];
    offset += 4;

    session->client_channel_id = remote_channel;

    remote_window = ((unsigned int)payload[offset] << 24) |
                    ((unsigned int)payload[offset + 1] << 16) |
                    ((unsigned int)payload[offset + 2] << 8) |
                    (unsigned int)payload[offset + 3];
    offset += 4;

    remote_max_packet = ((unsigned int)payload[offset] << 24) |
                        ((unsigned int)payload[offset + 1] << 16) |
                        ((unsigned int)payload[offset + 2] << 8) |
                        (unsigned int)payload[offset + 3];
    offset += 4;

    if (offset != payload_len)
        return -6;

    if (remote_max_packet == 0)
        return -7;

    response[response_len++] = (unsigned char)((remote_channel >> 24) & 0xFF);
    response[response_len++] = (unsigned char)((remote_channel >> 16) & 0xFF);
    response[response_len++] = (unsigned char)((remote_channel >> 8) & 0xFF);
    response[response_len++] = (unsigned char)(remote_channel & 0xFF);

    response[response_len++] = (unsigned char)((local_channel >> 24) & 0xFF);
    response[response_len++] = (unsigned char)((local_channel >> 16) & 0xFF);
    response[response_len++] = (unsigned char)((local_channel >> 8) & 0xFF);
    response[response_len++] = (unsigned char)(local_channel & 0xFF);

    response[response_len++] = (unsigned char)((SSH_CHANNEL_WINDOW_SIZE >> 24) & 0xFF);
    response[response_len++] = (unsigned char)((SSH_CHANNEL_WINDOW_SIZE >> 16) & 0xFF);
    response[response_len++] = (unsigned char)((SSH_CHANNEL_WINDOW_SIZE >> 8) & 0xFF);
    response[response_len++] = (unsigned char)(SSH_CHANNEL_WINDOW_SIZE & 0xFF);

    response[response_len++] = (unsigned char)((SSH_CHANNEL_MAX_PACKET_SIZE >> 24) & 0xFF);
    response[response_len++] = (unsigned char)((SSH_CHANNEL_MAX_PACKET_SIZE >> 16) & 0xFF);
    response[response_len++] = (unsigned char)((SSH_CHANNEL_MAX_PACKET_SIZE >> 8) & 0xFF);
    response[response_len++] = (unsigned char)(SSH_CHANNEL_MAX_PACKET_SIZE & 0xFF);

    int ret = ssh_send_packet(session,SSH_MSG_CHANNEL_OPEN_CONFIRMATION,response,response_len);
    if (ret <= 0)
        return -8;

    return 0;

    (void)remote_window;
}

int ssh_process_channel_request(ssh_session_t *session,const unsigned char *payload,unsigned long payload_len)
{
    unsigned long offset = 0;
    unsigned long request_len;
    unsigned int recipient_channel;
    unsigned char want_reply;
    unsigned char response[4];
    const unsigned char *request_name;
    unsigned long request_data_len;
    int accepted = 0;

    if (!session)
        return -1;

    if (!payload || payload_len < 9)
        return -2;

    recipient_channel = ((unsigned int)payload[offset] << 24) |
                        ((unsigned int)payload[offset + 1] << 16) |
                        ((unsigned int)payload[offset + 2] << 8) |
                        (unsigned int)payload[offset + 3];
    offset += 4;

    request_len = ((unsigned long)payload[offset] << 24) |
                  ((unsigned long)payload[offset + 1] << 16) |
                  ((unsigned long)payload[offset + 2] << 8) |
                  (unsigned long)payload[offset + 3];
    offset += 4;

    if (request_len > payload_len - offset ||
        payload_len - offset - request_len < 1)
        return -3;

    request_name = &payload[offset];
    offset += request_len;

    want_reply = payload[offset++];
    request_data_len = payload_len - offset;

    if (request_len == 7 &&
        ssh_memcmp(request_name, "pty-req", 7) == 0)
    {
        accepted = 1;
        //printf("PTY request received\n");
    }
    else if (request_len == 5 &&
             ssh_memcmp(request_name, "shell", 5) == 0)
    {
        if (request_data_len == 0 &&
            session->state != SSH_STATE_CONNECTED_SHELL)
        {
            session->state = SSH_STATE_CONNECTED_SHELL;
            accepted = 1;
            //printf("Shell request received\n");
        }
    }

    if (!want_reply)
        return accepted ? 0 : 0;

    response[0] = (unsigned char)((recipient_channel >> 24) & 0xFF);
    response[1] = (unsigned char)((recipient_channel >> 16) & 0xFF);
    response[2] = (unsigned char)((recipient_channel >> 8) & 0xFF);
    response[3] = (unsigned char)(recipient_channel & 0xFF);

    int ret = ssh_send_packet(session,
                              accepted ? SSH_MSG_CHANNEL_SUCCESS :
                                         SSH_MSG_CHANNEL_FAILURE,
                              response,
                              sizeof(response));

    if (ret <= 0)
        return -4;

    return 0;
}

int ssh_process_packet(ssh_session_t *session)
{
    if (!session || !session->read_hardware)
        return -1;

    ssh_inbound_lock_acquire(session);

    unsigned long header_bytes = 0;
    unsigned long block_size = 4;

    while (header_bytes < block_size)
    {
        long res = session->read_hardware(
            session->client_fd,
            &session->rx_buffer[header_bytes],
            block_size - header_bytes
        );

        if (res <= 0 || (unsigned long)res > block_size - header_bytes) {
            ssh_inbound_lock_release(session);
            return -2;
        }

        header_bytes += (unsigned long)res;
    }

    unsigned int packet_length = 0;

    if (session->encrypted)
    {
        unsigned char expanded_key[176];
        unsigned char keystream[16];
        unsigned char decrypted_header[4];

        aes_key_expansion(session->inbound_key, expanded_key);
        aes_encrypt_block(session->inbound_iv, keystream, expanded_key);

        for (int i = 0; i < 4; i++)
            decrypted_header[i] = session->rx_buffer[i] ^ keystream[i];

        packet_length =
            ((unsigned int)decrypted_header[0] << 24) |
            ((unsigned int)decrypted_header[1] << 16) |
            ((unsigned int)decrypted_header[2] << 8) |
            (unsigned int)decrypted_header[3];
    }
    else
    {
        packet_length =
            ((unsigned int)session->rx_buffer[0] << 24) |
            ((unsigned int)session->rx_buffer[1] << 16) |
            ((unsigned int)session->rx_buffer[2] << 8) |
            (unsigned int)session->rx_buffer[3];
    }

    if (packet_length < 6 || packet_length > SSH_MAX_PACKET_LEN - 4)
    {
        ssh_inbound_lock_release(session);
        return -4;
    }

    unsigned long total_required = (unsigned long)packet_length + 4;

    if (total_required > sizeof(session->rx_buffer)) {
        ssh_inbound_lock_release(session);
        return -4;
    }

    if (session->encrypted && (total_required % 16 != 0))
    {
        ssh_inbound_lock_release(session);
        return -4;
    }

    unsigned long packet_bytes = 4;

    while (packet_bytes < total_required)
    {
        long res = session->read_hardware(
            session->client_fd,
            &session->rx_buffer[packet_bytes],
            total_required - packet_bytes
        );

        if (res <= 0 || (unsigned long)res > total_required - packet_bytes) {
            ssh_inbound_lock_release(session);
            return -5;
        }

        packet_bytes += (unsigned long)res;
    }

    if (session->encrypted)
    {
        ssh_crypto_decrypt_inbound_block(
            session,
            session->rx_buffer,
            total_required
        );

        unsigned char remote_mac[32];
        unsigned long mac_bytes = 0;

        while (mac_bytes < sizeof(remote_mac))
        {
            long res = session->read_hardware(
                session->client_fd,
                &remote_mac[mac_bytes],
                sizeof(remote_mac) - mac_bytes
            );

            if (res <= 0 || (unsigned long)res > sizeof(remote_mac) - mac_bytes) {
                ssh_inbound_lock_release(session);
                return -7;
            }

            mac_bytes += (unsigned long)res;
        }

        unsigned char computed_mac[32];
        ssh_hmac_sha256_packet(
            session->inbound_mac_key,
            session->inbound_sequence,
            session->rx_buffer,
            total_required,
            computed_mac
        );

        if (ssh_memcmp(remote_mac, computed_mac, 32) != 0) {
            ssh_inbound_lock_release(session);
            return -8;
        }
    }

    session->inbound_sequence++;

    unsigned char padding_length = session->rx_buffer[4];
    unsigned char msg_type = session->rx_buffer[5];

    if (padding_length < SSH_MIN_PAD_LEN || (unsigned long)padding_length + 2 > packet_length) {
        ssh_inbound_lock_release(session);
        return -6;
    }

    unsigned long payload_len = (unsigned long)packet_length - padding_length - 2;

    if (msg_type == SSH_MSG_DISCONNECT)
    {
        session->state = SSH_STATE_DISCONNECTED;
        ssh_inbound_lock_release(session);
        return 0;
    }

    int ret = 0;

    if (msg_type == SSH_MSG_KEXINIT)
        ret = ssh_process_kexinit(session, &session->rx_buffer[6], payload_len);
    else if (msg_type == SSH_MSG_KEXDH_INIT)
        ret = ssh_process_kexdh_init(session, &session->rx_buffer[6], payload_len);
    else if (msg_type == SSH_MSG_NEWKEYS)
        ret = ssh_process_newkeys(session, &session->rx_buffer[6], payload_len);
    else if (msg_type == SSH_MSG_SERVICE_REQUEST)
        ret = ssh_process_service_request(session, &session->rx_buffer[6], payload_len);
    else if (msg_type == SSH_MSG_USERAUTH_REQUEST)
        ret = ssh_process_userauth_request(session, &session->rx_buffer[6], payload_len);
    else if (msg_type == SSH_MSG_CHANNEL_OPEN)
        ret = ssh_process_channel_open(session, &session->rx_buffer[6], payload_len);
    else if (msg_type == SSH_MSG_CHANNEL_REQUEST)
        ret = ssh_process_channel_refresh(session, &session->rx_buffer[6], payload_len);
    else if (msg_type == SSH_MSG_CHANNEL_DATA)
        ret = 0;

    ssh_inbound_lock_release(session);
    return ret;
}


int ssh_do_banner_exchange(ssh_session_t *session)
{
    if (!session || !session->write_hardware || !session->read_hardware)
        return -1;
    long res = session->write_hardware(session->client_fd, (const unsigned char *)SSH_BANNER_SERVER, 23);
    if (res <= 0)
        return -2;
    unsigned long idx = 0;
    while (idx < 255)
    {
        char ch;
        res = session->read_hardware(session->client_fd, (unsigned char *)&ch, 1);
        if (res <= 0)
            return -3;
        session->client_banner[idx++] = ch;
        if (ch == '\n')
            break;
    }
    session->client_banner[idx] = '\0';
    session->client_banner_len = idx;
    char *ver = session->client_banner;
    while (*ver && *ver != 'S')
        ver++;
    if (*ver == 'S' && *(ver + 1) == 'S' && *(ver + 2) == 'H' && *(ver + 3) == '-' && *(ver + 4) == '2' && *(ver + 5) == '.' && *(ver + 6) == '0')
    {
        session->state = SSH_STATE_KEX_INIT;
        return 0;
    }
    return -4;
}

int ssh_channel_poll(ssh_session_t *session)
{
    if (!session || !session->read_hardware || session->state != SSH_STATE_CONNECTED_SHELL)
        return 0;

    return 1;
}

int ssh_channel_read(ssh_session_t *session, void *buffer, unsigned long max_len, int timeout)
{
    (void)timeout;
    if (!session || !buffer || max_len == 0)
        return -1;

    if (ssh_process_packet(session) != 0) {
        return -1; 
    }

    unsigned char msg_type = session->rx_buffer[5];
    if (msg_type != SSH_MSG_CHANNEL_DATA) {
        return 0;
    }

    unsigned long data_len = ((unsigned long)session->rx_buffer[10] << 24) |
                             ((unsigned long)session->rx_buffer[11] << 16) |
                             ((unsigned long)session->rx_buffer[12] << 8)  |
                             (unsigned long)session->rx_buffer[13];

    if (data_len == 0) {
        return 0;
    }

    unsigned long bytes_a_copiar = (data_len < max_len) ? data_len : max_len;
    unsigned char *out = (unsigned char *)buffer;
    for (unsigned long i = 0; i < bytes_a_copiar; i++) {
        out[i] = session->rx_buffer[14 + i];
    }
    return (int)bytes_a_copiar;
}

int ssh_channel_write(ssh_session_t *session, const void *buffer, unsigned long len)
{
    if (!session || !buffer || len == 0)
        return -1;
    unsigned char payload[4096 + 8];
    ssh_memset(payload, 0, sizeof(payload));
    unsigned long p_idx = 0;

    payload[p_idx++] = (unsigned char)(session->client_channel_id >> 24);
    payload[p_idx++] = (unsigned char)(session->client_channel_id >> 16);
    payload[p_idx++] = (unsigned char)(session->client_channel_id >> 8);
    payload[p_idx++] = (unsigned char)session->client_channel_id;

    payload[p_idx++] = (unsigned char)(len >> 24);
    payload[p_idx++] = (unsigned char)(len >> 16);
    payload[p_idx++] = (unsigned char)(len >> 8);
    payload[p_idx++] = (unsigned char)len;

    if (p_idx + len > sizeof(payload)) {
        return -1; 
    }

    const unsigned char *src = (const unsigned char *)buffer;
    for (unsigned long i = 0; i < len; i++) {
        payload[p_idx++] = src[i];
    }
    
    int result = ssh_send_packet(session, SSH_MSG_CHANNEL_DATA, payload, p_idx);

    if (result < 0) {
        return -1;
    }

    return (int)len; 
}
