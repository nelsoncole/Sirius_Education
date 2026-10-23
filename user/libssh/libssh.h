#ifndef LIBSSH_H
#define LIBSSH_H

#define SSH_BANNER_SERVER      "SSH-2.0-SiriusSSH_0.1\r\n"
#define SSH_BANNER_CLIENT_MAX  256
#define SSH_MAX_PACKET_LEN     35000
#define SSH_MIN_PAD_LEN        4

#define SSH_CHANNEL_WINDOW_SIZE     65536
#define SSH_CHANNEL_MAX_PACKET_SIZE 32768

#define SSH_MSG_DISCONNECT                  1
#define SSH_MSG_KEXINIT                     20
#define SSH_MSG_NEWKEYS                     21
#define SSH_MSG_KEXDH_INIT                  30
#define SSH_MSG_KEXDH_REPLY                 31
#define SSH_MSG_SERVICE_REQUEST             5
#define SSH_MSG_SERVICE_ACCEPT              6
#define SSH_MSG_USERAUTH_REQUEST            50
#define SSH_MSG_USERAUTH_FAILURE            51
#define SSH_MSG_USERAUTH_SUCCESS            52
#define SSH_MSG_CHANNEL_OPEN                90
#define SSH_MSG_CHANNEL_OPEN_CONFIRMATION   91
#define SSH_MSG_CHANNEL_DATA                94
#define SSH_MSG_CHANNEL_REQUEST             98
#define SSH_MSG_CHANNEL_SUCCESS             99
#define SSH_MSG_CHANNEL_FAILURE             100


typedef struct {
    unsigned char secret_x[32];
    unsigned char public_y[32];
    unsigned char client_e[32];
    unsigned char shared_k[32];
} ssh_dh_ctx_t;

typedef enum {
    SSH_STATE_DISCONNECTED,
    SSH_STATE_EXCHANGE_BANNERS,
    SSH_STATE_KEX_INIT,
    SSH_STATE_KEX_DH,
    SSH_STATE_EXPECT_NEWKEYS,
    SSH_STATE_AUTH_STAGE,
    SSH_STATE_CONNECTED_SHELL
} ssh_state_t;

typedef long (*ssh_write_fn)(int fd, const void *buf, unsigned long len);
typedef long (*ssh_read_fn)(int fd, void *buf, unsigned long len);

typedef struct {
    int client_fd;
    ssh_read_fn read_hardware;
    ssh_write_fn write_hardware;
    ssh_state_t state;
    int encrypted;

    char client_banner[SSH_BANNER_CLIENT_MAX];
    unsigned long client_banner_len;

    unsigned char rx_buffer[SSH_MAX_PACKET_LEN];
    unsigned char tx_buffer[SSH_MAX_PACKET_LEN];

    unsigned char client_kexinit[4096];
    unsigned long client_kexinit_len;

    unsigned char server_kexinit[4096];
    unsigned long server_kexinit_len;

    unsigned char host_privkey[64];
    unsigned char host_pubkey[32];

    ssh_dh_ctx_t dh_ctx;

    unsigned char session_id[32];
    unsigned char session_id_initialized;
    unsigned char exchange_hash[32];

    unsigned char inbound_key[16];
    unsigned char inbound_iv[16];
    unsigned char inbound_mac_key[32];

    unsigned char outbound_key[16];
    unsigned char outbound_iv[16];
    unsigned char outbound_mac_key[32];

    unsigned char inbound_cipher_name[256];

    unsigned int inbound_sequence;
    unsigned int outbound_sequence;

    unsigned int client_channel_id;

    volatile int inbound_lock;
    volatile int outbound_lock;
    volatile int user_lock;

}ssh_session_t;

void ssh_lock_acquire(ssh_session_t* session);
void ssh_lock_release(ssh_session_t* session);
void ssh_init_session(ssh_session_t* session, int fd, ssh_read_fn r_fn, ssh_write_fn w_fn);
int ssh_do_banner_exchange(ssh_session_t* session);
int ssh_process_packet(ssh_session_t* session);
int ssh_send_packet(ssh_session_t* session, unsigned char msg_type, const unsigned char* payload, unsigned long payload_len);
void ssh_dh_generate_keys(ssh_dh_ctx_t* ctx);
void ssh_dh_compute_shared_secret(ssh_dh_ctx_t* ctx);
int ssh_process_kexinit(ssh_session_t* session, const unsigned char* payload, unsigned long payload_len);
int ssh_build_kexdh_reply(ssh_session_t* session, unsigned char* response, unsigned long max_len, unsigned long* out_len);
int ssh_process_kexdh_init(ssh_session_t* session, const unsigned char* payload, unsigned long payload_len);
int ssh_process_newkeys(ssh_session_t *session, const unsigned char *payload, unsigned long payload_len);
int ssh_process_service_request(ssh_session_t *session, const unsigned char *payload, unsigned long payload_len);
int ssh_process_userauth_request(ssh_session_t *session,const unsigned char *payload,unsigned long payload_len);
int ssh_process_channel_open(ssh_session_t *session,const unsigned char *payload,unsigned long payload_len);
int ssh_process_channel_request(ssh_session_t *session,const unsigned char *payload,unsigned long payload_len);
int ssh_do_banner_exchange(ssh_session_t* session);
int ssh_channel_poll(ssh_session_t *session);
int ssh_channel_read(ssh_session_t *session, void *buffer, unsigned long max_len, int timeout);
int ssh_channel_write(ssh_session_t *session, const void *buffer, unsigned long len);
void ssh_crypto_enable_inbound_decryption(ssh_session_t *session);
void ssh_crypto_decrypt_inbound_block(ssh_session_t *session, unsigned char *data, unsigned long len);


int ssh_memcmp(const void *ptr1, const void *ptr2, unsigned long len);
void *ssh_memcpy(void *dest, const void *src, unsigned long len);
void* ssh_memset(void *s, int val, unsigned long count);
int randombytes(unsigned char *buf, unsigned long long len);
int ssh_crypto_hash_sha256(unsigned char *out, const unsigned char *m, unsigned long long n);
int ssh_append_curve25519_mpint(unsigned char *buffer,unsigned long capacity,unsigned long *index,const unsigned char shared_k[32]);
void aes128_ctr_crypt(const unsigned char *key, unsigned char *iv, const unsigned char *src, unsigned char *dst, unsigned long len);
void aes_key_expansion(const unsigned char *key, unsigned char *w);
void aes_encrypt_block(const unsigned char *in, unsigned char *out, const unsigned char *w);
int ssh_derive_outbound_keys(const unsigned char *shared_k,const unsigned char *exchange_hash,const unsigned char *session_id,unsigned char outbound_key[16],unsigned char outbound_iv[16],unsigned char outbound_mac_key[32]);
void ssh_hmac_sha256_packet(const unsigned char key[32],unsigned int sequence,const unsigned char *packet,unsigned long packet_len,unsigned char mac[32]);
extern int printf(const char *c, ...);


#endif