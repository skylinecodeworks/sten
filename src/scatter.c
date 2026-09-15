#include "scatter.h"
#include "crypto.h"
#include "deflate.h"
#include "rand.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

#define HDR_V1 12
#define HDR_V2 13
#define FLAG_COMP 0x01u
#define FLAG_ENC 0x02u
#define SALT_LEN 16
#define NONCE_LEN 12
#define PTRC_LEN 4
#define ENC_OVERHEAD (SALT_LEN + NONCE_LEN + PTRC_LEN)
#define MAX_MSG (16u * 1024u * 1024u)

/* Invariant fingerprint: only bits 3..7 (bits 0..2 are never modified). */
static uint64_t fingerprint(const unsigned char *d, size_t n) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; i++) {
        h ^= d[i] >> 3;
        h *= 0x100000001b3ULL;
    }
    return h;
}

static int allowed_bit(const carrier_t *c, size_t i) {
    if (!c->allowed)
        return 1;
    return (c->allowed[i >> 3] >> (i & 7)) & 1;
}

/* Adaptive depth by neighbourhood texture (high bits, invariant). */
static int depth_at(const unsigned char *d, size_t len, size_t i) {
    int hi = d[i] >> 3;
    int dmax = 0;
    if (i > 0) {
        int x = (int)(d[i - 1] >> 3) - hi;
        if (x < 0) x = -x;
        if (x > dmax) dmax = x;
    }
    if (i + 1 < len) {
        int x = (int)(d[i + 1] >> 3) - hi;
        if (x < 0) x = -x;
        if (x > dmax) dmax = x;
    }
    if (dmax <= 1) return 1;
    if (dmax <= 4) return 2;
    return 3;
}

static size_t capacity(const carrier_t *c) {
    size_t cap = 0;
    for (size_t i = 0; i < c->len; i++)
        if (allowed_bit(c, i))
            cap += (size_t)depth_at(c->data, c->len, i);
    return cap;
}

static int pos_ok(const carrier_t *c, const unsigned char *used, size_t i) {
    if (!allowed_bit(c, i))
        return 0;
    return used[i] < (unsigned char)depth_at(c->data, c->len, i);
}

static int enc_key_derive(const unsigned char *enc_key, size_t enc_key_len,
                          const unsigned char salt[16], unsigned char ekey[32]) {
    unsigned char key32[32];
    pbkdf2_sha256(enc_key, enc_key_len, salt, SALT_LEN,
                  STEN_PBKDF2_ITERS, key32, sizeof(key32));
    unsigned char mixin[48];
    memcpy(mixin, key32, 32);
    memcpy(mixin + 32, salt, SALT_LEN);
    sha256(mixin, sizeof(mixin), ekey);
    return 0;
}

