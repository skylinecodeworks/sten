#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "util.h"
#include "deflate.h"

static void wr32le(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static void wr32be(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void write_all(const char *path, const unsigned char *d, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(1);
    }
    if (fwrite(d, 1, n, f) != n) {
        perror(path);
        exit(1);
    }
    fclose(f);
}

static unsigned char noise(int x, int y, int c) {
    return (unsigned char)((x * 31 + y * 17 + c * 47 + x * y + (x ^ y)) & 0xFF);
}

/* --- BMP --- */

static void gen_bmp_ex(const char *path, int w, int h, int bpp) {
    int chan = bpp / 8;
    size_t rowbytes = (size_t)w * (size_t)chan;
    size_t stride = (rowbytes + 3) & ~(size_t)3;
    size_t pixlen = stride * (size_t)h;
    size_t total = 54 + pixlen;
    unsigned char *d = (unsigned char *)calloc(1, total);
    if (!d)
        exit(1);
    d[0] = 'B';
    d[1] = 'M';
    wr32le(d + 2, (uint32_t)total);
    wr32le(d + 10, 54);
    wr32le(d + 14, 40);
    wr32le(d + 18, (uint32_t)w);
    wr32le(d + 22, (uint32_t)h);
    d[26] = 1;
    d[28] = (unsigned char)bpp;
    wr32le(d + 34, (uint32_t)pixlen);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            size_t i = 54 + (size_t)y * stride + (size_t)x * (size_t)chan;
            d[i] = noise(x, y, 0);
            d[i + 1] = noise(x, y, 1);
            d[i + 2] = noise(x, y, 2);
            if (chan == 4)
                d[i + 3] = 0xFF;
        }
    write_all(path, d, total);
    free(d);
}

/* 8-bit paletted BMP: valid as a file, rejected by sten. */
static void gen_bmp_gray(const char *path) {
    int w = 16, h = 16;
    size_t rowbytes = (size_t)w;
    size_t stride = (rowbytes + 3) & ~(size_t)3;
    size_t off = 54 + 256 * 4;
    size_t pixlen = stride * (size_t)h;
    size_t total = off + pixlen;
    unsigned char *d = (unsigned char *)calloc(1, total);
    if (!d)
        exit(1);
    d[0] = 'B';
    d[1] = 'M';
    wr32le(d + 2, (uint32_t)total);
    wr32le(d + 10, (uint32_t)off);
    wr32le(d + 14, 40);
    wr32le(d + 18, (uint32_t)w);
    wr32le(d + 22, (uint32_t)h);
    d[26] = 1;
    d[28] = 8;
    wr32le(d + 34, (uint32_t)pixlen);
    for (int i = 0; i < 256; i++) {
        d[54 + i * 4 + 0] = (unsigned char)i;
        d[54 + i * 4 + 1] = (unsigned char)i;
        d[54 + i * 4 + 2] = (unsigned char)i;
    }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            d[off + (size_t)y * stride + (size_t)x] = noise(x, y, 0);
    write_all(path, d, total);
    free(d);
}

/* --- PNG --- */

static void gen_png_ex(const char *path, int w, int h, int color_type) {
    int chan = color_type == 6 ? 4 : 3;
    size_t stride = 1 + (size_t)w * (size_t)chan;
    size_t rawlen = stride * (size_t)h;
    unsigned char *raw = (unsigned char *)malloc(rawlen);
    if (!raw)
        exit(1);
    for (int y = 0; y < h; y++) {
        size_t row = (size_t)y * stride;
        raw[row] = 0;
        for (int x = 0; x < w; x++) {
            size_t o = row + 1 + (size_t)x * (size_t)chan;
            raw[o] = noise(x, y, 0);
            raw[o + 1] = noise(x, y, 1);
            raw[o + 2] = noise(x, y, 2);
            if (chan == 4)
                raw[o + 3] = 0xFF;
        }
    }
    unsigned char *z = NULL;
    size_t zlen = 0;
    if (deflate_zlib_stored(raw, rawlen, &z, &zlen))
        exit(1);

    size_t total = 8 + 25 + 12 + zlen + 12;
    unsigned char *d = (unsigned char *)calloc(1, total);
    if (!d)
        exit(1);
    size_t n = 0;
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    memcpy(d, sig, 8);
    n = 8;
    wr32be(d + n, 13);
    memcpy(d + n + 4, "IHDR", 4);
    wr32be(d + n + 8, (uint32_t)w);
    wr32be(d + n + 12, (uint32_t)h);
    d[n + 16] = 8;
    d[n + 17] = (unsigned char)color_type;
    wr32be(d + n + 21, crc32(d + n + 4, 17));
    n += 25;
    wr32be(d + n, (uint32_t)zlen);
    memcpy(d + n + 4, "IDAT", 4);
    memcpy(d + n + 8, z, zlen);
    wr32be(d + n + 8 + zlen, crc32(d + n + 4, 4 + zlen));
    n += 12 + zlen;
    wr32be(d + n, 0);
    memcpy(d + n + 4, "IEND", 4);
    wr32be(d + n + 8, crc32(d + n + 4, 4));
    n += 12;
    write_all(path, d, n);
    free(d);
    free(z);
    free(raw);
}

