#include "adapters.h"
#include "deflate.h"
#include "scatter.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd32be(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void wr32be(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static int buf_append(unsigned char **buf, size_t *cap, size_t *len,
                      const unsigned char *src, size_t n) {
    if (*len + n > *cap) {
        size_t nc = *cap ? *cap : 1024;
        while (nc < *len + n)
            nc *= 2;
        unsigned char *nd = (unsigned char *)realloc(*buf, nc);
        if (!nd)
            return -1;
        *buf = nd;
        *cap = nc;
    }
    memcpy(*buf + *len, src, n);
    *len += n;
    return 0;
}

/* Splits a PNG into: pre (signature + non-IDAT non-IEND chunks),
 * idat (concatenated compressed data) and iend (final chunk). */
static int png_load(const unsigned char *in, size_t len,
                    uint32_t *w, uint32_t *h, unsigned *color_type,
                    unsigned char **pre, size_t *pre_len,
                    unsigned char **idat, size_t *idat_len,
                    unsigned char **iend, size_t *iend_len) {
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    if (len < 8 + 12 || memcmp(in, sig, 8)) {
        fprintf(stderr, "error: invalid PNG\n");
        return -1;
    }
    size_t pre_cap = 4096, idat_cap = 4096;
    unsigned char *p = (unsigned char *)malloc(pre_cap);
    unsigned char *id = (unsigned char *)malloc(idat_cap);
    unsigned char *ie = NULL;
    size_t p_len = 0, id_len = 0, ie_len = 0;
    int got_ihdr = 0, got_idat = 0, got_iend = 0, ok = -1;

    if (!p || !id)
        goto done;

    memcpy(p, in, 8);
    p_len = 8;

    size_t pos = 8;
    while (pos + 12 <= len) {
        uint32_t clen = rd32be(in + pos);
        const unsigned char *type = in + pos + 4;
        if (pos + 12 + (uint64_t)clen > len)
            goto done;
        if (!memcmp(type, "IHDR", 4)) {
            if (clen != 13)
                goto done;
            *w = rd32be(in + pos + 8);
            *h = rd32be(in + pos + 12);
            unsigned bd = in[pos + 16];
            unsigned ct = in[pos + 17];
            unsigned comp = in[pos + 18];
            unsigned filt = in[pos + 19];
            unsigned inter = in[pos + 20];
            if (bd != 8 || (ct != 2 && ct != 6) || comp != 0 || filt != 0 || inter != 0) {
                fprintf(stderr, "error: PNG must be 8-bit RGB/RGBA without interlacing\n");
                goto done;
            }
            *color_type = ct;
            got_ihdr = 1;
            if (buf_append(&p, &pre_cap, &p_len, in + pos, 12 + clen))
                goto done;
        } else if (!memcmp(type, "IDAT", 4)) {
            got_idat = 1;
            if (buf_append(&id, &idat_cap, &id_len, in + pos + 8, clen))
                goto done;
        } else if (!memcmp(type, "IEND", 4)) {
            free(ie);
            ie = (unsigned char *)malloc(12 + clen);
            if (!ie)
                goto done;
            memcpy(ie, in + pos, 12 + clen);
            ie_len = 12 + clen;
            got_iend = 1;
        } else {
            if (buf_append(&p, &pre_cap, &p_len, in + pos, 12 + clen))
                goto done;
        }
        pos += 12 + clen;
    }
    if (!got_ihdr || !got_idat || !got_iend) {
        fprintf(stderr, "error: incomplete PNG\n");
        goto done;
    }
    *pre = p;
    *pre_len = p_len;
    *idat = id;
    *idat_len = id_len;
    *iend = ie;
    *iend_len = ie_len;
    ok = 0;

done:
    if (ok != 0) {
        free(p);
        free(id);
        free(ie);
    }
    return ok;
}

static unsigned char *png_allowed(size_t raw_len, size_t stride, uint32_t h) {
    unsigned char *a = (unsigned char *)malloc((raw_len + 7) / 8);
    if (!a)
        return NULL;
    memset(a, 0xFF, (raw_len + 7) / 8);
    for (uint32_t y = 0; y < h; y++) {
        size_t i = (size_t)y * stride;
        a[i >> 3] &= (unsigned char)~(1u << (i & 7));
    }
    return a;
}

