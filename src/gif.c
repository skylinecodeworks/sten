#include "adapters.h"
#include "scatter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int gif_parse(const unsigned char *in, size_t len,
                     unsigned char **palette, size_t *pal_len) {
    if (len < 13) {
        fprintf(stderr, "error: invalid GIF\n");
        return -1;
    }
    unsigned packed = in[10];
    if (!(packed & 0x80)) {
        fprintf(stderr, "error: GIF without global palette (not supported)\n");
        return -1;
    }
    size_t count = (size_t)1 << ((packed & 7) + 1);
    size_t plen = count * 3;
    if (13 + plen > len) {
        fprintf(stderr, "error: GIF palette out of range\n");
        return -1;
    }
    *palette = (unsigned char *)in + 13;
    *pal_len = plen;
    return 0;
}

int gif_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, unsigned char **out, size_t *out_len) {
    unsigned char *palette;
    size_t pal_len;
    if (gif_parse(in, in_len, &palette, &pal_len))
        return -1;
    carrier_t c = { palette, pal_len, NULL };
    int rc = scatter_embed(&c, palette, pal_len, msg, msg_len, key, key_len, 2);
    if (rc == -1) {
        fprintf(stderr, "error: message too large for this GIF palette\n");
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

int gif_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                unsigned char **msg, size_t *msg_len) {
    unsigned char *palette;
    size_t pal_len;
    if (gif_parse(in, in_len, &palette, &pal_len))
        return -1;
    carrier_t c = { palette, pal_len, NULL };
    return scatter_auto_extract(&c, palette, pal_len, key, key_len, msg, msg_len);
}