static int scatter_embed_v(carrier_t *c, const unsigned char *fpsrc, size_t fpsrc_len,
                           const unsigned char *msg, size_t msg_len,
                           const unsigned char *key, size_t key_len, int redundancy,
                           int version, const unsigned char *enc_key, size_t enc_key_len) {
    if (msg_len > MAX_MSG || redundancy < 1 || redundancy > 3)
        return -2;

    size_t hdr = (version == 1) ? HDR_V1 : HDR_V2;
    unsigned flags = 0;
    const unsigned char *stored = msg;
    size_t stored_len = msg_len;
    unsigned char *owned1 = NULL;
    unsigned char *owned2 = NULL;

    if (version == 2 && msg_len) {
        size_t zlen = 0;
        if (deflate_zlib(msg, msg_len, &owned1, &zlen) == 0 && zlen < msg_len) {
            stored = owned1;
            stored_len = zlen;
            flags |= FLAG_COMP;
        }
    }

    if (version == 2 && enc_key) {
        flags |= FLAG_ENC;
        size_t enc_len = ENC_OVERHEAD + stored_len;
        owned2 = (unsigned char *)malloc(enc_len);
        if (!owned2) {
            free(owned1);
            return -2;
        }
        unsigned char salt[SALT_LEN];
        unsigned char nonce[NONCE_LEN];
        if (rand_bytes(salt, sizeof(salt)) != 0 ||
            rand_bytes(nonce, sizeof(nonce)) != 0) {
            free(owned1);
            free(owned2);
            return -2;
        }
        unsigned char ekey[32];
        enc_key_derive(enc_key, enc_key_len, salt, ekey);
        memcpy(owned2, salt, SALT_LEN);
        memcpy(owned2 + SALT_LEN, nonce, NONCE_LEN);
        memcpy(owned2 + SALT_LEN + NONCE_LEN, stored, stored_len);
        chacha20_xor(owned2 + SALT_LEN + NONCE_LEN, stored_len, ekey, nonce, 1);
        uint32_t pt = crc32(stored, stored_len);
        owned2[enc_len - 4] = (unsigned char)(pt >> 24);
        owned2[enc_len - 3] = (unsigned char)(pt >> 16);
        owned2[enc_len - 2] = (unsigned char)(pt >> 8);
        owned2[enc_len - 1] = (unsigned char)pt;
        stored = owned2;
        stored_len = enc_len;
    }

    size_t plen = hdr + stored_len;
    unsigned char *payload = (unsigned char *)malloc(plen);
    if (!payload) {
        free(owned1);
        free(owned2);
        return -2;
    }
    if (version == 1) {
        memcpy(payload, "SBT1", 4);
        payload[4] = (unsigned char)(msg_len >> 24);
        payload[5] = (unsigned char)(msg_len >> 16);
        payload[6] = (unsigned char)(msg_len >> 8);
        payload[7] = (unsigned char)msg_len;
        uint32_t ck = crc32(msg, msg_len);
        payload[8] = (unsigned char)(ck >> 24);
        payload[9] = (unsigned char)(ck >> 16);
        payload[10] = (unsigned char)(ck >> 8);
        payload[11] = (unsigned char)ck;
    } else {
        memcpy(payload, "SBT2", 4);
        payload[4] = (unsigned char)flags;
        payload[5] = (unsigned char)(stored_len >> 24);
        payload[6] = (unsigned char)(stored_len >> 16);
        payload[7] = (unsigned char)(stored_len >> 8);
        payload[8] = (unsigned char)stored_len;
        uint32_t ck = crc32(stored, stored_len);
        payload[9] = (unsigned char)(ck >> 24);
        payload[10] = (unsigned char)(ck >> 16);
        payload[11] = (unsigned char)(ck >> 8);
        payload[12] = (unsigned char)ck;
    }
    memcpy(payload + hdr, stored, stored_len);
    free(owned1);
    free(owned2);

    size_t nbits = plen * 8;
    if (nbits * (size_t)redundancy > capacity(c)) {
        free(payload);
        return -1;
    }

    rng_t rng;
    uint64_t seed = fingerprint(fpsrc, fpsrc_len);
    if (key && key_len)
        seed ^= fnv1a64(key, key_len);
    rng_seed(&rng, seed);

    unsigned char *used = (unsigned char *)calloc(c->len, 1);
    if (!used) {
        free(payload);
        return -2;
    }

    for (size_t b = 0; b < nbits; b++) {
        int bit = (payload[b >> 3] >> (7 - (int)(b & 7))) & 1;
        for (int r = 0; r < redundancy; r++) {
            size_t i;
            do {
                i = rng_next(&rng) % c->len;
            } while (!pos_ok(c, used, i));
            c->data[i] = (unsigned char)((c->data[i] & ~(1u << used[i])) |
                                         ((unsigned)bit << used[i]));
            used[i]++;
        }
    }

    free(used);
    free(payload);
    return 0;
}

int scatter_embed(carrier_t *c, const unsigned char *fpsrc, size_t fpsrc_len,
                  const unsigned char *msg, size_t msg_len,
                  const unsigned char *key, size_t key_len, int redundancy) {
    return scatter_embed_v(c, fpsrc, fpsrc_len, msg, msg_len, key, key_len,
                           redundancy, 2, NULL, 0);
}

