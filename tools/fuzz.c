/* Mutation-based fuzzer for the steg parsers.
 *
 * Usage: tools/fuzz <iters> <seedfile>...
 *
 * Reads each seed, mutates it deterministically and exercises every adapter
 * (extract/capacity/embed). Designed to run under ASan/UBSan: any memory
 * error aborts the process and the test harness reports a nonzero exit
 * code. Produces no output on a clean run.
 */

#include "adapters.h"
#include "format.h"
#include "util.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MUT 65536
#define MAX_MSG 64

struct op {
    const char *name;
    int (*extract)(unsigned char *, size_t, const unsigned char *, size_t,
                   const unsigned char *, unsigned char **, size_t *);
    int (*capacity)(const unsigned char *, size_t, size_t *);
    int (*embed)(unsigned char *, size_t, const unsigned char *, size_t,
                 const unsigned char *, size_t, const unsigned char *,
                 unsigned char **, size_t *);
};

static const struct op ops[] = {
    { "bmp",  bmp_extract,  bmp_capacity,  bmp_embed },
    { "png",  png_extract,  png_capacity,  png_embed },
    { "gif",  gif_extract,  gif_capacity,  gif_embed },
    { "jpeg", jpeg_extract, jpeg_capacity, jpeg_embed },
    { "ppm",  ppm_extract,  ppm_capacity,  ppm_embed },
    { "tga",  tga_extract,  tga_capacity,  tga_embed },
    { "tiff", tiff_extract, tiff_capacity, tiff_embed },
    { "ico",  ico_extract,  ico_capacity,  ico_embed },
};
#define NOPS ((size_t)(sizeof(ops) / sizeof(ops[0])))

static uint64_t rng_state;

static uint64_t frng(void) {
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return x;
}

static unsigned char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END)) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0 || sz > (long)MAX_MUT) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    unsigned char *d = (unsigned char *)malloc(sz > 0 ? (size_t)sz : 1);
    if (!d) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(d, 1, (size_t)sz, f);
    fclose(f);
    if (got == 0 && sz > 0) {
        free(d);
        return NULL;
    }
    *len = got;
    return d;
}

static void mutate(const unsigned char *src, size_t slen,
                   unsigned char *dst, size_t *dlen) {
    if (slen == 0 && frng() % 4 == 0) {
        *dlen = 0;
        return;
    }
    unsigned op = (unsigned)(frng() % 8);
    size_t cap = slen < MAX_MUT ? slen : MAX_MUT;
    size_t n = (slen > 0) ? 1 + frng() % cap : 0;
    memcpy(dst, src, n);

    if (n == 0) {
        size_t ins = 1 + frng() % 16;
        size_t i = frng() % 16;
        if (i + ins > MAX_MUT)
            ins = MAX_MUT - i;
        for (size_t j = 0; j < ins; j++)
            dst[i + j] = (unsigned char)frng();
        *dlen = i + ins;
        return;
    }

    switch (op) {
    case 0: {
        size_t p = frng() % n;
        dst[p] ^= (unsigned char)(1u << (frng() % 8));
        break;
    }
    case 1: {
        size_t p = frng() % n;
        dst[p] = (unsigned char)frng();
        break;
    }
    case 2: {
        size_t p = frng() % (n + 1);
        *dlen = p;
        return;
    }
    case 3: {
        size_t h = n < 8 ? n : 8;
        for (size_t b = 0; b < h; b++)
            dst[b] = (unsigned char)frng();
        break;
    }
    case 4: {
        size_t p = frng() % (n + 1);
        size_t ins = 1 + frng() % 32;
        if (n + ins > MAX_MUT)
            ins = MAX_MUT - n;
        memmove(dst + p + ins, dst + p, n - p);
        for (size_t b = 0; b < ins; b++)
            dst[p + b] = (unsigned char)frng();
        n += ins;
        break;
    }
    case 5: {
        size_t a = frng() % n;
        size_t b = a + 1 + frng() % (n - a);
        size_t p = frng() % (n + 1);
        size_t ln = b - a;
        if (n + ln > MAX_MUT)
            ln = MAX_MUT - n;
        if (p > n)
            p = n;
        memmove(dst + p + ln, dst + p, n - p);
        memcpy(dst + p, dst + a, ln);
        n += ln;
        break;
    }
    case 6: {
        if (n < 8)
            break;
        size_t p = frng() % (n - 4);
        uint32_t v = (uint32_t)frng();
        if (frng() & 1) {
            dst[p] = (unsigned char)(v >> 24);
            dst[p + 1] = (unsigned char)(v >> 16);
            dst[p + 2] = (unsigned char)(v >> 8);
            dst[p + 3] = (unsigned char)v;
        } else {
            dst[p] = (unsigned char)v;
            dst[p + 1] = (unsigned char)(v >> 8);
            dst[p + 2] = (unsigned char)(v >> 16);
            dst[p + 3] = (unsigned char)(v >> 24);
        }
        break;
    }
    case 7: {
        break;
    }
    }
    *dlen = n;
}

static void exercise(const unsigned char *d, size_t n) {
    (void)detect_format(d, n);
    static const unsigned char key[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    static const unsigned char msg[MAX_MSG];
    for (size_t k = 0; k < NOPS; k++) {
        unsigned char *copy = (unsigned char *)malloc(n ? n : 1);
        if (!copy)
            exit(2);
        memcpy(copy, d, n);
        unsigned char *m = NULL;
        size_t ml = 0;
        (void)ops[k].extract(copy, n, key, sizeof(key), NULL, &m, &ml);
        free(m);
        size_t cap = 0;
        (void)ops[k].capacity(copy, n, &cap);
        unsigned char *o = NULL;
        size_t ol = 0;
        (void)ops[k].embed(copy, n, msg, sizeof(msg), key, sizeof(key), NULL,
                           &o, &ol);
        if (o && o != copy)
            free(o);
        free(copy);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <iters> <seedfile>...\n", argv[0]);
        return 2;
    }
    long iters = atol(argv[1]);
    if (iters <= 0)
        return 2;

    unsigned char *buf = (unsigned char *)malloc(MAX_MUT);
    if (!buf)
        return 2;
    for (int a = 2; a < argc; a++) {
        size_t slen = 0;
        unsigned char *seed = read_file(argv[a], &slen);
        if (!seed)
            continue;
        rng_state = fnv1a64(seed, slen) ^ 0x9e3779b97f4a7c15ULL;
        for (long it = 0; it < iters; it++) {
            size_t n = 0;
            mutate(seed, slen, buf, &n);
            exercise(buf, n);
        }
        free(seed);
    }
    free(buf);
    return 0;
}