static unsigned char paeth(unsigned char a, unsigned char b, unsigned char c) {
    int p = (int)a + b - c;
    int pa = p - a; if (pa < 0) pa = -pa;
    int pb = p - b; if (pb < 0) pb = -pb;
    int pc = p - c; if (pc < 0) pc = -pc;
    if (pa <= pb && pa <= pc)
        return a;
    if (pb <= pc)
        return b;
    return c;
}

/* Converts a filtered row to raw pixels (in-place, left to right).
 * prev = previous row already in the raw domain, or NULL (zeros). */
static void png_unfilter_row(unsigned char *row, const unsigned char *prev,
                             size_t pxbytes, size_t bpp) {
    switch (row[-1]) {
    case 0:
        return;
    case 1:
        for (size_t i = bpp; i < pxbytes; i++)
            row[i] = (unsigned char)(row[i] + row[i - bpp]);
        return;
    case 2:
        for (size_t i = 0; i < pxbytes; i++)
            row[i] = (unsigned char)(row[i] + (prev ? prev[i] : 0));
        return;
    case 3:
        for (size_t i = 0; i < pxbytes; i++) {
            unsigned char left = i >= bpp ? row[i - bpp] : 0;
            unsigned char up = prev ? prev[i] : 0;
            row[i] = (unsigned char)(row[i] + ((unsigned)left + up) / 2);
        }
        return;
    case 4:
        for (size_t i = 0; i < pxbytes; i++) {
            unsigned char left = i >= bpp ? row[i - bpp] : 0;
            unsigned char up = prev ? prev[i] : 0;
            unsigned char ul = (prev && i >= bpp) ? prev[i - bpp] : 0;
            row[i] = (unsigned char)(row[i] + paeth(left, up, ul));
        }
        return;
    default:
        return;
    }
}

/* Converts raw pixels into a filtered row (in-place, right to left).
 * prev = previous row in the raw domain, or NULL (zeros). */
static void png_refilter_row(unsigned char *row, const unsigned char *prev,
                             size_t pxbytes, size_t bpp) {
    size_t i = pxbytes;
    switch (row[-1]) {
    case 0:
        return;
    case 1:
        while (i > bpp) {
            i--;
            row[i] = (unsigned char)(row[i] - row[i - bpp]);
        }
        return;
    case 2:
        while (i > 0) {
            i--;
            row[i] = (unsigned char)(row[i] - (prev ? prev[i] : 0));
        }
        return;
    case 3:
        while (i > 0) {
            i--;
            unsigned char left = i >= bpp ? row[i - bpp] : 0;
            unsigned char up = prev ? prev[i] : 0;
            row[i] = (unsigned char)(row[i] - ((unsigned)left + up) / 2);
        }
        return;
    case 4:
        while (i > 0) {
            i--;
            unsigned char left = i >= bpp ? row[i - bpp] : 0;
            unsigned char up = prev ? prev[i] : 0;
            unsigned char ul = (prev && i >= bpp) ? prev[i - bpp] : 0;
            row[i] = (unsigned char)(row[i] - paeth(left, up, ul));
        }
        return;
    default:
        return;
    }
}

static int png_to_raw(unsigned char *buf, uint32_t h, size_t stride, size_t bpp) {
    size_t pxbytes = stride - 1;
    for (uint32_t y = 0; y < h; y++) {
        unsigned char *row = buf + (size_t)y * stride;
        if (row[0] > 4) {
            fprintf(stderr, "error: unknown PNG filter (%u)\n", row[0]);
            return -1;
        }
        png_unfilter_row(row + 1, y ? row - stride + 1 : NULL, pxbytes, bpp);
    }
    return 0;
}

static int png_to_filtered(unsigned char *buf, uint32_t h, size_t stride, size_t bpp) {
    size_t pxbytes = stride - 1;
    unsigned char *prev = (unsigned char *)malloc(pxbytes ? pxbytes : 1);
    unsigned char *cur = (unsigned char *)malloc(pxbytes ? pxbytes : 1);
    if (!prev || !cur) {
        free(prev);
        free(cur);
        return -1;
    }
    for (uint32_t y = 0; y < h; y++) {
        unsigned char *row = buf + (size_t)y * stride;
        memcpy(cur, row + 1, pxbytes);
        png_refilter_row(row + 1, y ? prev : NULL, pxbytes, bpp);
        memcpy(prev, cur, pxbytes);
    }
    free(prev);
    free(cur);
    return 0;
}

