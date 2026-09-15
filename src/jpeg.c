#include "adapters.h"
#include "scatter.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t start;     /* position of the 0xFF of the FF FE marker */
    size_t ext_end;   /* end of the full segment (marker + length + data) */
    const unsigned char *payload;
    uint32_t payload_len;
} cominfo_t;

static int jpeg_find_com(const unsigned char *in, size_t len, cominfo_t *ci) {
    size_t p = 2;
    while (p + 1 < len) {
        if (in[p] != 0xFF) {
            p++;
            continue;
        }
        size_t q = p;
        while (p < len && in[p] == 0xFF)
            p++;
        if (p >= len)
            break;
        unsigned char m = in[p++];
        if (m == 0xD9 || m == 0xD8)
            break;
        if (m == 0x01)
            continue;
        if (m >= 0xD0 && m <= 0xD7)
            continue;
        if (m == 0xDA) {
            while (p + 1 < len) {
                if (in[p] == 0xFF && in[p + 1] != 0x00 &&
                    !(in[p + 1] >= 0xD0 && in[p + 1] <= 0xD7))
                    break;
                p++;
            }
            continue;
        }
        if (p + 2 > len)
            break;
        uint32_t l = ((uint32_t)in[p] << 8) | in[p + 1];
        if (l < 2 || (uint64_t)p + l > len)
            break;
        if (m == 0xFE) {
            ci->start = q;
            ci->ext_end = p + l;
            ci->payload = in + p + 2;
            ci->payload_len = l - 2;
            return 1;
        }
        p += l;
    }
    return 0;
}

int jpeg_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
               const unsigned char *key, size_t key_len, const unsigned char *enc_key, size_t enc_key_len,
               unsigned char **out, size_t *out_len) {
    if (in_len < 4 || in[0] != 0xFF || in[1] != 0xD8) {
        fprintf(stderr, "error: invalid JPEG\n");
        return -1;
    }
    size_t overhead = enc_key ? 32u : 0u;
    size_t plen = 13 + msg_len + overhead;
    if (plen * 4 > 65533) {
        fprintf(stderr, "error: message too large for JPEG (max ~16 KB)\n");
        return -1;
    }
    size_t L = plen * 4;
    if (in_len + (size_t)4 + L < in_len) {
        fprintf(stderr, "error: JPEG too large\n");
        return -1;
    }
    size_t olen = in_len + 4 + L;
    unsigned char *o = (unsigned char *)malloc(olen);
    if (!o) {
        fprintf(stderr, "error: out of memory\n");
        return -1;
    }
    memcpy(o, in, 2);
    o[2] = 0xFF;
    o[3] = 0xFE;
    size_t lenc = L + 2;
    o[4] = (unsigned char)(lenc >> 8);
    o[5] = (unsigned char)(lenc & 0xFF);

    rng_t rng;
    rng_seed(&rng, fnv1a64(in, in_len));
    for (size_t i = 0; i < L; i++)
        o[6 + i] = (unsigned char)(rng_next(&rng) >> 32);

    carrier_t c = { o + 6, L, NULL };
    int rc = scatter_embed_ex(&c, in, in_len, msg, msg_len, key, key_len, 1, enc_key, enc_key_len);
    if (rc != 0) {
        free(o);
        fprintf(stderr, "error: could not embed in the COM segment\n");
        return -1;
    }
    memcpy(o + 6 + L, in + 2, in_len - 2);
    *out = o;
    *out_len = olen;
    return 0;
}

int jpeg_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                 const unsigned char *enc_key, size_t enc_key_len, unsigned char **msg, size_t *msg_len) {
    if (in_len < 4 || in[0] != 0xFF || in[1] != 0xD8) {
        fprintf(stderr, "error: invalid JPEG\n");
        return -1;
    }
    cominfo_t ci;
    if (!jpeg_find_com(in, in_len, &ci))
        return 1;

    size_t skip = ci.ext_end - ci.start;
    size_t flen = in_len - skip;
    unsigned char *fp = (unsigned char *)malloc(flen ? flen : 1);
    if (!fp) {
        fprintf(stderr, "error: out of memory\n");
        return -1;
    }
    memcpy(fp, in, ci.start);
    memcpy(fp + ci.start, in + ci.ext_end, in_len - ci.ext_end);

    carrier_t c = { (unsigned char *)ci.payload, ci.payload_len, NULL };
    int rc = scatter_auto_extract_ex(&c, fp, flen, key, key_len, enc_key, enc_key_len, msg, msg_len);
    free(fp);
    return rc;
}

int jpeg_capacity(const unsigned char *in, size_t in_len, size_t *bytes) {
    if (in_len < 4 || in[0] != 0xFF || in[1] != 0xD8)
        return -1;
    *bytes = (65533 / 4) > 13 ? (65533 / 4) - 13 : 0;
    return 0;
}

/* Scans entropy-structure markers to locate SOF and its dimensions. */
static int jpeg_scan_sof(const unsigned char *in, size_t len,
                         unsigned *w, unsigned *h, unsigned *components) {
    size_t p = 2;
    while (p + 1 < len) {
        if (in[p] != 0xFF) {
            p++;
            continue;
        }
        /* skip runs of 0xFF fill bytes */
        while (p < len && in[p] == 0xFF)
            p++;
        if (p >= len)
            break;
        unsigned char m = in[p++];
        if (m == 0xD9 || m == 0xD8)
            break;
        if (m == 0x01)
            continue;
        if (m >= 0xD0 && m <= 0xD7)
            continue;
        if (m == 0xDA) {
            while (p + 1 < len) {
                if (in[p] == 0xFF && in[p + 1] != 0x00 &&
                    !(in[p + 1] >= 0xD0 && in[p + 1] <= 0xD7))
                    break;
                p++;
            }
            continue;
        }
        if (p + 2 > len)
            break;
        uint32_t l = ((uint32_t)in[p] << 8) | in[p + 1];
        if (l < 2 || (uint64_t)p + l > len)
            break;
        if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
            if (l >= 8) {
                *h = ((unsigned)in[p + 3] << 8) | in[p + 4];
                *w = ((unsigned)in[p + 5] << 8) | in[p + 6];
                *components = in[p + 7];
                return 1;
            }
            return 0;
        }
        p += l;
    }
    return 0;
}

int jpeg_inspect(const unsigned char *in, size_t in_len, sten_info_t *info) {
    if (in_len < 4 || in[0] != 0xFF || in[1] != 0xD8)
        return -1;
    unsigned w = 0, h = 0, comp = 0;
    info->w = 0;
    info->h = 0;
    info->channels = 3;
    info->bits = 8;
    info->has_dims = jpeg_scan_sof(in, in_len, &w, &h, &comp);
    if (info->has_dims) {
        info->w = (long)w;
        info->h = (long)h;
        info->channels = (int)comp;
    }
    return 0;
}
