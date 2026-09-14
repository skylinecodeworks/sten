#include "deflate.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    const unsigned char *p;
    size_t len, pos;
    uint64_t bits;
    int nbits;
} bitreader_t;

static int br_bit(bitreader_t *b) {
    if (b->nbits == 0) {
        if (b->pos >= b->len)
            return -1;
        b->bits = b->p[b->pos++];
        b->nbits = 8;
    }
    int v = (int)(b->bits & 1);
    b->bits >>= 1;
    b->nbits--;
    return v;
}

static int br_bits(bitreader_t *b, int n) {
    int v = 0;
    for (int i = 0; i < n; i++) {
        int t = br_bit(b);
        if (t < 0)
            return -1;
        v |= t << i;
    }
    return v;
}

typedef struct {
    unsigned short counts[16];
    unsigned short symbols[320];
} htree_t;

static int htree_build(htree_t *t, const unsigned char *lengths, int n) {
    memset(t->counts, 0, sizeof(t->counts));
    for (int i = 0; i < n; i++) {
        unsigned len = lengths[i];
        if (len > 15)
            return -1;
        t->counts[len]++;
    }
    int left = 1;
    for (int len = 1; len <= 15; len++) {
        left <<= 1;
        left -= t->counts[len];
        if (left < 0)
            return -1;
    }
    int k = 0;
    for (int len = 1; len <= 15; len++) {
        for (int s = 0; s < n; s++) {
            if (lengths[s] == len) {
                if (k >= 320)
                    return -1;
                t->symbols[k++] = (unsigned short)s;
            }
        }
    }
    return 0;
}

