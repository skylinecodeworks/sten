#ifndef STEN_CRYPTO_H
#define STEN_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#define STEN_PBKDF2_ITERS 100000u

void sha256(const unsigned char *data, size_t len, unsigned char out[32]);
void hmac_sha256(const unsigned char *key, size_t key_len,
                 const unsigned char *data, size_t data_len, unsigned char out[32]);
void pbkdf2_sha256(const unsigned char *pwd, size_t pwd_len,
                   const unsigned char *salt, size_t salt_len,
                   unsigned iters, unsigned char *out, size_t out_len);
void chacha20_xor(unsigned char *buf, size_t len,
                  const unsigned char key[32], const unsigned char nonce[12],
                  uint32_t counter);

#endif