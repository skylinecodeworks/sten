/* Unit tests for util, deflate and scatter.
 * Prints TAP-style lines ("ok name" / "not ok name -- reason")
 * which tests/run.sh integrates into the overall summary. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "deflate.h"
#include "scatter.h"
#include "util.h"

static int passed = 0;
static int failed = 0;

static void ok(const char *name) {
    printf("ok %s\n", name);
    passed++;
}

static void ko(const char *name, const char *why) {
    printf("not ok %s -- %s\n", name, why);
    failed++;
}

static void check(const char *name, int cond) {
    if (cond)
        ok(name);
    else
        ko(name, "condition false");
}

static void check_u64(const char *name, uint64_t a, uint64_t b) {
    if (a == b) {
        ok(name);
    } else {
        char m[160];
        snprintf(m, sizeof(m), "0x%llx != 0x%llx",
                 (unsigned long long)a, (unsigned long long)b);
        ko(name, m);
    }
}

static void check_u32(const char *name, uint32_t a, uint32_t b) {
    check_u64(name, a, b);
}

static void fill(unsigned char *p, size_t n, uint32_t seed) {
    rng_t r;
    rng_seed(&r, seed);
    for (size_t i = 0; i < n; i++)
        p[i] = (unsigned char)(rng_next(&r) >> 32);
}

static void test_hashes(void) {
    const unsigned char *d = (const unsigned char *)"123456789";
    check_u32("crc32(\"123456789\")", crc32(d, 9), 0xCBF43926u);
    check_u32("crc32(\"\")", crc32((const unsigned char *)"", 0), 0u);

    const unsigned char *w = (const unsigned char *)"Wikipedia";
    check_u32("adler32(\"Wikipedia\")", adler32(w, 9), 0x11E60398u);
    check_u32("adler32(\"\")", adler32((const unsigned char *)"", 0), 1u);

    check_u64("fnv1a64(\"\")", fnv1a64((const unsigned char *)"", 0),
              0xcbf29ce484222325ULL);
    check_u64("fnv1a64(\"a\")", fnv1a64((const unsigned char *)"a", 1),
              0xaf63dc4c8601ec8cULL);
}

static void test_rng(void) {
    rng_t a, b, c;
    rng_seed(&a, 12345);
    rng_seed(&b, 12345);
    rng_seed(&c, 99999);

    int same = 1, diff = 0;
    for (int i = 0; i < 8; i++) {
        uint64_t x = rng_next(&a);
        uint64_t y = rng_next(&b);
        uint64_t z = rng_next(&c);
        if (x != y)
            same = 0;
        if (x != z)
            diff = 1;
    }
    check("rng: same seed, same sequence", same);
    check("rng: different seeds, different sequences", diff);

    rng_t z0;
    rng_seed(&z0, 0);
    check("rng: seed 0 is replaced by a constant", z0.s != 0);
}

static void test_deflate_roundtrip(size_t n, const char *label) {
    unsigned char *data = (unsigned char *)malloc(n ? n : 1);
    if (!data)
        exit(2);
    fill(data, n, (uint32_t)(n * 2654435761u + 1));

    unsigned char *z = NULL;
    size_t zlen = 0;
    if (deflate_zlib_stored(data, n, &z, &zlen) != 0) {
        char m[128];
        snprintf(m, sizeof(m), "deflate failed n=%zu", n);
        ko(label, m);
        free(data);
        return;
    }

    unsigned char *out = NULL;
    size_t olen = 0;
    if (inflate_zlib(z, zlen, &out, &olen, n) != 0) {
        char m[128];
        snprintf(m, sizeof(m), "inflate failed n=%zu", n);
        ko(label, m);
        free(z);
        free(data);
        return;
    }
    int eq = (olen == n) && (n == 0 || memcmp(out, data, n) == 0);
    check(label, eq);
    free(out);
    free(z);
    free(data);
}

static void test_deflate_errors(void) {
    unsigned char data[200];
    fill(data, sizeof(data), 7);
    unsigned char *z = NULL;
    size_t zlen = 0;
    if (deflate_zlib_stored(data, sizeof(data), &z, &zlen) != 0) {
        ko("deflate rejects truncated stream", "could not compress");
        return;
    }

    unsigned char *out = NULL;
    size_t olen = 0;
    check("inflate rejects truncated stream",
          inflate_zlib(z, 16, &out, &olen, 0) != 0);

    out = NULL;
    olen = 0;
    check("inflate rejects different expected size",
          inflate_zlib(z, zlen, &out, &olen, sizeof(data) + 1) != 0);
    free(out);

    unsigned char bad[2] = { 0x00, 0x00 };
    out = NULL;
    olen = 0;
    check("inflate rejects invalid zlib header",
          inflate_zlib(bad, sizeof(bad), &out, &olen, 0) != 0);
    free(out);
    free(z);
}

static void test_deflate_zlib_roundtrip(size_t n, const char *label) {
    unsigned char *data = (unsigned char *)malloc(n ? n : 1);
    if (!data)
        exit(2);
    fill(data, n, (uint32_t)(n * 2654435761u + 1));

    unsigned char *z = NULL;
    size_t zlen = 0;
    if (deflate_zlib(data, n, &z, &zlen) != 0) {
        char m[128];
        snprintf(m, sizeof(m), "deflate_zlib failed n=%zu", n);
        ko(label, m);
        free(data);
        return;
    }

    unsigned char *out = NULL;
    size_t olen = 0;
    if (inflate_zlib(z, zlen, &out, &olen, n) != 0) {
        char m[128];
        snprintf(m, sizeof(m), "deflate_zlib inflate failed n=%zu", n);
        ko(label, m);
        free(z);
        free(data);
        return;
    }
    int eq = (olen == n) && (n == 0 || memcmp(out, data, n) == 0);
    check(label, eq);
    free(out);
    free(z);
    free(data);
}

static void test_deflate_zlib_compression(void) {
    unsigned char data[10000];
    memset(data, 'A', sizeof(data));
    unsigned char *z = NULL;
    size_t zlen = 0;
    check("deflate_zlib compresses repetitive data",
          deflate_zlib(data, sizeof(data), &z, &zlen) == 0 && zlen < 1000);

    unsigned char *out = NULL;
    size_t olen = 0;
    check("deflate_zlib repetitive roundtrip",
          inflate_zlib(z, zlen, &out, &olen, sizeof(data)) == 0 &&
          olen == sizeof(data) && memcmp(out, data, sizeof(data)) == 0);
    free(out);
    free(z);
}

static void test_scatter(void) {
    const size_t N = 8192;
    unsigned char *carrier = (unsigned char *)malloc(N);
    unsigned char *fpsrc = (unsigned char *)malloc(N);
    if (!carrier || !fpsrc)
        exit(2);
    fill(carrier, N, 0xABCDEF01u);
    memcpy(fpsrc, carrier, N);

    const char *text = "ScatterBit unit test message";
    size_t mlen = strlen(text);

    carrier_t c = { carrier, N, NULL };
    check("scatter_embed without key",
          scatter_embed(&c, fpsrc, N, (const unsigned char *)text, mlen,
                        NULL, 0, 3) == 0);

    unsigned char *msg = NULL;
    size_t msg_len = 0;
    check("scatter_auto_extract without key",
          scatter_auto_extract(&c, fpsrc, N, NULL, 0, &msg, &msg_len) == 0);
    check("scatter roundtrip content",
          msg && msg_len == mlen && memcmp(msg, text, mlen) == 0);
    free(msg);

    /* The fingerprint only uses the high bits: altering the low 3 bits of
     * fpsrc must not prevent extraction. */
    unsigned char *fpsrc2 = (unsigned char *)malloc(N);
    if (!fpsrc2)
        exit(2);
    memcpy(fpsrc2, fpsrc, N);
    for (size_t i = 0; i < N; i++)
        fpsrc2[i] = (unsigned char)((fpsrc2[i] & 0xF8) | (i & 7));

    msg = NULL;
    msg_len = 0;
    check("scatter fingerprint ignores low bits",
          scatter_auto_extract(&c, fpsrc2, N, NULL, 0, &msg, &msg_len) == 0);
    free(msg);
    free(fpsrc2);

    /* With a key: extraction works with the right key and fails with another. */
    fill(carrier, N, 0x0BADF00Du);
    memcpy(fpsrc, carrier, N);
    const char *key = "secret-key";
    check("scatter_embed with key",
          scatter_embed(&c, fpsrc, N, (const unsigned char *)text, mlen,
                        (const unsigned char *)key, strlen(key), 2) == 0);
    msg = NULL;
    msg_len = 0;
    check("scatter_auto_extract with correct key",
          scatter_auto_extract(&c, fpsrc, N,
                               (const unsigned char *)key, strlen(key),
                               &msg, &msg_len) == 0);
    free(msg);
    msg = NULL;
    msg_len = 0;
    check("scatter_auto_extract with wrong key",
          scatter_auto_extract(&c, fpsrc, N,
                               (const unsigned char *)"other", 5,
                               &msg, &msg_len) == 1);
    free(msg);

    /* Clean carrier: no message. */
    fill(carrier, N, 0x11223344u);
    memcpy(fpsrc, carrier, N);
    msg = NULL;
    msg_len = 0;
    check("scatter_auto_extract clean carrier",
          scatter_auto_extract(&c, fpsrc, N, NULL, 0, &msg, &msg_len) == 1);

    /* Binary message with zeros. */
    unsigned char bin[32];
    for (size_t i = 0; i < sizeof(bin); i++)
        bin[i] = (unsigned char)(i * 7);
    bin[3] = 0;
    bin[10] = 0;
    fill(carrier, N, 0x55667788u);
    memcpy(fpsrc, carrier, N);
    check("scatter_embed binary message",
          scatter_embed(&c, fpsrc, N, bin, sizeof(bin), NULL, 0, 3) == 0);
    msg = NULL;
    msg_len = 0;
    check("scatter_auto_extract binary message",
          scatter_auto_extract(&c, fpsrc, N, NULL, 0, &msg, &msg_len) == 0 &&
          msg_len == sizeof(bin) && memcmp(msg, bin, sizeof(bin)) == 0);
    free(msg);

    /* Message too large. */
    unsigned char *huge = (unsigned char *)malloc(4096);
    if (!huge)
        exit(2);
    memset(huge, 'Z', 4096);
    fill(carrier, 64, 1);
    carrier_t small = { carrier, 64, NULL };
    check("scatter_embed rejects oversized message",
          scatter_embed(&small, carrier, 64, huge, 4096, NULL, 0, 3) == -1);
    free(huge);

    /* Phase 1: payload compression. A carrier with ~1024 editable bits must
     * not fit a 4000-byte raw message, but must fit it once compressed. */
    unsigned char *cc = (unsigned char *)malloc(1024);
    unsigned char *cf = (unsigned char *)malloc(1024);
    if (!cc || !cf)
        exit(2);
    memset(cc, 0x42, 1024);
    memcpy(cf, cc, 1024);
    size_t big_len = 4000;
    unsigned char *bigmsg = (unsigned char *)malloc(big_len);
    if (!bigmsg)
        exit(2);
    memset(bigmsg, 'A', big_len);
    carrier_t smallc = { cc, 1024, NULL };
    check("scatter_embed fits only via compression",
          scatter_embed(&smallc, cf, 1024, bigmsg, big_len, NULL, 0, 1) == 0);
    free(bigmsg);

    unsigned char *m2 = NULL;
    size_t olen2 = 0;
    check("scatter extracts compressed payload",
          scatter_auto_extract(&smallc, cf, 1024, NULL, 0, &m2, &olen2) == 0 &&
          olen2 == 4000 && m2 && memcmp(m2, "AAAAAAAAAAAAAAAAAAAA", 20) == 0);
    free(m2);
    free(cc);
    free(cf);

    /* Phase 1: legacy v1 (SBT1) payload must still extract. */
    unsigned char lc[65536];
    unsigned char lf[65536];
    fill(lc, sizeof(lc), 0x10203040u);
    memcpy(lf, lc, sizeof(lc));
    const char *legacy = "legacy v1 message";
    carrier_t lcar = { lc, sizeof(lc), NULL };
    check("scatter_embed_v1 embeds",
          scatter_embed_v1(&lcar, lf, sizeof(lc), (const unsigned char *)legacy,
                           strlen(legacy), NULL, 0, 3) == 0);
    unsigned char *lm = NULL;
    size_t llen = 0;
    check("scatter extracts legacy v1 payload",
          scatter_auto_extract(&lcar, lf, sizeof(lc), NULL, 0, &lm, &llen) == 0 &&
          llen == strlen(legacy) && memcmp(lm, legacy, llen) == 0);
    free(lm);

    free(carrier);
    free(fpsrc);
}

int main(void) {
    test_hashes();
    test_rng();

    test_deflate_roundtrip(0, "deflate roundtrip n=0");
    test_deflate_roundtrip(1, "deflate roundtrip n=1");
    test_deflate_roundtrip(2, "deflate roundtrip n=2");
    test_deflate_roundtrip(100, "deflate roundtrip n=100");
    test_deflate_roundtrip(65535, "deflate roundtrip n=65535");
    test_deflate_roundtrip(65536, "deflate roundtrip n=65536");
    test_deflate_roundtrip(70000, "deflate roundtrip n=70000");
    test_deflate_zlib_roundtrip(0, "deflate_zlib roundtrip n=0");
    test_deflate_zlib_roundtrip(1, "deflate_zlib roundtrip n=1");
    test_deflate_zlib_roundtrip(100, "deflate_zlib roundtrip n=100");
    test_deflate_zlib_roundtrip(65536, "deflate_zlib roundtrip n=65536");
    test_deflate_zlib_roundtrip(70000, "deflate_zlib roundtrip n=70000");
    test_deflate_zlib_compression();
    test_deflate_errors();

    test_scatter();

    fprintf(stderr, "unit: %d ok, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
