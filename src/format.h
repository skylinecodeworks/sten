#ifndef STEN_FORMAT_H
#define STEN_FORMAT_H

#include <stddef.h>

typedef enum {
    FMT_UNKNOWN = 0,
    FMT_BMP,
    FMT_PNG,
    FMT_GIF,
    FMT_JPEG
} fmt_t;

fmt_t detect_format(const unsigned char *d, size_t n);
const char *fmt_name(fmt_t f);

#endif