/* Horizontal RGB gradient: very compressible image for phase 1. */
static void gen_png_flat(const char *path, int w, int h) {
    size_t stride = 1 + (size_t)w * 3;
    size_t rawlen = stride * (size_t)h;
    unsigned char *raw = (unsigned char *)malloc(rawlen);
    if (!raw)
        exit(1);
    for (int y = 0; y < h; y++) {
        size_t row = (size_t)y * stride;
        raw[row] = 0;
        for (int x = 0; x < w; x++) {
            size_t o = row + 1 + (size_t)x * 3;
            raw[o] = (unsigned char)((unsigned)(x * 255) / (unsigned)w);
            raw[o + 1] = (unsigned char)((unsigned)(x * 63) / (unsigned)w);
            raw[o + 2] = (unsigned char)((unsigned)((w - 1 - x) * 255) / (unsigned)w);
        }
    }
    unsigned char *z = NULL;
    size_t zlen = 0;
    if (deflate_zlib_stored(raw, rawlen, &z, &zlen))
        exit(1);

    size_t total = 8 + 25 + 12 + zlen + 12;
    unsigned char *d = (unsigned char *)calloc(1, total);
    if (!d)
        exit(1);
    size_t n = 0;
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    memcpy(d, sig, 8);
    n = 8;
    wr32be(d + n, 13);
    memcpy(d + n + 4, "IHDR", 4);
    wr32be(d + n + 8, (uint32_t)w);
    wr32be(d + n + 12, (uint32_t)h);
    d[n + 16] = 8;
    d[n + 17] = 2;
    wr32be(d + n + 21, crc32(d + n + 4, 17));
    n += 25;
    wr32be(d + n, (uint32_t)zlen);
    memcpy(d + n + 4, "IDAT", 4);
    memcpy(d + n + 8, z, zlen);
    wr32be(d + n + 8 + zlen, crc32(d + n + 4, 4 + zlen));
    n += 12 + zlen;
    wr32be(d + n, 0);
    memcpy(d + n + 4, "IEND", 4);
    wr32be(d + n + 8, crc32(d + n + 4, 4));
    n += 12;
    write_all(path, d, n);
    free(d);
    free(z);
    free(raw);
}

/* PNG solo con firma + IHDR: sirve para probar el rechazo por bit depth/color type. */
static void gen_png_header(const char *path, int w, int h, int bit_depth, int color_type) {
    unsigned char d[8 + 25];
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    memcpy(d, sig, 8);
    wr32be(d + 8, 13);
    memcpy(d + 12, "IHDR", 4);
    wr32be(d + 16, (uint32_t)w);
    wr32be(d + 20, (uint32_t)h);
    d[24] = (unsigned char)bit_depth;
    d[25] = (unsigned char)color_type;
    d[26] = 0;
    d[27] = 0;
    d[28] = 0;
    wr32be(d + 29, crc32(d + 12, 17));
    write_all(path, d, sizeof(d));
}

/* --- GIF --- */

static void gen_gif(const char *path) {
    size_t total = 6 + 4 + 3 + 768 + 1 + 8 + 1 + 1 + 1 + 4 + 1 + 1;
    unsigned char *d = (unsigned char *)calloc(1, total);
    if (!d)
        exit(1);
    memcpy(d, "GIF89a", 6);
    d[6] = 1;
    d[8] = 1;
    d[10] = 0x87;
    rng_t rng;
    rng_seed(&rng, 0x123456789ABCDEF0ULL);
    for (int i = 0; i < 256; i++) {
        d[13 + i * 3] = (unsigned char)(rng_next(&rng) >> 32);
        d[14 + i * 3] = (unsigned char)(rng_next(&rng) >> 32);
        d[15 + i * 3] = (unsigned char)(rng_next(&rng) >> 32);
    }
    size_t n = 13 + 768;
    d[n++] = 0x2C;
    d[n++] = 0; d[n++] = 0; d[n++] = 0; d[n++] = 0;
    d[n++] = 1; d[n++] = 0; d[n++] = 1; d[n++] = 0;
    d[n++] = 0;
    d[n++] = 8;
    unsigned codes[3] = { 256, 0, 257 };
    unsigned char bits[4] = { 0, 0, 0, 0 };
    unsigned bitbuf = 0;
    int nb = 0, bn = 0;
    for (int ci = 0; ci < 3; ci++)
        for (int k = 0; k < 9; k++) {
            unsigned bit = (codes[ci] >> k) & 1;
            bitbuf |= bit << nb;
            nb++;
            if (nb == 8) {
                bits[bn++] = (unsigned char)bitbuf;
                bitbuf = 0;
                nb = 0;
            }
        }
    if (nb)
        bits[bn++] = (unsigned char)bitbuf;
    d[n++] = (unsigned char)bn;
    for (int i = 0; i < bn; i++)
        d[n++] = bits[i];
    d[n++] = 0;
    d[n++] = 0x3B;
    write_all(path, d, n);
    free(d);
}