int scatter_embed_ex(carrier_t *c, const unsigned char *fpsrc, size_t fpsrc_len,
                     const unsigned char *msg, size_t msg_len,
                     const unsigned char *key, size_t key_len, int redundancy,
                     const unsigned char *enc_key, size_t enc_key_len) {
    return scatter_embed_v(c, fpsrc, fpsrc_len, msg, msg_len, key, key_len,
                           redundancy, 2, enc_key, enc_key_len);
}

int scatter_embed_v1(carrier_t *c, const unsigned char *fpsrc, size_t fpsrc_len,
                     const unsigned char *msg, size_t msg_len,
                     const unsigned char *key, size_t key_len, int redundancy) {
    return scatter_embed_v(c, fpsrc, fpsrc_len, msg, msg_len, key, key_len,
                           redundancy, 1, NULL, 0);
}

static int extract_with_r(const carrier_t *c, const unsigned char *fpsrc, size_t fpsrc_len,
                          const unsigned char *key, size_t key_len, int redundancy,
                          const unsigned char *enc_key, size_t enc_key_len,
                          unsigned char **msg, size_t *msg_len) {
    size_t cap = capacity(c);
    if (cap < 64u * (size_t)redundancy)
        return 1;

    rng_t rng;
    uint64_t seed = fingerprint(fpsrc, fpsrc_len);
    if (key && key_len)
        seed ^= fnv1a64(key, key_len);
    rng_seed(&rng, seed);

    unsigned char *used = (unsigned char *)calloc(c->len, 1);
    if (!used)
        return -1;

    unsigned char *payload = NULL;
    size_t pcap = 0;
    size_t nbits = 0;
    uint32_t mlen = 0;
    int mlen_known = 0;
    int hdr = 0;
    unsigned flags = 0;
    int sum = 0, cnt = 0;
    int rc = 1;

    while (1) {
        size_t i;
        do {
            i = rng_next(&rng) % c->len;
        } while (!pos_ok(c, used, i));
        sum += (c->data[i] >> used[i]) & 1;
        used[i]++;
        if (++cnt != redundancy)
            continue;

        int bit = sum > redundancy / 2;
        nbits++;
        size_t bidx = (nbits - 1) >> 3;
        if (bidx >= pcap) {
            size_t np = pcap ? pcap * 2 : 64;
            while (np <= bidx)
                np *= 2;
            unsigned char *nd = (unsigned char *)realloc(payload, np);
            if (!nd) {
                rc = -1;
                goto done;
            }
            payload = nd;
            pcap = np;
        }
        if (bit)
            payload[bidx] |= (unsigned char)(1u << (7 - ((nbits - 1) & 7)));
        else
            payload[bidx] &= (unsigned char)~(1u << (7 - ((nbits - 1) & 7)));
        sum = cnt = 0;

        if (nbits == 32) {
            if (memcmp(payload, "SBT1", 4) == 0)
                hdr = HDR_V1;
            else if (memcmp(payload, "SBT2", 4) == 0)
                hdr = HDR_V2;
            else
                goto done;
        }
        size_t len_bits = 8u * (size_t)hdr - 32;
        if (mlen_known == 0 && hdr && nbits == len_bits) {
            if (hdr == HDR_V2) {
                flags = payload[4];
                mlen = ((uint32_t)payload[5] << 24) | ((uint32_t)payload[6] << 16) |
                       ((uint32_t)payload[7] << 8) | payload[8];
            } else {
                mlen = ((uint32_t)payload[4] << 24) | ((uint32_t)payload[5] << 16) |
                       ((uint32_t)payload[6] << 8) | payload[7];
            }
            if (mlen > MAX_MSG)
                goto done;
            mlen_known = 1;
            if (8u * ((size_t)hdr + mlen) * (size_t)redundancy > cap)
                goto done;
            unsigned char *nd = (unsigned char *)realloc(payload, (size_t)hdr + mlen);
            if (!nd) {
                rc = -1;
                goto done;
            }
            payload = nd;
            pcap = (size_t)hdr + mlen;
        }
        if (mlen_known && nbits == 8u * ((size_t)hdr + mlen)) {
            uint32_t want = ((uint32_t)payload[hdr - 4] << 24) |
                            ((uint32_t)payload[hdr - 3] << 16) |
                            ((uint32_t)payload[hdr - 2] << 8) | payload[hdr - 1];
            if (crc32(payload + hdr, mlen) != want)
                goto done;
            const unsigned char *data = payload + hdr;
            size_t data_len = mlen;
            unsigned char *dec = NULL;
            if (hdr == HDR_V2 && (flags & FLAG_ENC)) {
                if (!enc_key)
                    goto done;
                if (mlen < ENC_OVERHEAD)
                    goto done;
                const unsigned char *salt = data;
                const unsigned char *nonce = data + SALT_LEN;
                size_t ct_len = mlen - ENC_OVERHEAD;
                uint32_t want_pt = ((uint32_t)data[mlen - 4] << 24) |
                                   ((uint32_t)data[mlen - 3] << 16) |
                                   ((uint32_t)data[mlen - 2] << 8) |
                                   data[mlen - 1];
                unsigned char ekey[32];
                enc_key_derive(enc_key, enc_key_len, salt, ekey);
                dec = (unsigned char *)malloc(ct_len ? ct_len : 1);
                if (!dec) {
                    rc = -1;
                    goto done;
                }
                memcpy(dec, data + SALT_LEN + NONCE_LEN, ct_len);
                chacha20_xor(dec, ct_len, ekey, nonce, 1);
                if (crc32(dec, ct_len) != want_pt) {
                    free(dec);
                    goto done;
                }
                data = dec;
                data_len = ct_len;
            }
            unsigned char *m = NULL;
            size_t mlen_o = 0;
            if (hdr == HDR_V2 && (flags & FLAG_COMP)) {
                if (inflate_zlib(data, data_len, &m, &mlen_o, 0) != 0) {
                    free(dec);
                    goto done;
                }
                free(dec);
                if (mlen_o > MAX_MSG) {
                    free(m);
                    goto done;
                }
            } else {
                m = (unsigned char *)malloc(data_len ? data_len : 1);
                if (!m) {
                    free(dec);
                    rc = -1;
                    goto done;
                }
                memcpy(m, data, data_len);
                mlen_o = data_len;
                free(dec);
            }
            *msg = m;
            *msg_len = mlen_o;
            rc = 0;
            goto done;
        }
    }

done:
    free(payload);
    free(used);
    return rc;
}

int scatter_auto_extract(const carrier_t *c, const unsigned char *fpsrc, size_t fpsrc_len,
                         const unsigned char *key, size_t key_len,
                         unsigned char **msg, size_t *msg_len) {
    return scatter_auto_extract_ex(c, fpsrc, fpsrc_len, key, key_len, NULL, 0, msg, msg_len);
}

int scatter_auto_extract_ex(const carrier_t *c, const unsigned char *fpsrc, size_t fpsrc_len,
                            const unsigned char *key, size_t key_len,
                            const unsigned char *enc_key, size_t enc_key_len,
                            unsigned char **msg, size_t *msg_len) {
    for (int r = 3; r >= 1; r--) {
        int rc = extract_with_r(c, fpsrc, fpsrc_len, key, key_len, r, enc_key, enc_key_len,
                                msg, msg_len);
        if (rc == 0)
            return 0;
        if (rc < 0)
            return rc;
    }
    return 1;
}

size_t scatter_msg_capacity(const carrier_t *c, int redundancy) {
    size_t cap = capacity(c);
    size_t payload_avail = cap / (8u * (size_t)redundancy);
    return payload_avail >= (size_t)HDR_V2 ? payload_avail - (size_t)HDR_V2 : 0;
}
