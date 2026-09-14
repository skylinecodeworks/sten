#include "crypto.h"
#include <stdlib.h>
#include <string.h>

/* SHA-256 */

typedef struct {
    uint32_t h[8];
    uint64_t nbytes;
    unsigned char buf[64];
    size_t buflen;
} sha256_ctx;

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t rotl(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

static void sha256_block(sha256_ctx *c, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotl(w[i - 15], 25) ^ rotl(w[i - 15], 14) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotl(w[i - 2], 15) ^ rotl(w[i - 2], 13) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3];
    uint32_t e = c->h[4], f = c->h[5], g = c->h[6], h = c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotl(e, 26) ^ rotl(e, 21) ^ rotl(e, 7);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + sha256_k[i] + w[i];
        uint32_t S0 = rotl(a, 30) ^ rotl(a, 19) ^ rotl(a, 10);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
    c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}

static void sha256_init(sha256_ctx *c) {
    c->h[0] = 0x6a09e667; c->h[1] = 0xbb67ae85; c->h[2] = 0x3c6ef372; c->h[3] = 0xa54ff53a;
    c->h[4] = 0x510e527f; c->h[5] = 0x9b05688c; c->h[6] = 0x1f83d9ab; c->h[7] = 0x5be0cd19;
    c->nbytes = 0;
    c->buflen = 0;
}

static void sha256_update(sha256_ctx *c, const unsigned char *data, size_t len) {
    c->nbytes += len;
    while (len > 0) {
        size_t take = 64 - c->buflen;
        if (take > len)
            take = len;
        memcpy(c->buf + c->buflen, data, take);
        c->buflen += take;
        data += take;
        len -= take;
        if (c->buflen == 64) {
            sha256_block(c, c->buf);
            c->buflen = 0;
        }
    }
}

static void sha256_final(sha256_ctx *c, unsigned char out[32]) {
    uint64_t bits = c->nbytes * 8;
    unsigned char pad = 0x80;
    sha256_update(c, &pad, 1);
    static const unsigned char zero[64] = { 0 };
    while (c->buflen > 56)
        sha256_update(c, zero, 64 - c->buflen);
    if (c->buflen < 56)
        sha256_update(c, zero, 56 - c->buflen);
    unsigned char blen[8];
    for (int i = 0; i < 8; i++)
        blen[i] = (unsigned char)(bits >> (56 - i * 8));
    sha256_update(c, blen, 8);
    for (int i = 0; i < 8; i++) {
        out[i * 4] = (unsigned char)(c->h[i] >> 24);
        out[i * 4 + 1] = (unsigned char)(c->h[i] >> 16);
        out[i * 4 + 2] = (unsigned char)(c->h[i] >> 8);
        out[i * 4 + 3] = (unsigned char)c->h[i];
    }
}

void sha256(const unsigned char *data, size_t len, unsigned char out[32]) {
    sha256_ctx c;
    sha256_init(&c);
    sha256_update(&c, data, len);
    sha256_final(&c, out);
}

/* HMAC-SHA256 */

void hmac_sha256(const unsigned char *key, size_t key_len,
                 const unsigned char *data, size_t data_len, unsigned char out[32]) {
    unsigned char k[64];
    if (key_len > 64) {
        sha256(key, key_len, k);
        memset(k + 32, 0, 32);
    } else {
        memset(k, 0, 64);
        memcpy(k, key, key_len);
    }
    unsigned char ipad[64], opad[64];
    for (int i = 0; i < 64; i++) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }
    sha256_ctx c;
    sha256_init(&c);
    sha256_update(&c, ipad, 64);
    sha256_update(&c, data, data_len);
    unsigned char ih[32];
    sha256_final(&c, ih);
    sha256_init(&c);
    sha256_update(&c, opad, 64);
    sha256_update(&c, ih, 32);
    sha256_final(&c, out);
}

/* PBKDF2-HMAC-SHA256 */

