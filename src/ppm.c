#include "adapters.h"
#include "scatter.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Netpbm: binary PPM (P6), PGM (P5) and PAM (P7), 8-bit. Pixels are edited
 * in place and the file keeps its size. */

/* Parses an integer from a not-necessarily-NUL-terminated PAM header line.
 * Returns -1 when no digits / value out of range. */
static long pam_num(const unsigned char *s, size_t n) {
    size_t i = 0;
    int neg = 0;
    long v = 0;
    if (n > 0 && s[0] == '-') {
        neg = 1;
        i = 1;
    }
    int have = 0;
    for (; i < n; i++) {
        if (s[i] < '0' || s[i] > '9')
            break;
        have = 1;
        if (v > 100000)
            return -1;
        v = v * 10 + (s[i] - '0');
    }
    if (!have)
        return -1;
    return neg ? -v : v;
}

static int netpbm_parse(const unsigned char *in, size_t len,
                        unsigned char **pixels, size_t *pix_len,
                        long *w_out, long *h_out, int *ch_out) {
    if (len < 2 || in[0] != 'P') {
        fprintf(stderr, "error: invalid Netpbm image\n");
        return -1;
    }
    char magic = (char)in[1];
    if (magic == '1' || magic == '2' || magic == '3' || magic == '4') {
        fprintf(stderr, "error: only binary 8-bit Netpbm images are supported\n");
        return -1;
    }
    if (magic == '5' || magic == '6') {
        size_t p = 2;
        long w = 0, h = 0, maxval = -1;
        int tok = 0;
        while (p < len && tok < 3) {
            unsigned char c = in[p];
            if (c == '#' && tok != 0 && (p == 0 || in[p - 1] == '\n')) {
                while (p < len && in[p] != '\n')
                    p++;
                continue;
            }
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                p++;
                continue;
            }
            long val = 0;
            int have = 0;
            while (p < len && in[p] >= '0' && in[p] <= '9') {
                val = val * 10 + (in[p] - '0');
                if (val > 100000) {
                    fprintf(stderr, "error: invalid Netpbm header\n");
                    return -1;
                }
                p++;
                have = 1;
            }
            if (!have) {
                fprintf(stderr, "error: invalid Netpbm header\n");
                return -1;
            }
            if (tok == 0) w = val;
            else if (tok == 1) h = val;
            else maxval = val;
            tok++;
        }
        if (w <= 0 || h <= 0 || maxval != 255) {
            fprintf(stderr, "error: Netpbm must be 8-bit (maxval 255)\n");
            return -1;
        }
        if (p >= len) {
            fprintf(stderr, "error: Netpbm header truncated\n");
            return -1;
        }
        if (in[p] != ' ' && in[p] != '\t' && in[p] != '\r' && in[p] != '\n') {
            fprintf(stderr, "error: invalid Netpbm header\n");
            return -1;
        }
        p++;
        size_t mul = magic == '6' ? 3 : 1;
        if ((size_t)w > SIZE_MAX / (size_t)h / mul) {
            fprintf(stderr, "error: Netpbm dimensions too large\n");
            return -1;
        }
        size_t need = (size_t)w * (size_t)h * mul;
        if (len - p < need) {
            fprintf(stderr, "error: Netpbm pixel data out of range\n");
            return -1;
        }
        *pixels = (unsigned char *)in + p;
        *pix_len = need;
        if (w_out)
            *w_out = w;
        if (h_out)
            *h_out = h;
        if (ch_out)
            *ch_out = magic == '6' ? 3 : 1;
        return 0;
    }
    if (magic == '7') {
        size_t p = 2;
        long w = 0, h = 0, depth = 0, maxval = 0;
        int got = 0;
        for (;;) {
            if (p >= len) {
                fprintf(stderr, "error: PAM header truncated\n");
                return -1;
            }
            size_t line_start = p;
            while (p < len && in[p] != '\n')
                p++;
            size_t llen = p - line_start;
            if (p >= len && llen == 0)
                break;
            if (p < len)
                p++;
            if (llen >= 6 && !memcmp(in + line_start, "ENDHDR", 6)) {
                got = 1;
                break;
            }
            if (llen >= 5 && !memcmp(in + line_start, "WIDTH", 5) && llen > 6)
                w = pam_num(in + line_start + 6, llen - 6);
            else if (llen >= 6 && !memcmp(in + line_start, "HEIGHT", 6) && llen > 7)
                h = pam_num(in + line_start + 7, llen - 7);
            else if (llen >= 5 && !memcmp(in + line_start, "DEPTH", 5) && llen > 6)
                depth = pam_num(in + line_start + 6, llen - 6);
            else if (llen >= 6 && !memcmp(in + line_start, "MAXVAL", 6) && llen > 7)
                maxval = pam_num(in + line_start + 7, llen - 7);
        }
        if (!got || w <= 0 || h <= 0 || depth < 1 || depth > 4 || maxval != 255) {
            fprintf(stderr, "error: PAM must be 8-bit, depth 1-4, maxval 255\n");
            return -1;
        }
        if (w > 100000 || h > 100000) {
            fprintf(stderr, "error: PAM dimensions too large\n");
            return -1;
        }
        size_t need = (size_t)w * (size_t)h * (size_t)depth;
        if (len - p < need) {
            fprintf(stderr, "error: PAM pixel data out of range\n");
            return -1;
        }
        *pixels = (unsigned char *)in + p;
        *pix_len = need;
        if (w_out)
            *w_out = w;
        if (h_out)
            *h_out = h;
        if (ch_out)
            *ch_out = (int)depth;
        return 0;
    }
    fprintf(stderr, "error: unsupported Netpbm format\n");
    return -1;
}

int ppm_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key,
              unsigned char **out, size_t *out_len) {
    unsigned char *pixels;
    size_t pix_len;
    if (netpbm_parse(in, in_len, &pixels, &pix_len, NULL, NULL, NULL))
        return -1;
    carrier_t c = { pixels, pix_len, NULL };
    int rc = scatter_embed_ex(&c, pixels, pix_len, msg, msg_len, key, key_len, 3, enc_key);
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

int ppm_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, unsigned char **msg, size_t *msg_len) {
    unsigned char *pixels;
    size_t pix_len;
    if (netpbm_parse(in, in_len, &pixels, &pix_len, NULL, NULL, NULL))
        return -1;
    carrier_t c = { pixels, pix_len, NULL };
    return scatter_auto_extract_ex(&c, pixels, pix_len, key, key_len, enc_key, msg, msg_len);
}

int ppm_capacity(const unsigned char *in, size_t in_len, size_t *bytes) {
    unsigned char *pixels;
    size_t pix_len;
    if (netpbm_parse(in, in_len, &pixels, &pix_len, NULL, NULL, NULL))
        return -1;
    carrier_t c = { pixels, pix_len, NULL };
    *bytes = scatter_msg_capacity(&c, 3);
    return 0;
}
int ppm_inspect(const unsigned char *in, size_t in_len, sten_info_t *info) {
    unsigned char *pixels;
    size_t pix_len;
    long w = 0, h = 0;
    int ch = 0;
    if (netpbm_parse(in, in_len, &pixels, &pix_len, &w, &h, &ch))
        return -1;
    info->w = w;
    info->h = h;
    info->channels = ch;
    info->bits = 8;
    info->has_dims = 1;
    return 0;
}
