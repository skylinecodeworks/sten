#include "adapters.h"
#include "scatter.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Uncompressed little-endian baseline TIFF (RGB 8-bit, single strip).
 * The strip is edited in place and the file keeps its size. */

#define TIFF_TAG_WIDTH 256
#define TIFF_TAG_HEIGHT 257
#define TIFF_TAG_BITS 258
#define TIFF_TAG_COMPRESS 259
#define TIFF_TAG_PHOTOMETRIC 262
#define TIFF_TAG_STRIPOFFS 273
#define TIFF_TAG_SAMPLES 277
#define TIFF_TAG_ROWS 278
#define TIFF_TAG_STRIPCNT 279
#define TIFF_TAG_PLANAR 284

static unsigned short g_ushort(const unsigned char *d) {
    return (unsigned short)(d[0] | (d[1] << 8));
}
static unsigned long g_ulong(const unsigned char *d) {
    return (unsigned long)d[0] | ((unsigned long)d[1] << 8) |
           ((unsigned long)d[2] << 16) | ((unsigned long)d[3] << 24);
}

static int tiff_parse(const unsigned char *in, size_t len,
                      unsigned char **pixels, size_t *pix_len,
                      long *w_out, long *h_out) {
    if (len < 8 || !memcmp(in, "MM", 2)) {
        fprintf(stderr, "error: only little-endian TIFF is supported\n");
        return -1;
    }
    if (memcmp(in, "II", 2) || g_ushort(in + 2) != 42) {
        fprintf(stderr, "error: invalid TIFF\n");
        return -1;
    }
    unsigned long ifd = g_ulong(in + 4);
    if ((uint64_t)ifd + 2 > len) {
        fprintf(stderr, "error: TIFF IFD out of range\n");
        return -1;
    }
    unsigned n = g_ushort(in + ifd);
    if (n == 0) {
        fprintf(stderr, "error: TIFF with empty IFD\n");
        return -1;
    }
    if ((uint64_t)ifd + 2 + (uint64_t)n * 12 + 4 > len) {
        fprintf(stderr, "error: TIFF IFD out of range\n");
        return -1;
    }
    int have_w = 0, have_h = 0, have_offs = 0, have_cnt = 0;
    unsigned long w = 0, h = 0, offs = 0, cnt = 0;
    for (unsigned i = 0; i < n; i++) {
        const unsigned char *e = in + ifd + 2 + (unsigned long)i * 12;
        unsigned tag = g_ushort(e);
        unsigned type = g_ushort(e + 2);
        unsigned long c0 = g_ulong(e + 4);
        unsigned long v = g_ulong(e + 8);
        if (type == 3)
            v &= 0xFFFFu;
        if (tag == TIFF_TAG_WIDTH && c0 == 1) {
            w = v; have_w = 1;
        } else if (tag == TIFF_TAG_HEIGHT && c0 == 1) {
            h = v; have_h = 1;
        } else if (tag == TIFF_TAG_STRIPOFFS && c0 == 1) {
            offs = v; have_offs = 1;
        } else if (tag == TIFF_TAG_STRIPCNT && c0 == 1) {
            cnt = v; have_cnt = 1;
        } else if (tag == TIFF_TAG_BITS) {
            if (c0 == 3 && v + 6 <= len) {
                unsigned short b0 = g_ushort(in + v);
                unsigned short b1 = g_ushort(in + v + 2);
                unsigned short b2 = g_ushort(in + v + 4);
                if (b0 != 8 || b1 != 8 || b2 != 8) {
                    fprintf(stderr, "error: only 8-bit TIFF is supported\n");
                    return -1;
                }
            } else if (c0 == 1 && v != 8) {
                fprintf(stderr, "error: only 8-bit TIFF is supported\n");
                return -1;
            }
        } else if (tag == TIFF_TAG_COMPRESS) {
            if (v != 1) {
                fprintf(stderr, "error: only uncompressed TIFF is supported\n");
                return -1;
            }
        } else if (tag == TIFF_TAG_SAMPLES) {
            if (v != 3) {
                fprintf(stderr, "error: only RGB TIFF is supported\n");
                return -1;
            }
        } else if (tag == TIFF_TAG_PHOTOMETRIC) {
            if (v != 2) {
                fprintf(stderr, "error: only RGB TIFF (photometric 2) is supported\n");
                return -1;
            }
        } else if (tag == TIFF_TAG_PLANAR) {
            if (v != 1) {
                fprintf(stderr, "error: only chunky TIFF is supported\n");
                return -1;
            }
        }
    }
    if (!have_w || !have_h || !have_offs || !have_cnt) {
        fprintf(stderr, "error: TIFF is missing required fields\n");
        return -1;
    }
    if (offs > len || cnt > len - offs) {
        fprintf(stderr, "error: TIFF strip out of range\n");
        return -1;
    }
    *pixels = (unsigned char *)in + offs;
    *pix_len = (size_t)cnt;
    if (w_out)
        *w_out = (long)w;
    if (h_out)
        *h_out = (long)h;
    return 0;
}

int tiff_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
               const unsigned char *key, size_t key_len, const unsigned char *enc_key, size_t enc_key_len,
               unsigned char **out, size_t *out_len) {
    unsigned char *pixels;
    size_t pix_len;
    if (tiff_parse(in, in_len, &pixels, &pix_len, NULL, NULL))
        return -1;
    carrier_t c = { pixels, pix_len, NULL };
    int rc = scatter_embed_ex(&c, pixels, pix_len, msg, msg_len, key, key_len, 3, enc_key, enc_key_len);
    if (rc == -1) {
        fprintf(stderr, "error: message too large for this image\n");
        return -1;
    }
    if (rc == -2) {
        fprintf(stderr, "error: internal ScatterBit failure\n");
        return -1;
    }
    *out = in;
    *out_len = in_len;
    return 0;
}

int tiff_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                 const unsigned char *enc_key, size_t enc_key_len, unsigned char **msg, size_t *msg_len) {
    unsigned char *pixels;
    size_t pix_len;
    if (tiff_parse(in, in_len, &pixels, &pix_len, NULL, NULL))
        return -1;
    carrier_t c = { pixels, pix_len, NULL };
    return scatter_auto_extract_ex(&c, pixels, pix_len, key, key_len, enc_key, enc_key_len, msg, msg_len);
}

int tiff_capacity(const unsigned char *in, size_t in_len, size_t *bytes) {
    unsigned char *pixels;
    size_t pix_len;
    if (tiff_parse(in, in_len, &pixels, &pix_len, NULL, NULL))
        return -1;
    carrier_t c = { pixels, pix_len, NULL };
    *bytes = scatter_msg_capacity(&c, 3);
    return 0;
}
int tiff_inspect(const unsigned char *in, size_t in_len, sten_info_t *info) {
    unsigned char *pixels;
    size_t pix_len;
    long w = 0, h = 0;
    if (tiff_parse(in, in_len, &pixels, &pix_len, &w, &h))
        return -1;
    info->w = w;
    info->h = h;
    info->channels = 3;
    info->bits = 8;
    info->has_dims = 1;
    return 0;
}
