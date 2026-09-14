#ifndef STEN_UTIL_H
#define STEN_UTIL_H

#include <stddef.h>
#include <stdint.h>

uint32_t crc32(const unsigned char *data, size_t len);
uint32_t adler32(const unsigned char *data, size_t len);
uint64_t fnv1a64(const unsigned char *data, size_t len);

typedef struct {
    uint64_t s;
} rng_t;

void rng_seed(rng_t *r, uint64_t seed);
uint64_t rng_next(rng_t *r);

#endif