static void wr32be_un(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

void pbkdf2_sha256(const unsigned char *pwd, size_t pwd_len,
                   const unsigned char *salt, size_t salt_len,
                   unsigned iters, unsigned char *out, size_t out_len) {
    unsigned char *block = (unsigned char *)malloc(salt_len + 4);
    if (!block)
        abort();
    unsigned char u[32], t[32];
    size_t written = 0;
    unsigned int idx = 1;
    while (written < out_len) {
        memcpy(block, salt, salt_len);
        wr32be_un(block + salt_len, idx);
        hmac_sha256(pwd, pwd_len, block, salt_len + 4, u);
        memcpy(t, u, 32);
        for (unsigned i = 1; i < iters; i++) {
            hmac_sha256(pwd, pwd_len, u, 32, u);
            for (int j = 0; j < 32; j++)
                t[j] ^= u[j];
        }
        size_t take = out_len - written;
        if (take > 32)
            take = 32;
        memcpy(out + written, t, take);
        written += take;
        idx++;
    }
    free(block);
}

/* ChaCha20 (RFC 8439; initial counter/counter param) */

static uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void w32le(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static uint32_t rl(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

static void chacha_block(const uint32_t in[16], uint32_t out[16]) {
    uint32_t x[16];
    memcpy(x, in, sizeof(x));
    for (int i = 0; i < 10; i++) {
        x[0] += x[4]; x[12] = rl(x[12] ^ x[0], 16);
        x[8] += x[12]; x[4] = rl(x[4] ^ x[8], 12);
        x[0] += x[4]; x[12] = rl(x[12] ^ x[0], 8);
        x[8] += x[12]; x[4] = rl(x[4] ^ x[8], 7);
        x[1] += x[5]; x[13] = rl(x[13] ^ x[1], 16);
        x[9] += x[13]; x[5] = rl(x[5] ^ x[9], 12);
        x[1] += x[5]; x[13] = rl(x[13] ^ x[1], 8);
        x[9] += x[13]; x[5] = rl(x[5] ^ x[9], 7);
        x[2] += x[6]; x[14] = rl(x[14] ^ x[2], 16);
        x[10] += x[14]; x[6] = rl(x[6] ^ x[10], 12);
        x[2] += x[6]; x[14] = rl(x[14] ^ x[2], 8);
        x[10] += x[14]; x[6] = rl(x[6] ^ x[10], 7);
        x[3] += x[7]; x[15] = rl(x[15] ^ x[3], 16);
        x[11] += x[15]; x[7] = rl(x[7] ^ x[11], 12);
        x[3] += x[7]; x[15] = rl(x[15] ^ x[3], 8);
        x[11] += x[15]; x[7] = rl(x[7] ^ x[11], 7);
        x[0] += x[5]; x[15] = rl(x[15] ^ x[0], 16);
        x[10] += x[15]; x[5] = rl(x[5] ^ x[10], 12);
        x[0] += x[5]; x[15] = rl(x[15] ^ x[0], 8);
        x[10] += x[15]; x[5] = rl(x[5] ^ x[10], 7);
        x[1] += x[6]; x[12] = rl(x[12] ^ x[1], 16);
        x[11] += x[12]; x[6] = rl(x[6] ^ x[11], 12);
        x[1] += x[6]; x[12] = rl(x[12] ^ x[1], 8);
        x[11] += x[12]; x[6] = rl(x[6] ^ x[11], 7);
        x[2] += x[7]; x[13] = rl(x[13] ^ x[2], 16);
        x[8] += x[13]; x[7] = rl(x[7] ^ x[8], 12);
        x[2] += x[7]; x[13] = rl(x[13] ^ x[2], 8);
        x[8] += x[13]; x[7] = rl(x[7] ^ x[8], 7);
        x[3] += x[4]; x[14] = rl(x[14] ^ x[3], 16);
        x[9] += x[14]; x[4] = rl(x[4] ^ x[9], 12);
        x[3] += x[4]; x[14] = rl(x[14] ^ x[3], 8);
        x[9] += x[14]; x[4] = rl(x[4] ^ x[9], 7);
    }
    for (int i = 0; i < 16; i++)
        out[i] = x[i] + in[i];
}

void chacha20_xor(unsigned char *buf, size_t len,
                  const unsigned char key[32], const unsigned char nonce[12],
                  uint32_t counter) {
    uint32_t in[16];
    in[0] = 0x61707865;
    in[1] = 0x3320646e;
    in[2] = 0x79622d32;
    in[3] = 0x6b206574;
    for (int i = 0; i < 8; i++)
        in[4 + i] = le32(key + i * 4);
    in[12] = counter;
    in[13] = le32(nonce);
    in[14] = le32(nonce + 4);
    in[15] = le32(nonce + 8);
    unsigned char keystream[64];
    size_t off = 0;
    while (off < len) {
        uint32_t out[16];
        chacha_block(in, out);
        for (int i = 0; i < 16; i++)
            w32le(keystream + i * 4, out[i]);
        size_t take = len - off;
        if (take > 64)
            take = 64;
        for (size_t i = 0; i < take; i++)
            buf[off + i] ^= keystream[i];
        off += take;
        in[12]++;
    }
}