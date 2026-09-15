#include "adapters.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ICO files: the embedded data rides in the first entry whose image payload is
 * a PNG (reusing the PNG adapter). The header and entries are rebuilt, so the
 * file size may change. */

static int ico_find_png(const unsigned char *in, size_t len,
                        unsigned *entry_idx, unsigned char **png, size_t *png_len) {
    if (len < 6 || in[0] != 0 || in[1] != 0) {
        fprintf(stderr, "error: invalid ICO header\n");
        return -1;
    }
    unsigned type = (unsigned)in[2] | ((unsigned)in[3] << 8);
    unsigned count = (unsigned)in[4] | ((unsigned)in[5] << 8);
    if (type != 1) {
        fprintf(stderr, "error: only ICO (icon) files are supported\n");
        return -1;
    }
    if (count == 0) {
        fprintf(stderr, "error: ICO contains no entries\n");
        return -1;
    }
    if (len < 6 + (size_t)count * 16) {
        fprintf(stderr, "error: ICO directory truncated\n");
        return -1;
    }
    for (unsigned i = 0; i < count; i++) {
        const unsigned char *e = in + 6 + (size_t)i * 16;
        unsigned long bytes_in_res = (unsigned long)e[8] | ((unsigned long)e[9] << 8) |
                                     ((unsigned long)e[10] << 16) | ((unsigned long)e[11] << 24);
        unsigned long off = (unsigned long)e[12] | ((unsigned long)e[13] << 8) |
                            ((unsigned long)e[14] << 16) | ((unsigned long)e[15] << 24);
        if (off > len || bytes_in_res > len - off) {
            fprintf(stderr, "error: ICO entry out of range\n");
            return -1;
        }
        if (bytes_in_res >= 8 && !memcmp(in + off, "\x89PNG\r\n\x1a\n", 8)) {
            *entry_idx = i;
            *png = (unsigned char *)in + off;
            *png_len = (size_t)bytes_in_res;
            return 0;
        }
    }
    fprintf(stderr, "error: ICO contains no PNG data\n");
    return -1;
}

static void w_le32(unsigned char *p, unsigned long v) {
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

int ico_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key, size_t enc_key_len,
              unsigned char **out, size_t *out_len) {
    unsigned idx;
    unsigned char *png;
    size_t png_len;
    if (ico_find_png(in, in_len, &idx, &png, &png_len))
        return -1;

    unsigned char *new_png;
    size_t new_png_len;
    if (png_embed(png, png_len, msg, msg_len, key, key_len, enc_key, enc_key_len,
                  &new_png, &new_png_len))
        return -1;

    unsigned count = (unsigned)in[4] | ((unsigned)in[5] << 8);
    size_t hdr_len = 6 + (size_t)count * 16;
    size_t olen = hdr_len;
    for (unsigned i = 0; i < count; i++) {
        const unsigned char *e = in + 6 + (size_t)i * 16;
        unsigned long b = (unsigned long)e[8] | ((unsigned long)e[9] << 8) |
                          ((unsigned long)e[10] << 16) | ((unsigned long)e[11] << 24);
        olen += (i == idx) ? new_png_len : (size_t)b;
    }
    unsigned char *o = (unsigned char *)malloc(olen);
    if (!o) {
        free(new_png);
        fprintf(stderr, "error: out of memory\n");
        return -1;
    }
    memcpy(o, in, hdr_len);
    size_t run = hdr_len;
    for (unsigned i = 0; i < count; i++) {
        const unsigned char *e = in + 6 + (size_t)i * 16;
        unsigned long b = (unsigned long)e[8] | ((unsigned long)e[9] << 8) |
                          ((unsigned long)e[10] << 16) | ((unsigned long)e[11] << 24);
        unsigned long off = (unsigned long)e[12] | ((unsigned long)e[13] << 8) |
                            ((unsigned long)e[14] << 16) | ((unsigned long)e[15] << 24);
        unsigned char *oe = o + 6 + (size_t)i * 16;
        memcpy(oe, e, 8);
        unsigned long nb = (i == idx) ? (unsigned long)new_png_len : b;
        w_le32(oe + 8, nb);
        w_le32(oe + 12, (unsigned long)run);
        const unsigned char *src = (i == idx) ? new_png : in + off;
        memcpy(o + run, src, nb);
        run += (size_t)nb;
    }
    free(new_png);
    *out = o;
    *out_len = olen;
    return 0;
}

int ico_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, size_t enc_key_len, unsigned char **msg, size_t *msg_len) {
    unsigned idx;
    unsigned char *png;
    size_t png_len;
    if (ico_find_png(in, in_len, &idx, &png, &png_len))
        return -1;
    (void)idx;
    return png_extract(png, png_len, key, key_len, enc_key, enc_key_len, msg, msg_len);
}

int ico_capacity(const unsigned char *in, size_t in_len, size_t *bytes) {
    unsigned idx;
    unsigned char *png;
    size_t png_len;
    if (ico_find_png(in, in_len, &idx, &png, &png_len))
        return -1;
    (void)idx;
    return png_capacity(png, png_len, bytes);
}
int ico_inspect(const unsigned char *in, size_t in_len, sten_info_t *info) {
    unsigned entry_idx;
    unsigned char *png;
    size_t png_len;
    if (ico_find_png(in, in_len, &entry_idx, &png, &png_len))
        return -1;
    uint32_t w, h;
    unsigned ct;
    info->has_dims = 0;
    if (png_info(png, png_len, &w, &h, &ct) == 0) {
        info->w = (long)w;
        info->h = (long)h;
        info->channels = ct == 6 ? 4 : 3;
        info->bits = 8;
        info->has_dims = 1;
    }
    return 0;
}
