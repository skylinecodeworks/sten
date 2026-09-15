#ifndef STEN_RAND_H
#define STEN_RAND_H

#include <stddef.h>

/* Cryptographically secure random bytes from the OS entropy source.
 * Fills out[0..n-1]; returns 0 on success, -1 on failure. */
int rand_bytes(unsigned char *out, size_t n);

#endif