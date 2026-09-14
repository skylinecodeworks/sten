#include "format.h"
#include <string.h>

fmt_t detect_format(const unsigned char *d, size_t n) {
    if (n >= 8 && !memcmp(d, "\x89PNG\r\n\x1a\n", 8))
        return FMT_PNG;
    if (n >= 2 && d[0] == 0xFF && d[1] == 0xD8)
        return FMT_JPEG;
    if (n >= 6 && (!memcmp(d, "GIF87a", 6) || !memcmp(d, "GIF89a", 6)))
        return FMT_GIF;
    if (n >= 2 && d[0] == 'B' && d[1] == 'M')
        return FMT_BMP;
    return FMT_UNKNOWN;
}

const char *fmt_name(fmt_t f) {
    switch (f) {
    case FMT_BMP:  return "BMP";
    case FMT_PNG:  return "PNG";
    case FMT_GIF:  return "GIF";
    case FMT_JPEG: return "JPEG";
    default:       return "unknown";
    }
}
