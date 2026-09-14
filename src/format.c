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
    if (n >= 4 && d[0] == 0 && d[1] == 0 && d[2] == 1 && d[3] == 0)
        return FMT_ICO;
    if (n >= 4 && d[0] == 'I' && d[1] == 'I' && d[2] == 42 && d[3] == 0)
        return FMT_TIFF;
    if (n >= 2 && d[0] == 'P' && d[1] >= '1' && d[1] <= '7')
        return FMT_NETPBM;
    if (n >= 3 && d[0] == 0 && d[1] == 0 && (d[2] == 2 || d[2] == 3))
        return FMT_TGA;
    return FMT_UNKNOWN;
}

const char *fmt_name(fmt_t f) {
    switch (f) {
    case FMT_BMP:    return "BMP";
    case FMT_PNG:    return "PNG";
    case FMT_GIF:    return "GIF";
    case FMT_JPEG:   return "JPEG";
    case FMT_NETPBM: return "PNM/PAM";
    case FMT_TGA:    return "TGA";
    case FMT_TIFF:   return "TIFF";
    case FMT_ICO:    return "ICO";
    default:         return "unknown";
    }
}