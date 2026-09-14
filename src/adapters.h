#ifndef STEN_ADAPTERS_H
#define STEN_ADAPTERS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    long w, h;
    int channels;
    int bits;
    int has_dims;
} sten_info_t;

/* embed: 0 ok / -1 error. If the format is in-place (bmp/gif), *out == in. */
/* extract: 0 ok / 1 no message / -1 error. */
/* enc_key: 32-byte payload encryption key, or NULL for plain. */
/* inspect: 0 ok / -1 error; fills the image info struct. */

/* Validates the PNG signature + IHDR and returns dimensions and color type
 * (2=RGB, 6=RGBA). Shared by the PNG and ICO adapters. */
int png_info(const unsigned char *in, size_t in_len, uint32_t *w, uint32_t *h,
             unsigned *color_type);

int bmp_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key,
              unsigned char **out, size_t *out_len);
int bmp_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int bmp_capacity(const unsigned char *in, size_t in_len, size_t *bytes);
int bmp_inspect(const unsigned char *in, size_t in_len, sten_info_t *info);

int png_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key,
              unsigned char **out, size_t *out_len);
int png_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int png_capacity(const unsigned char *in, size_t in_len, size_t *bytes);
int png_inspect(const unsigned char *in, size_t in_len, sten_info_t *info);

int gif_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key,
              unsigned char **out, size_t *out_len);
int gif_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int gif_capacity(const unsigned char *in, size_t in_len, size_t *bytes);
int gif_inspect(const unsigned char *in, size_t in_len, sten_info_t *info);

int jpeg_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
               const unsigned char *key, size_t key_len, const unsigned char *enc_key,
               unsigned char **out, size_t *out_len);
int jpeg_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                 const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int jpeg_capacity(const unsigned char *in, size_t in_len, size_t *bytes);
int jpeg_inspect(const unsigned char *in, size_t in_len, sten_info_t *info);

int ppm_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key,
              unsigned char **out, size_t *out_len);
int ppm_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int ppm_capacity(const unsigned char *in, size_t in_len, size_t *bytes);
int ppm_inspect(const unsigned char *in, size_t in_len, sten_info_t *info);

int tga_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key,
              unsigned char **out, size_t *out_len);
int tga_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int tga_capacity(const unsigned char *in, size_t in_len, size_t *bytes);
int tga_inspect(const unsigned char *in, size_t in_len, sten_info_t *info);

int tiff_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
               const unsigned char *key, size_t key_len, const unsigned char *enc_key,
               unsigned char **out, size_t *out_len);
int tiff_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                 const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int tiff_capacity(const unsigned char *in, size_t in_len, size_t *bytes);
int tiff_inspect(const unsigned char *in, size_t in_len, sten_info_t *info);

int ico_embed(unsigned char *in, size_t in_len, const unsigned char *msg, size_t msg_len,
              const unsigned char *key, size_t key_len, const unsigned char *enc_key,
              unsigned char **out, size_t *out_len);
int ico_extract(unsigned char *in, size_t in_len, const unsigned char *key, size_t key_len,
                const unsigned char *enc_key, unsigned char **msg, size_t *msg_len);
int ico_capacity(const unsigned char *in, size_t in_len, size_t *bytes);
int ico_inspect(const unsigned char *in, size_t in_len, sten_info_t *info);

#endif