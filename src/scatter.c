#include "scatter.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

#define HDR 12
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

int scatter_embed(carrier_t *c, const unsigned char *fpsrc, size_t fpsrc_len,
                  const unsigned char *msg, size_t msg_len,
                  const unsigned char *key, size_t key_len, int redundancy) {
    if (msg_len > MAX_MSG || redundancy < 1 || redundancy > 3)
        return -2;

    size_t plen = HDR + msg_len;
    unsigned char *payload = (unsigned char *)malloc(plen);
    if (!payload)
        return -2;
    memcpy(payload, SCATTER_MAGIC, 4);
    payload[4] = (unsigned char)(msg_len >> 24);
    payload[5] = (unsigned char)(msg_len >> 16);
    payload[6] = (unsigned char)(msg_len >> 8);
    payload[7] = (unsigned char)msg_len;
    uint32_t ck = crc32(msg, msg_len);
    payload[8] = (unsigned char)(ck >> 24);
    payload[9] = (unsigned char)(ck >> 16);
    payload[10] = (unsigned char)(ck >> 8);
    payload[11] = (unsigned char)ck;
    memcpy(payload + HDR, msg, msg_len);

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

static int extract_with_r(const carrier_t *c, const unsigned char *fpsrc, size_t fpsrc_len,
                          const unsigned char *key, size_t key_len, int redundancy,
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
            if (memcmp(payload, SCATTER_MAGIC, 4) != 0)
                goto done;
        }
        if (nbits == 64) {
            mlen = ((uint32_t)payload[4] << 24) | ((uint32_t)payload[5] << 16) |
                   ((uint32_t)payload[6] << 8) | payload[7];
            if (mlen > MAX_MSG)
                goto done;
            mlen_known = 1;
            if (8u * (HDR + mlen) * (size_t)redundancy > cap)
                goto done;
            unsigned char *nd = (unsigned char *)realloc(payload, HDR + mlen);
            if (!nd) {
                rc = -1;
                goto done;
            }
            payload = nd;
            pcap = HDR + mlen;
        }
        if (mlen_known && nbits == 8u * (HDR + mlen)) {
            uint32_t want = ((uint32_t)payload[8] << 24) | ((uint32_t)payload[9] << 16) |
                            ((uint32_t)payload[10] << 8) | payload[11];
            if (crc32(payload + HDR, mlen) != want)
                goto done;
            unsigned char *m = (unsigned char *)malloc(mlen ? mlen : 1);
            if (!m) {
                rc = -1;
                goto done;
            }
            memcpy(m, payload + HDR, mlen);
            *msg = m;
            *msg_len = mlen;
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
    for (int r = 3; r >= 1; r--) {
        int rc = extract_with_r(c, fpsrc, fpsrc_len, key, key_len, r, msg, msg_len);
        if (rc == 0)
            return 0;
        if (rc < 0)
            return rc;
    }
    return 1;
}
