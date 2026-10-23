#include "libssh.h"

int ssh_memcmp(const void *ptr1, const void *ptr2, unsigned long len)
{
    const unsigned char *a = (const unsigned char *)ptr1;
    const unsigned char *b = (const unsigned char *)ptr2;

    if (!a || !b)
        return (a == b) ? 0 : (a ? 1 : -1);

    for (unsigned long i = 0; i < len; i++)
    {
        if (a[i] != b[i])
            return (int)a[i] - (int)b[i];
    }

    return 0;
}

void *ssh_memcpy(void *dest, const void *src, unsigned long len)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (!d || !s)
        return dest;

    for (unsigned long i = 0; i < len; i++)
        d[i] = s[i];

    return dest;
}

void* ssh_memset(void *s, int val, unsigned long count)
{
    unsigned long _count = count;
    unsigned char *tmp = (unsigned char *)s;
    for (; _count != 0; _count--)
        *tmp++ = val;
    return s;
}

int randombytes(unsigned char *buf, unsigned long long len)
{
    if (buf == 0) return -1;

    unsigned long long i = 0;

    while (i < len)
    {
        unsigned int rand_val;
        unsigned char ok;
        int retries = 0;

        do
        {
            __asm__ __volatile__(
                "rdrand %k0\n\t"
                "setc %1"
                : "=&r"(rand_val), "=q"(ok)
                :
                : "cc"
            );

            retries++;
            if (retries > 10) 
            {
                return -2; 
            }

        } while (!ok);

        for (unsigned int j = 0;
             j < sizeof(rand_val) && i < len;
             j++, i++)
        {
            buf[i] = (unsigned char)(rand_val >> (j * 8));
        }
    }

    return 0;
}


typedef struct
{
    unsigned int state[8];
    unsigned long long count;
    unsigned char buffer[64];
} ssh_sha256_ctx;