/* GIF without a global color table: rejected by sten. */
static void gen_gif_nopal(const char *path) {
    unsigned char d[14];
    memset(d, 0, sizeof(d));
    memcpy(d, "GIF89a", 6);
    d[6] = 1; d[7] = 0; d[8] = 1; d[9] = 0;
    d[10] = 0x00; /* sin flag de paleta global */
    d[11] = 0; d[12] = 0; d[13] = 0x3B;
    write_all(path, d, sizeof(d));
}

/* --- JPEG --- */

static const unsigned char min_jpeg[] = {
    0xFF, 0xD8,
    0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0xFF, 0xDB, 0x00, 0x43, 0x00,
    0x08, 0x06, 0x06, 0x07, 0x06, 0x05, 0x08, 0x07, 0x07, 0x07, 0x09, 0x09, 0x08, 0x0A, 0x0C, 0x14,
    0x0D, 0x0C, 0x0B, 0x0B, 0x0C, 0x19, 0x12, 0x13, 0x0F, 0x14, 0x1D, 0x1A, 0x1F, 0x1E, 0x1D, 0x1A,
    0x1C, 0x1C, 0x20, 0x24, 0x2E, 0x27, 0x20, 0x22, 0x2C, 0x23, 0x1C, 0x1C, 0x28, 0x37, 0x29, 0x2C,
    0x30, 0x31, 0x34, 0x34, 0x34, 0x1F, 0x27, 0x39, 0x3D, 0x38, 0x32, 0x3C, 0x2E, 0x33, 0x34, 0x32,
    0xFF, 0xC0, 0x00, 0x0B, 0x08, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x11, 0x00,
    0xFF, 0xC4, 0x00, 0x1F, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0xFF, 0xC4, 0x00, 0xB5, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04,
    0x00, 0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13,
    0x51, 0x61, 0x07, 0x71, 0x91, 0x32, 0x81, 0x01, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1,
    0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
    0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73,
    0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93,
    0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2,
    0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA,
    0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8,
    0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA,
    0xFF, 0xC4, 0x00, 0x1F, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0xFF, 0xDA, 0x00, 0x0C, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3F, 0x00,
    0x1B, 0x86,
    0xFF, 0xD9
};

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s DIR\n", argv[0]);
        return 1;
    }
    char path[1024];

    snprintf(path, sizeof(path), "%s/test.bmp", argv[1]);
    gen_bmp_ex(path, 64, 48, 24);
    snprintf(path, sizeof(path), "%s/test32.bmp", argv[1]);
    gen_bmp_ex(path, 64, 48, 32);
    snprintf(path, sizeof(path), "%s/big.bmp", argv[1]);
    gen_bmp_ex(path, 256, 256, 24);
    snprintf(path, sizeof(path), "%s/gray8.bmp", argv[1]);
    gen_bmp_gray(path);

    snprintf(path, sizeof(path), "%s/test.png", argv[1]);
    gen_png_ex(path, 64, 64, 2);
    snprintf(path, sizeof(path), "%s/rgba.png", argv[1]);
    gen_png_ex(path, 64, 64, 6);
    snprintf(path, sizeof(path), "%s/big.png", argv[1]);
    gen_png_ex(path, 256, 256, 2);
    snprintf(path, sizeof(path), "%s/tiny.png", argv[1]);
    gen_png_ex(path, 1, 1, 2);
    snprintf(path, sizeof(path), "%s/pal.png", argv[1]);
    gen_png_header(path, 16, 16, 8, 3);
    snprintf(path, sizeof(path), "%s/gray16.png", argv[1]);
    gen_png_header(path, 16, 16, 16, 0);
    snprintf(path, sizeof(path), "%s/flat.png", argv[1]);
    gen_png_flat(path, 64, 64);

    snprintf(path, sizeof(path), "%s/test.gif", argv[1]);
    gen_gif(path);
    snprintf(path, sizeof(path), "%s/nopal.gif", argv[1]);
    gen_gif_nopal(path);

    snprintf(path, sizeof(path), "%s/test.jpg", argv[1]);
    write_all(path, min_jpeg, sizeof(min_jpeg));

    return 0;
}
