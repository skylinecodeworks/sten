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

typedef struct {
    unsigned char *d;
    size_t n, cap;
    uint64_t bitbuf;
    int nbits;
} bitwriter_t;

static int bw_reserve(bitwriter_t *w, size_t extra) {
    if (w->n + extra <= w->cap)
        return 0;
    size_t ncap = w->cap ? w->cap : 1024;
    while (ncap < w->n + extra) {
        if (ncap > ((size_t)1 << 30))
            return -1;
        ncap *= 2;
    }
    unsigned char *nd = (unsigned char *)realloc(w->d, ncap);
    if (!nd)
        return -1;
    w->d = nd;
    w->cap = ncap;
    return 0;
}

static int bw_bit(bitwriter_t *w, unsigned b) {
    w->bitbuf |= (uint64_t)(b & 1u) << w->nbits;
    if (++w->nbits == 8) {
        if (bw_reserve(w, 1))
            return -1;
        w->d[w->n++] = (unsigned char)w->bitbuf;
        w->bitbuf = 0;
        w->nbits = 0;
    }
    return 0;
}

static int bw_code(bitwriter_t *w, unsigned code, int len) {
    for (int i = len - 1; i >= 0; i--)
        if (bw_bit(w, (code >> i) & 1u))
            return -1;
    return 0;
}

static const unsigned HBITS = 15;
#define HWINDOW 32768

static unsigned hash3(const unsigned char *p) {
    unsigned h = ((unsigned)p[0] << 10) ^ ((unsigned)p[1] << 5) ^ (unsigned)p[2];
    return h & ((1u << HBITS) - 1);
}

static void fixed_litlen(int sym, unsigned *code, int *bits) {
    if (sym < 144) {
        *code = 0x30u + (unsigned)sym;
        *bits = 8;
    } else if (sym < 256) {
        *code = 0x190u + (unsigned)(sym - 144);
        *bits = 9;
    } else if (sym < 280) {
        *code = (unsigned)(sym - 256);
        *bits = 7;
    } else {
        *code = 0xC0u + (unsigned)(sym - 280);
        *bits = 8;
    }
}

static int emit_matches(bitwriter_t *w, size_t len, size_t dist) {
    int s;
    for (s = 0; s < 29; s++) {
        unsigned base = len_base[s];
        unsigned span = 1u << len_extra[s];
        if (len >= (size_t)base && len <= (size_t)base + span - 1)
            break;
    }
    if (s == 29)
        return -1;
    unsigned code;
    int bits;
    fixed_litlen(257 + s, &code, &bits);
    if (bw_code(w, code, bits))
        return -1;
    unsigned lenx = (unsigned)len - len_base[s];
    for (int i = 0; i < len_extra[s]; i++)
        if (bw_bit(w, (lenx >> i) & 1u))
            return -1;
    int d;
    for (d = 0; d < 30; d++) {
        unsigned base = dist_base[d];
        unsigned span = 1u << dist_extra[d];
        if (dist >= (size_t)base && dist <= (size_t)base + span - 1)
            break;
    }
    if (d == 30)
        return -1;
    if (bw_code(w, (unsigned)d, 5))
        return -1;
    unsigned dstx = (unsigned)dist - dist_base[d];
    for (int i = 0; i < dist_extra[d]; i++)
        if (bw_bit(w, (dstx >> i) & 1u))
            return -1;
    return 0;
}

static int deflate_fixed(const unsigned char *data, size_t len, bitwriter_t *w) {
    if (bw_bit(w, 1))
        return -1;
    if (bw_bit(w, 1))
        return -1;
    if (bw_bit(w, 0))
        return -1;
    unsigned *head = (unsigned *)malloc(sizeof(unsigned) * (1u << HBITS));
    unsigned *prev = (unsigned *)malloc(sizeof(unsigned) * (len ? len : 1));
    if (!head || !prev) {
        free(head);
        free(prev);
        return -1;
    }
    for (size_t i = 0; i < (1u << HBITS); i++)
        head[i] = 0xFFFFFFFFu;
    size_t pos = 0;
    while (pos < len) {
        int matched = 0;
        if (pos + 3 <= len) {
            unsigned h = hash3(data + pos);
            size_t best = 0, best_dist = 0;
            unsigned chain = 128;
            size_t limit = pos > HWINDOW ? pos - HWINDOW : 0;
            size_t p = head[h];
            while (chain-- && p != 0xFFFFFFFFu && p >= limit && p < pos) {
                size_t l = 0;
                while (l < 258 && pos + l < len && data[p + l] == data[pos + l])
                    l++;
                if (l > best) {
                    best = l;
                    best_dist = pos - p;
                    if (l == 258 || l >= 64)
                        break;
                }
                p = prev[p];
            }
            prev[pos] = head[h];
            head[h] = (unsigned)pos;
            if (best >= 3) {
                if (emit_matches(w, best, best_dist)) {
                    free(head);
                    free(prev);
                    return -1;
                }
                pos += best;
                matched = 1;
            }
        }
        if (!matched) {
            unsigned code;
            int bits;
            fixed_litlen(data[pos], &code, &bits);
            if (bw_code(w, code, bits)) {
                free(head);
                free(prev);
                return -1;
            }
            pos++;
        }
    }
    unsigned code;
    int bits;
    fixed_litlen(256, &code, &bits);
    int rc = bw_code(w, code, bits);
    free(head);
    free(prev);
    return rc;
}

int deflate_zlib(const unsigned char *data, size_t len,
                 unsigned char **out, size_t *out_len) {
    size_t stored_cap = 2 + (len / 65535 + 1) * 5 + len + 4;
    bitwriter_t w = { NULL, 0, 0, 0, 0 };
    if (bw_reserve(&w, len + len / 4 + 64))
        return -1;
    w.d[w.n++] = 0x78;
    w.d[w.n++] = 0x01;
    if (deflate_fixed(data, len, &w)) {
        free(w.d);
        return -1;
    }
    if (w.nbits) {
        if (bw_reserve(&w, 1)) {
            free(w.d);
            return -1;
        }
        w.d[w.n++] = (unsigned char)w.bitbuf;
        w.bitbuf = 0;
        w.nbits = 0;
    }
    uint32_t a = adler32(data, len);
    if (bw_reserve(&w, 4)) {
        free(w.d);
        return -1;
    }
    w.d[w.n++] = (unsigned char)(a >> 24);
    w.d[w.n++] = (unsigned char)(a >> 16);
    w.d[w.n++] = (unsigned char)(a >> 8);
    w.d[w.n++] = (unsigned char)a;
    if (w.n >= stored_cap) {
        free(w.d);
        return deflate_zlib_stored(data, len, out, out_len);
    }
    *out = w.d;
    *out_len = w.n;
    return 0;
}
