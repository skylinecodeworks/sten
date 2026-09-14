#ifndef STEN_ADAPTERS_H
#define STEN_ADAPTERS_H

#include <stddef.h>

/* embed: 0 ok / -1 error. If the format is in-place (bmp/gif), *out == in. */
/* extract: 0 ok / 1 no message / -1 error. */

int bmp_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, unsigned char **out, size_t *out_len);
int bmp_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                unsigned char **msg, size_t *msg_len);

int png_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, unsigned char **out, size_t *out_len);
int png_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                unsigned char **msg, size_t *msg_len);

int gif_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, unsigned char **out, size_t *out_len);
int gif_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                unsigned char **msg, size_t *msg_len);

int jpeg_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
               const unsigned char *key, size_t key_len, unsigned char **out, size_t *out_len);
int jpeg_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                 unsigned char **msg, size_t *msg_len);

#endif