static int htree_decode(const htree_t *t, bitreader_t *b) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= 15; len++) {
        int bit = br_bit(b);
        if (bit < 0)
            return -1;
        code |= bit;
        int cnt = t->counts[len];
        if (code - first < cnt)
            return t->symbols[index + code - first];
        index += cnt;
        first += cnt;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

static int htree_fixed(htree_t *lit, htree_t *dist) {
    unsigned char lengths[288];
    int i = 0;
    for (; i < 144; i++) lengths[i] = 8;
    for (; i < 256; i++) lengths[i] = 9;
    for (; i < 280; i++) lengths[i] = 7;
    for (; i < 288; i++) lengths[i] = 8;
    if (htree_build(lit, lengths, 288))
        return -1;
    unsigned char dlens[32];
    for (int j = 0; j < 32; j++) dlens[j] = 5;
    return htree_build(dist, dlens, 32);
}

static const unsigned short len_base[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258
};
static const unsigned char len_extra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const unsigned short dist_base[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
};
static const unsigned char dist_extra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

typedef struct {
    unsigned char *d;
    size_t n, cap;
} outbuf_t;

static int ob_reserve(outbuf_t *o, size_t extra) {
    if (o->n + extra <= o->cap)
        return 0;
    size_t ncap = o->cap ? o->cap : 4096;
    while (ncap < o->n + extra) {
        if (ncap > ((size_t)1 << 30))
            return -1;
        ncap *= 2;
    }
    unsigned char *nd = (unsigned char *)realloc(o->d, ncap);
    if (!nd)
        return -1;
    o->d = nd;
    o->cap = ncap;
    return 0;
}

int inflate_zlib(const unsigned char *src, size_t src_len,
                 unsigned char **out, size_t *out_len, size_t expected_len) {
    if (src_len < 2)
        return -1;
    unsigned cmf = src[0], flg = src[1];
    if ((cmf & 0x0F) != 8)
        return -1;
    if ((((unsigned)cmf << 8) | flg) % 31 != 0)
        return -1;
    if (flg & 0x20)
        return -1;

    bitreader_t b = { src + 2, src_len - 2, 0, 0, 0 };
    outbuf_t o = { NULL, 0, 0 };
    htree_t lit, dist, clen_tree;
    int final = 0, ok = -1;

    while (!final) {
        int fb = br_bit(&b);
        if (fb < 0)
            goto fail;
        final = fb;
        int type = br_bits(&b, 2);
        if (type < 0)
            goto fail;

        if (type == 0) {
            while (b.nbits > 0)
                br_bit(&b);
            int len = br_bits(&b, 16);
            int nlen = br_bits(&b, 16);
            if (len < 0 || nlen < 0)
                goto fail;
            if ((len ^ 0xFFFF) != nlen)
                goto fail;
            if (b.pos + (size_t)len > b.len)
                goto fail;
            if (ob_reserve(&o, (size_t)len))
                goto fail;
            memcpy(o.d + o.n, b.p + b.pos, (size_t)len);
            o.n += (size_t)len;
            b.pos += (size_t)len;
        } else if (type == 1 || type == 2) {
            if (type == 1) {
                if (htree_fixed(&lit, &dist))
                    goto fail;
            } else {
                int hlit = br_bits(&b, 5);
                int hdist = br_bits(&b, 5);
                int hclen = br_bits(&b, 4);
                if (hlit < 0 || hdist < 0 || hclen < 0)
                    goto fail;
                hlit += 257;
                hdist += 1;
                hclen += 4;
                static const unsigned char order[19] = {
                    16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
                };
                unsigned char clens[19] = { 0 };
                for (int i = 0; i < hclen; i++) {
                    int v = br_bits(&b, 3);
                    if (v < 0)
                        goto fail;
                    clens[order[i]] = (unsigned char)v;
                }
                if (htree_build(&clen_tree, clens, 19))
                    goto fail;
                unsigned char lengths[320] = { 0 };
                int total = hlit + hdist;
                int idx = 0;
                while (idx < total) {
                    int sym = htree_decode(&clen_tree, &b);
                    if (sym < 0 || sym > 18)
                        goto fail;
                    if (sym <= 15) {
                        lengths[idx++] = (unsigned char)sym;
                    } else if (sym == 16) {
                        if (idx == 0)
                            goto fail;
                        int rep = br_bits(&b, 2);
                        if (rep < 0)
                            goto fail;
                        rep += 3;
                        while (rep-- > 0 && idx < total)
                            lengths[idx] = lengths[idx - 1], idx++;
                    } else if (sym == 17) {
                        int rep = br_bits(&b, 3);
                        if (rep < 0)
                            goto fail;
                        rep += 3;
                        while (rep-- > 0 && idx < total)
                            lengths[idx++] = 0;
                    } else {
                        int rep = br_bits(&b, 7);
                        if (rep < 0)
                            goto fail;
                        rep += 11;
                        while (rep-- > 0 && idx < total)
                            lengths[idx++] = 0;
                    }
                }
                if (htree_build(&lit, lengths, hlit))
                    goto fail;
                if (htree_build(&dist, lengths + hlit, hdist))
                    goto fail;
            }
            for (;;) {
                int sym = htree_decode(&lit, &b);
                if (sym < 0)
                    goto fail;
                if (sym < 256) {
                    if (ob_reserve(&o, 1))
                        goto fail;
                    o.d[o.n++] = (unsigned char)sym;
                } else if (sym == 256) {
                    break;
                } else {
                    sym -= 257;
                    if (sym >= 29)
                        goto fail;
                    int extra = br_bits(&b, len_extra[sym]);
                    if (extra < 0)
                        goto fail;
                    int length = len_base[sym] + extra;
                    int dsym = htree_decode(&dist, &b);
                    if (dsym < 0 || dsym >= 30)
                        goto fail;
                    int dextra = br_bits(&b, dist_extra[dsym]);
                    if (dextra < 0)
                        goto fail;
                    int distance = dist_base[dsym] + dextra;
                    if ((size_t)distance > o.n)
                        goto fail;
                    if (ob_reserve(&o, (size_t)length))
                        goto fail;
                    for (int k = 0; k < length; k++)
                        o.d[o.n + k] = o.d[o.n - (size_t)distance + k];
                    o.n += (size_t)length;
                }
            }
        } else {
            goto fail;
        }
    }
    if (expected_len && o.n != expected_len)
        goto fail;
    ok = 0;

fail:
    if (ok == 0) {
        *out = o.d;
        *out_len = o.n;
        return 0;
    }
    free(o.d);
    return -1;
}

int deflate_zlib_stored(const unsigned char *data, size_t len,
                        unsigned char **out, size_t *out_len) {
    size_t cap = len + (len / 65535 + 1) * 5 + 16;
    unsigned char *o = (unsigned char *)malloc(cap);
    if (!o)
        return -1;
    size_t n = 0;
    o[n++] = 0x78;
    o[n++] = 0x01;
    size_t off = 0;
    do {
        size_t chunk = len - off;
        if (chunk > 65535)
            chunk = 65535;
        int final = (off + chunk == len) ? 1 : 0;
        o[n++] = (unsigned char)final;
        o[n++] = (unsigned char)(chunk & 0xFF);
        o[n++] = (unsigned char)((chunk >> 8) & 0xFF);
        o[n++] = (unsigned char)((~chunk) & 0xFF);
        o[n++] = (unsigned char)(((~chunk) >> 8) & 0xFF);
        memcpy(o + n, data + off, chunk);
        n += chunk;
        off += chunk;
    } while (off < len);
    uint32_t a = adler32(data, len);
    o[n++] = (unsigned char)(a >> 24);
    o[n++] = (unsigned char)(a >> 16);
    o[n++] = (unsigned char)(a >> 8);
    o[n++] = (unsigned char)a;
    *out = o;
    *out_len = n;
    return 0;
}