int png_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, unsigned char **out, size_t *out_len) {
    uint32_t w, h;
    unsigned ct;
    unsigned char *pre, *idat, *iend;
    size_t pre_len, idat_len, iend_len;
    if (png_load(in, in_len, &w, &h, &ct, &pre, &pre_len, &idat, &idat_len, &iend, &iend_len))
        return -1;

    size_t stride = 1 + (size_t)w * (ct == 6 ? 4 : 3);
    size_t bpp = ct == 6 ? 4 : 3;
    size_t expected = stride * (size_t)h;
    unsigned char *raw = NULL;
    size_t raw_len = 0;
    if (inflate_zlib(idat, idat_len, &raw, &raw_len, expected)) {
        fprintf(stderr, "error: could not decompress the IDAT\n");
        free(pre);
        free(idat);
        free(iend);
        return -1;
    }
    free(idat);

    if (png_to_raw(raw, h, stride, bpp)) {
        free(raw);
        free(pre);
        free(iend);
        return -1;
    }

    unsigned char *allowed = png_allowed(raw_len, stride, h);
    if (!allowed) {
        fprintf(stderr, "error: out of memory\n");
        free(raw);
        free(pre);
        free(iend);
        return -1;
    }
    carrier_t c = { raw, raw_len, allowed };
    int rc = scatter_embed(&c, raw, raw_len, msg, msg_len, key, key_len, 3);
    free(allowed);
    if (rc == -1) {
        fprintf(stderr, "error: message too large for this image\n");
        free(raw);
        free(pre);
        free(iend);
        return -1;
    }
    if (rc == -2) {
        fprintf(stderr, "error: internal ScatterBit failure\n");
        free(raw);
        free(pre);
        free(iend);
        return -1;
    }

    if (png_to_filtered(raw, h, stride, bpp)) {
        fprintf(stderr, "error: out of memory\n");
        free(raw);
        free(pre);
        free(iend);
        return -1;
    }

    unsigned char *zdata;
    size_t zlen;
    if (deflate_zlib_stored(raw, raw_len, &zdata, &zlen)) {
        fprintf(stderr, "error: could not compress the IDAT\n");
        free(raw);
        free(pre);
        free(iend);
        return -1;
    }
    free(raw);

    size_t olen = pre_len + 12 + zlen + iend_len;
    unsigned char *o = (unsigned char *)malloc(olen);
    if (!o) {
        fprintf(stderr, "error: out of memory\n");
        free(zdata);
        free(pre);
        free(iend);
        return -1;
    }
    size_t n = 0;
    memcpy(o + n, pre, pre_len);
    n += pre_len;
    wr32be(o + n, (uint32_t)zlen);
    memcpy(o + n + 4, "IDAT", 4);
    memcpy(o + n + 8, zdata, zlen);
    uint32_t ck = crc32(o + n + 4, 4 + zlen);
    wr32be(o + n + 8 + zlen, ck);
    n += 12 + zlen;
    memcpy(o + n, iend, iend_len);
    n += iend_len;

    free(zdata);
    free(pre);
    free(iend);
    *out = o;
    *out_len = n;
    return 0;
}

int png_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                unsigned char **msg, size_t *msg_len) {
    uint32_t w, h;
    unsigned ct;
    unsigned char *pre, *idat, *iend;
    size_t pre_len, idat_len, iend_len;
    if (png_load(in, in_len, &w, &h, &ct, &pre, &pre_len, &idat, &idat_len, &iend, &iend_len))
        return -1;

    size_t stride = 1 + (size_t)w * (ct == 6 ? 4 : 3);
    size_t bpp = ct == 6 ? 4 : 3;
    size_t expected = stride * (size_t)h;
    unsigned char *raw = NULL;
    size_t raw_len = 0;
    if (inflate_zlib(idat, idat_len, &raw, &raw_len, expected)) {
        fprintf(stderr, "error: could not decompress the IDAT\n");
        free(pre);
        free(idat);
        free(iend);
        return -1;
    }

    if (png_to_raw(raw, h, stride, bpp)) {
        free(raw);
        free(pre);
        free(idat);
        free(iend);
        return -1;
    }

    unsigned char *allowed = png_allowed(raw_len, stride, h);
    if (!allowed) {
        free(raw);
        free(pre);
        free(idat);
        free(iend);
        return -1;
    }

    carrier_t c = { raw, raw_len, allowed };
    int rc = scatter_auto_extract(&c, raw, raw_len, key, key_len, msg, msg_len);

    free(allowed);
    free(raw);
    free(pre);
    free(idat);
    free(iend);
    return rc;
}
