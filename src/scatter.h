#ifndef STEN_SCATTER_H
#define STEN_SCATTER_H

#include <stddef.h>

#define SCATTER_MAGIC "SBT1"

typedef struct {
    unsigned char *data;
    size_t len;
    const unsigned char *allowed; /* bitmap of editable bytes; NULL = all */
} carrier_t;

/* Embeds msg into the carrier with the ScatterBit algorithm.
 * fpsrc/fpsrc_len: source bytes for the content fingerprint
 * (must be identical in embed and extract; only their high bits are hashed).
 * redundancy: replicas of each bit (1..3).
 * Returns 0 ok, -1 message too large, -2 internal error. */
int scatter_embed(carrier_t *c, const unsigned char *fpsrc, size_t fpsrc_len,
                  const unsigned char *msg, size_t msg_len,
                  const unsigned char *key, size_t key_len, int redundancy);

/* Legacy v1 (SBT1) embed; the extractor still reads v1 payloads. */
int scatter_embed_v1(carrier_t *c, const unsigned char *fpsrc, size_t fpsrc_len,
                     const unsigned char *msg, size_t msg_len,
                     const unsigned char *key, size_t key_len, int redundancy);

/* Same as scatter_embed, but encrypts the stored payload with a 32-byte key
 * (ChaCha20; salt/nonce embedded in the payload). enc_key NULL = plain. */
int scatter_embed_ex(carrier_t *c, const unsigned char *fpsrc, size_t fpsrc_len,
                     const unsigned char *msg, size_t msg_len,
                     const unsigned char *key, size_t key_len, int redundancy,
                     const unsigned char *enc_key);

/* Extracts by testing redundancies 3, 2 and 1.
 * Returns 0 ok, 1 no message (or wrong key), -1 internal error. */
int scatter_auto_extract(const carrier_t *c, const unsigned char *fpsrc, size_t fpsrc_len,
                         const unsigned char *key, size_t key_len,
                         unsigned char **msg, size_t *msg_len);

/* Variant with a 32-byte decryption key, as scatter_embed_ex. */
int scatter_auto_extract_ex(const carrier_t *c, const unsigned char *fpsrc, size_t fpsrc_len,
                            const unsigned char *key, size_t key_len,
                            const unsigned char *enc_key,
                            unsigned char **msg, size_t *msg_len);

/* Maximum embeddable message bytes at the given redundancy. */
size_t scatter_msg_capacity(const carrier_t *c, int redundancy);

#endif
