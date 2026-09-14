#ifndef STEN_ADAPTERS_H
#define STEN_ADAPTERS_H

#include <stddef.h>

/* embed: 0 ok / -1 error. If the format is in-place (bmp/gif), *out == in. */
/* extract: 0 ok / 1 no message / -1 error. */
/* enc_key: 32-byte payload encryption key, or NULL for plain. */

int bmp_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key,
              unsigned char **out, size_t *out_len);
int bmp_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int bmp_capacity(const unsigned char *in, size_t in_len, size_t *bytes);

int png_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key,
              unsigned char **out, size_t *out_len);
int png_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int png_capacity(const unsigned char *in, size_t in_len, size_t *bytes);

int gif_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key,
              unsigned char **out, size_t *out_len);
int gif_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int gif_capacity(const unsigned char *in, size_t in_len, size_t *bytes);

int jpeg_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
               const unsigned char *key, size_t key_len, const unsigned char *enc_key,
               unsigned char **out, size_t *out_len);
int jpeg_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                 const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int jpeg_capacity(const unsigned char *in, size_t in_len, size_t *bytes);

int ppm_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key,
              unsigned char **out, size_t *out_len);
int ppm_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int ppm_capacity(const unsigned char *in, size_t in_len, size_t *bytes);

int tga_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key,
              unsigned char **out, size_t *out_len);
int tga_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int tga_capacity(const unsigned char *in, size_t in_len, size_t *bytes);

int tiff_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
               const unsigned char *key, size_t key_len, const unsigned char *enc_key,
               unsigned char **out, size_t *out_len);
int tiff_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                 const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int tiff_capacity(const unsigned char *in, size_t in_len, size_t *bytes);

int ico_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key,
              unsigned char **out, size_t *out_len);
int ico_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int ico_capacity(const unsigned char *in, size_t in_len, size_t *bytes);

#endif