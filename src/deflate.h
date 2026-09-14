#ifndef STEN_DEFLATE_H
#define STEN_DEFLATE_H

#include <stddef.h>

/* Decompresses a zlib stream (RFC 1950 + RFC 1951).
 * If expected_len != 0 the output must have that exact size.
 * Returns 0 ok / -1 error. Output is allocated with malloc in *out. */
int inflate_zlib(const unsigned char *src, size_t src_len,
                 unsigned char **out, size_t *out_len, size_t expected_len);

/* Compresses with stored (uncompressed) blocks inside a valid zlib stream
 * (header + deflate blocks + adler32). Returns 0 ok / -1 error. */
int deflate_zlib_stored(const unsigned char *data, size_t len,
                        unsigned char **out, size_t *out_len);

/* Compresses with LZ77 + fixed Huffman (falling back to stored blocks when
 * that is smaller) into a valid zlib stream. Returns 0 ok / -1 error. */
int deflate_zlib(const unsigned char *data, size_t len,
                 unsigned char **out, size_t *out_len);

#endif
