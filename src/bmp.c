#include "adapters.h"
#include "scatter.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd32le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int bmp_parse(const unsigned char *in, size_t len,
                     unsigned char **pixels, size_t *pix_len,
                     size_t *stride, size_t *rowbytes, size_t *rows) {
    if (len < 54 || in[0] != 'B' || in[1] != 'M') {
        fprintf(stderr, "error: invalid BMP\n");
        return -1;
    }
    uint32_t off = rd32le(in + 10);
    uint32_t bpp = (uint32_t)in[28] | ((uint32_t)in[29] << 8);
    uint32_t comp = rd32le(in + 30);
    int64_t w = (int64_t)(int32_t)rd32le(in + 18);
    int64_t hraw = (int64_t)(int32_t)rd32le(in + 22);
    if (bpp != 24 && bpp != 32) {
        fprintf(stderr, "error: BMP must be 24 or 32 bits per pixel\n");
        return -1;
    }
    if (comp != 0) {
        fprintf(stderr, "error: compressed BMP not supported\n");
        return -1;
    }
    if (w <= 0 || hraw == 0) {
        fprintf(stderr, "error: invalid dimensions\n");
        return -1;
    }
    int64_t h = hraw < 0 ? -hraw : hraw;
    size_t rb = (size_t)w * (bpp / 8);
    size_t st = (rb + 3) & ~(size_t)3;
    size_t plen = st * (size_t)h;
    if ((uint64_t)off + plen > len) {
        fprintf(stderr, "error: pixel data out of range\n");
        return -1;
    }
    *pixels = (unsigned char *)in + off;
    *pix_len = plen;
    *stride = st;
    *rowbytes = rb;
    *rows = (size_t)h;
    return 0;
}

static unsigned char *bmp_allowed(size_t stride, size_t rowbytes, size_t rows) {
    size_t plen = stride * rows;
    unsigned char *a = (unsigned char *)calloc((plen + 7) / 8, 1);
    if (!a)
        return NULL;
    for (size_t y = 0; y < rows; y++)
        for (size_t x = 0; x < rowbytes; x++) {
            size_t i = y * stride + x;
            a[i >> 3] |= (unsigned char)(1u << (i & 7));
        }
    return a;
}

int bmp_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key,
              unsigned char **out, size_t *out_len) {
    unsigned char *pixels;
    size_t pix_len, stride, rb, rows;
    if (bmp_parse(in, in_len, &pixels, &pix_len, &stride, &rb, &rows))
        return -1;
    unsigned char *allowed = bmp_allowed(stride, rb, rows);
    if (!allowed) {
        fprintf(stderr, "error: out of memory\n");
        return -1;
    }
    carrier_t c = { pixels, pix_len, allowed };
    int rc = scatter_embed_ex(&c, pixels, pix_len, msg, msg_len, key, key_len, 3, enc_key);
    free(allowed);
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

int bmp_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, unsigned char **msg, size_t *msg_len) {
    unsigned char *pixels;
    size_t pix_len, stride, rb, rows;
    if (bmp_parse(in, in_len, &pixels, &pix_len, &stride, &rb, &rows))
        return -1;
    unsigned char *allowed = bmp_allowed(stride, rb, rows);
    if (!allowed)
        return -1;
    carrier_t c = { pixels, pix_len, allowed };
    int rc = scatter_auto_extract_ex(&c, pixels, pix_len, key, key_len, enc_key, msg, msg_len);
    free(allowed);
    return rc;
}

int bmp_capacity(const unsigned char *in, size_t in_len, size_t *bytes) {
    unsigned char *pixels;
    size_t pix_len, stride, rb, rows;
    if (bmp_parse((unsigned char *)in, in_len, &pixels, &pix_len, &stride, &rb, &rows))
        return -1;
    unsigned char *allowed = bmp_allowed(stride, rb, rows);
    if (!allowed)
        return -1;
    carrier_t c = { pixels, pix_len, allowed };
    *bytes = scatter_msg_capacity(&c, 3);
    free(allowed);
    return 0;
}