static void ssh_sha256_transform(unsigned int *state, const unsigned char *buffer)
{

    unsigned int a = state[0], b = state[1], c = state[2], d = state[3];

    unsigned int e = state[4], f = state[5], g = state[6], h = state[7];

    unsigned int w[64];

    for (int i = 0; i < 16; i++)
    {

        w[i] = ((unsigned int)buffer[i * 4] << 24) |

               ((unsigned int)buffer[i * 4 + 1] << 16) |

               ((unsigned int)buffer[i * 4 + 2] << 8) |

               (unsigned int)buffer[i * 4 + 3];
    }

    static const unsigned int k[64] = {

        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,

        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,

        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,

        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,

        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,

        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,

        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,

        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2

    };

    for (int i = 16; i < 64; i++)
    {

        unsigned int s0 = (((w[i - 15] >> 7) | (w[i - 15] << 25)) ^ ((w[i - 15] >> 18) | (w[i - 15] << 14)) ^ (w[i - 15] >> 3));

        unsigned int s1 = (((w[i - 2] >> 17) | (w[i - 2] << 15)) ^ ((w[i - 2] >> 19) | (w[i - 2] << 13)) ^ (w[i - 2] >> 10));

        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    for (int i = 0; i < 64; i++)
    {

        unsigned int s1_e = (((e >> 6) | (e << 26)) ^ ((e >> 11) | (e << 21)) ^ ((e >> 25) | (e << 7)));

        unsigned int ch = (e & f) ^ (~e & g);

        unsigned int temp1 = h + s1_e + ch + k[i] + w[i];

        unsigned int s0_a = (((a >> 2) | (a << 30)) ^ ((a >> 13) | (a << 19)) ^ ((a >> 22) | (a << 10)));

        unsigned int maj = (a & b) ^ (a & c) ^ (b & c);

        unsigned int temp2 = s0_a + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;

        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

static void ssh_sha256_init(ssh_sha256_ctx *context)
{
    context->state[0] = 0x6a09e667;
    context->state[1] = 0xbb67ae85;

    context->state[2] = 0x3c6ef372;
    context->state[3] = 0xa54ff53a;

    context->state[4] = 0x510e527f;
    context->state[5] = 0x9b05688c;

    context->state[6] = 0x1f83d9ab;
    context->state[7] = 0x5be0cd19;

    context->count = 0;
}

static void ssh_sha256_update(ssh_sha256_ctx *context, const unsigned char *data, unsigned long len)
{

    unsigned long i, j;

    j = (unsigned long)((context->count >> 3) & 63);

    context->count += (unsigned long long)len << 3;

    if ((j + len) > 63)
    {

        for (i = 0; i < (64 - j); i++)
            context->buffer[j + i] = data[i];

        ssh_sha256_transform(context->state, context->buffer);

        for (; i + 63 < len; i += 64)
            ssh_sha256_transform(context->state, &data[i]);

        j = 0;
    }
    else
    {

        i = 0;
    }

    for (; i < len; i++)
        context->buffer[j++] = data[i];
}

static void ssh_sha256_final(unsigned char *digest, ssh_sha256_ctx *context)
{
    unsigned char finalcount[8];

    for (int i = 0; i < 8; i++)
    {

        finalcount[i] = (unsigned char)((context->count >> ((7 - i) * 8)) & 0xFF);
    }

    unsigned char c = 0x80;

    ssh_sha256_update(context, &c, 1);

    while ((unsigned long)((context->count >> 3) & 63) != 56)
    {

        c = 0x00;

        ssh_sha256_update(context, &c, 1);
    }

    ssh_sha256_update(context, finalcount, 8);

    for (int i = 0; i < 32; i++)
    {

        digest[i] = (unsigned char)((context->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 0xFF);
    }
}

int ssh_crypto_hash_sha256(unsigned char *out, const unsigned char *m, unsigned long long n)
{
    ssh_sha256_ctx ctx;

    ssh_sha256_init(&ctx);

    ssh_sha256_update(&ctx, m, (unsigned long)n);

    ssh_sha256_final(out, &ctx);

    return 0;
}

int ssh_append_curve25519_mpint(unsigned char *buffer, unsigned long capacity, unsigned long *index, const unsigned char shared_k[32])
{
    if (!buffer || !index || !shared_k)
        return -1;

    if (*index > capacity || capacity - *index < 4)
        return -1;

    unsigned char value[33];
    unsigned long start = 1;
    unsigned long length;

    value[0] = 0;

    for (unsigned long i = 0; i < 32; i++)
        value[i + 1] = shared_k[i];

    while (start < 33 && value[start] == 0)
        start++;

    if (start == 33)
    {
        buffer[(*index)++] = 0;
        buffer[(*index)++] = 0;
        buffer[(*index)++] = 0;
        buffer[(*index)++] = 0;
        return 0;
    }

    length = 33 - start;

    if (value[start] & 0x80)
    {
        start--;
        value[start] = 0;
        length++;
    }

    if (*index > capacity || capacity - *index < 4 + length)
        return -1;

    buffer[(*index)++] = (unsigned char)(length >> 24);
    buffer[(*index)++] = (unsigned char)(length >> 16);
    buffer[(*index)++] = (unsigned char)(length >> 8);
    buffer[(*index)++] = (unsigned char)length;

    for (unsigned long i = 0; i < length; i++)
        buffer[(*index)++] = value[start + i];

    return 0;
}

void ssh_hmac_sha256_packet(const unsigned char key[32],unsigned int sequence,const unsigned char *packet,unsigned long packet_len,unsigned char mac[32])
{
    unsigned char ipad[64];
    unsigned char opad[64];
    unsigned char inner_hash[32];
    unsigned char seq[4];

    for (int i = 0; i < 64; i++)
    {
        unsigned char k = i < 32 ? key[i] : 0;
        ipad[i] = k ^ 0x36;
        opad[i] = k ^ 0x5c;
    }

    seq[0] = (unsigned char)(sequence >> 24);
    seq[1] = (unsigned char)(sequence >> 16);
    seq[2] = (unsigned char)(sequence >> 8);
    seq[3] = (unsigned char)sequence;

    ssh_sha256_ctx ctx;

    ssh_sha256_init(&ctx);
    ssh_sha256_update(&ctx, ipad, 64);
    ssh_sha256_update(&ctx, seq, 4);
    ssh_sha256_update(&ctx, packet, packet_len);
    ssh_sha256_final(inner_hash, &ctx);

    ssh_sha256_init(&ctx);
    ssh_sha256_update(&ctx, opad, 64);
    ssh_sha256_update(&ctx, inner_hash, 32);
    ssh_sha256_final(mac, &ctx);
}

void ssh_crypto_enable_inbound_decryption(ssh_session_t *session)
{
    if (!session)
        return;

    unsigned char k_mpint[40];
    unsigned long k_mpint_idx = 0;

    if (ssh_append_curve25519_mpint(k_mpint, sizeof(k_mpint), &k_mpint_idx, session->dh_ctx.shared_k) != 0)
        return;

    unsigned char iv_buffer[128];
    unsigned long iv_idx = 0;

    for (unsigned long i = 0; i < k_mpint_idx; i++)
        iv_buffer[iv_idx++] = k_mpint[i];

    for (int i = 0; i < 32; i++)
        iv_buffer[iv_idx++] = session->exchange_hash[i];

    iv_buffer[iv_idx++] = 'A';

    for (int i = 0; i < 32; i++)
        iv_buffer[iv_idx++] = session->session_id[i];

    unsigned char iv_c2s[32];
    ssh_crypto_hash_sha256(iv_c2s, iv_buffer, iv_idx);

    unsigned char key_buffer[128];
    unsigned long key_idx = 0;

    for (unsigned long i = 0; i < k_mpint_idx; i++)
        key_buffer[key_idx++] = k_mpint[i];

    for (int i = 0; i < 32; i++)
        key_buffer[key_idx++] = session->exchange_hash[i];

    key_buffer[key_idx++] = 'C';

    for (int i = 0; i < 32; i++)
        key_buffer[key_idx++] = session->session_id[i];

    unsigned char key_c2s[32];
    ssh_crypto_hash_sha256(key_c2s, key_buffer, key_idx);

    unsigned char mac_buffer[128];
    unsigned long mac_idx = 0;

    for (unsigned long i = 0; i < k_mpint_idx; i++)
        mac_buffer[mac_idx++] = k_mpint[i];

    for (int i = 0; i < 32; i++)
        mac_buffer[mac_idx++] = session->exchange_hash[i];

    mac_buffer[mac_idx++] = 'E';

    for (int i = 0; i < 32; i++)
        mac_buffer[mac_idx++] = session->session_id[i];

    unsigned char mac_c2s[32];
    ssh_crypto_hash_sha256(mac_c2s, mac_buffer, mac_idx);

    for (int i = 0; i < 16; i++)
    {
        session->inbound_iv[i] = iv_c2s[i];
        session->inbound_key[i] = key_c2s[i];
    }

    for (int i = 0; i < 32; i++)
    {
        session->inbound_mac_key[i] = mac_c2s[i];
    }
}

// TABELAS E MÉTODOS COMPLETOS DO AES-128-CTR
static const unsigned char sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

void aes_key_expansion(const unsigned char *key, unsigned char *w)
{
    unsigned char temp[4];
    unsigned int rcon = 0x01;

    for (int i = 0; i < 16; i++)
        w[i] = key[i];

    for (int i = 16; i < 176; i += 4)
    {
        for (int j = 0; j < 4; j++)
            temp[j] = w[i - 4 + j];

        if (i % 16 == 0)
        {
            unsigned char k = temp[0];
            temp[0] = sbox[temp[1]];
            temp[1] = sbox[temp[2]];
            temp[2] = sbox[temp[3]];
            temp[3] = sbox[k];

            temp[0] ^= rcon;
            rcon = (rcon << 1) ^ ((rcon & 0x80) ? 0x11b : 0x00);
        }

        for (int j = 0; j < 4; j++)
            w[i + j] = w[i - 16 + j] ^ temp[j];
    }
}

void aes_encrypt_block(const unsigned char *in, unsigned char *out, const unsigned char *w)
{
    unsigned char state[16];

    for (int i = 0; i < 16; i++)
        state[i] = in[i] ^ w[i];

    for (int round = 1; round <= 10; round++)
    {
        unsigned char t[16];

        for (int i = 0; i < 16; i++)
            t[i] = sbox[state[i]];

        state[0] = t[0];
        state[1] = t[5];
        state[2] = t[10];
        state[3] = t[15];

        state[4] = t[4];
        state[5] = t[9];
        state[6] = t[14];
        state[7] = t[3];

        state[8] = t[8];
        state[9] = t[13];
        state[10] = t[2];
        state[11] = t[7];

        state[12] = t[12];
        state[13] = t[1];
        state[14] = t[6];
        state[15] = t[11];

        if (round < 10)
        {
            for (int i = 0; i < 16; i += 4)
            {
                unsigned char a = state[i];
                unsigned char b = state[i + 1];
                unsigned char c = state[i + 2];
                unsigned char d = state[i + 3];

                unsigned char a2 = (unsigned char)((a << 1) ^ ((a & 0x80) ? 0x1B : 0));
                unsigned char b2 = (unsigned char)((b << 1) ^ ((b & 0x80) ? 0x1B : 0));
                unsigned char c2 = (unsigned char)((c << 1) ^ ((c & 0x80) ? 0x1B : 0));
                unsigned char d2 = (unsigned char)((d << 1) ^ ((d & 0x80) ? 0x1B : 0));

                state[i]     = a2 ^ b2 ^ b ^ c ^ d;
                state[i + 1] = a ^ b2 ^ c2 ^ c ^ d;
                state[i + 2] = a ^ b ^ c2 ^ d2 ^ d;
                state[i + 3] = a2 ^ a ^ b ^ c ^ d2;
            }
        }

        int r_offset = round * 16;
        for (int i = 0; i < 16; i++)
            state[i] ^= w[r_offset + i];
    }

    for (int i = 0; i < 16; i++)
        out[i] = state[i];
}

void aes128_ctr_crypt(const unsigned char *key, unsigned char *iv, const unsigned char *src, unsigned char *dst, unsigned long len)
{
    unsigned char expanded_key[176];
    unsigned char keystream[16];

    aes_key_expansion(key, expanded_key);

    unsigned long pos = 0;

    while (pos < len)
    {
        aes_encrypt_block(iv, keystream, expanded_key);

        for (int i = 15; i >= 0; i--)
        {
            iv[i]++;
            if (iv[i] != 0)
                break;
        }

        for (unsigned long i = 0; i < 16 && pos < len; i++, pos++)
        {
            dst[pos] = src[pos] ^ keystream[i];
        }
    }

    ssh_memset(expanded_key, 0, sizeof(expanded_key));
    ssh_memset(keystream, 0, sizeof(keystream));
}

void ssh_crypto_decrypt_inbound_block(ssh_session_t *session, unsigned char *data, unsigned long len)
{
    if (!session || !data || len == 0)
        return;

    aes128_ctr_crypt(session->inbound_key, session->inbound_iv, data, data, len);
}

int ssh_derive_outbound_keys(const unsigned char *shared_k, const unsigned char *exchange_hash, const unsigned char *session_id, unsigned char outbound_key[16], unsigned char outbound_iv[16], unsigned char outbound_mac_key[32])
{
    if (!shared_k || !exchange_hash || !session_id || !outbound_key || !outbound_iv || !outbound_mac_key)
        return -1;

    unsigned char k_encoded[64];
    unsigned long k_len = 0;

    if (ssh_append_curve25519_mpint(k_encoded, sizeof(k_encoded), &k_len, shared_k) != 0)
        return -1;

    unsigned char input[128];
    unsigned char digest[32];
    unsigned long idx;

    const unsigned char letters[3] = { 'D', 'B', 'F' };
    unsigned char *outputs[3] = { outbound_key, outbound_iv, outbound_mac_key };
    const unsigned long output_lengths[3] = { 16, 16, 32 };

    for (int i = 0; i < 3; i++)
    {
        idx = 0;

        for (unsigned long j = 0; j < k_len; j++)
            input[idx++] = k_encoded[j];

        for (int j = 0; j < 32; j++)
            input[idx++] = exchange_hash[j];

        input[idx++] = letters[i];

        for (int j = 0; j < 32; j++)
            input[idx++] = session_id[j];

        if (ssh_crypto_hash_sha256(digest, input, idx) != 0)
            return -1;

        for (unsigned long j = 0; j < output_lengths[i]; j++)
            outputs[i][j] = digest[j];
    }

    ssh_memset(input, 0, sizeof(input));
    ssh_memset(digest, 0, sizeof(digest));

    return 0;
}
