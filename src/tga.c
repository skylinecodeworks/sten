#include "adapters.h"
#include "scatter.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Uncompressed TGA type 2 (truecolor 24/32-bit) and type 3 (grayscale 8-bit).
 * Pixels are edited in place and the file keeps its size. */

static int tga_parse(const unsigned char *in, size_t len,
                     unsigned char **pixels, size_t *pix_len,
                     long *w_out, long *h_out, int *ch_out) {
    if (len < 18 || in[1] != 0) {
        fprintf(stderr, "error: invalid TGA (expected no color map)\n");
        return -1;
    }
    unsigned type = in[2];
    size_t bpp = in[16];
    if (type == 2) {
        if (bpp != 24 && bpp != 32) {
            fprintf(stderr, "error: TGA truecolor must be 24 or 32 bits\n");
            return -1;
        }
    } else if (type == 3) {
        if (bpp != 8) {
            fprintf(stderr, "error: TGA grayscale must be 8 bits\n");
            return -1;
        }
    } else {
        fprintf(stderr, "error: only uncompressed TGA (type 2/3) is supported\n");
        return -1;
    }
    size_t w = (size_t)in[12] | ((size_t)in[13] << 8);
    size_t h = (size_t)in[14] | ((size_t)in[15] << 8);
    if (w == 0 || h == 0) {
        fprintf(stderr, "error: invalid TGA dimensions\n");
        return -1;
    }
    size_t start = 18 + (size_t)in[0];
    if (w > SIZE_MAX / h / (bpp / 8)) {
        fprintf(stderr, "error: TGA dimensions too large\n");
        return -1;
    }
    size_t need = w * h * (bpp / 8);
    if (start > len || len - start < need) {
        fprintf(stderr, "error: TGA pixel data out of range\n");
        return -1;
    }
    *pixels = (unsigned char *)in + start;
    *pix_len = need;
    if (w_out)
        *w_out = (long)w;
    if (h_out)
        *h_out = (long)h;
    if (ch_out)
        *ch_out = (int)(bpp / 8);
    return 0;
}

int tga_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key, size_t enc_key_len,
              unsigned char **out, size_t *out_len) {
    unsigned char *pixels;
    size_t pix_len;
    if (tga_parse(in, in_len, &pixels, &pix_len, NULL, NULL, NULL))
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

int tga_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, size_t enc_key_len, unsigned char **msg, size_t *msg_len) {
    unsigned char *pixels;
    size_t pix_len;
    if (tga_parse(in, in_len, &pixels, &pix_len, NULL, NULL, NULL))
        return -1;
    carrier_t c = { pixels, pix_len, NULL };
    return scatter_auto_extract_ex(&c, pixels, pix_len, key, key_len, enc_key, enc_key_len, msg, msg_len);
}

int tga_capacity(const unsigned char *in, size_t in_len, size_t *bytes) {
    unsigned char *pixels;
    size_t pix_len;
    if (tga_parse(in, in_len, &pixels, &pix_len, NULL, NULL, NULL))
        return -1;
    carrier_t c = { pixels, pix_len, NULL };
    *bytes = scatter_msg_capacity(&c, 3);
    return 0;
}
int tga_inspect(const unsigned char *in, size_t in_len, sten_info_t *info) {
    unsigned char *pixels;
    size_t pix_len;
    long w = 0, h = 0;
    int ch = 0;
    if (tga_parse(in, in_len, &pixels, &pix_len, &w, &h, &ch))
        return -1;
    info->w = w;
    info->h = h;
    info->channels = ch;
    info->bits = 8;
    info->has_dims = 1;
    return 0;
}
