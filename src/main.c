#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include "adapters.h"
#include "crypto.h"
#include "format.h"

static void usage(FILE *f) {
    fprintf(f,
        "sten - ScatterBit steganography for images (pure C, no libraries)\n"
        "\n"
        "usage:\n"
        "  sten encode -i image -o output (-m \"text\" | -f file) [-k key | -p passphrase]\n"
        "  sten decode -i image [-k key | -p passphrase]\n"
        "  sten capacity -i image\n"
        "\n"
        "options:\n"
        "  -i, --input   input image (BMP, PNG, GIF, JPEG, PNM/PAM, TGA, TIFF, ICO)\n"
        "  -o, --output  output image (encode only)\n"
        "  -m, --message text message to hide\n"
        "  -f, --file    read the message from a file\n"
        "  -k, --key     optional key (derives the bit path)\n"
        "  -p, --passphrase optional passphrase (encrypts the payload; ChaCha20 + PBKDF2)\n"
        "  -h, --help    show this help\n");
}

static unsigned char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: could not open %s\n", path);
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    unsigned char *d = (unsigned char *)malloc((size_t)sz ? (size_t)sz : 1);
    if (!d) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(d, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) {
        free(d);
        fprintf(stderr, "error: incomplete read of %s\n", path);
        return NULL;
    }
    *len = (size_t)sz;
    return d;
}

static int write_file(const char *path, const unsigned char *d, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "error: could not create %s\n", path);
        return -1;
    }
    if (len && fwrite(d, 1, len, f) != len) {
        fprintf(stderr, "error: incomplete write to %s\n", path);
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

static unsigned char *read_message(const char *msg, const char *msgfile, size_t *msg_len) {
    if (msgfile) {
        return read_file(msgfile, msg_len);
    }
    if (msg) {
        *msg_len = strlen(msg);
        return (unsigned char *)msg;
    }
    size_t cap = 4096, n = 0;
    unsigned char *d = (unsigned char *)malloc(cap);
    if (!d)
        return NULL;
    for (;;) {
        if (n == cap) {
            cap *= 2;
            unsigned char *nd = (unsigned char *)realloc(d, cap);
            if (!nd) {
                free(d);
                return NULL;
            }
            d = nd;
        }
        size_t r = fread(d + n, 1, cap - n, stdin);
        n += r;
        if (r == 0)
            break;
    }
    *msg_len = n;
    return d;
}

typedef int (*embed_fn)(unsigned char *, size_t, const unsigned char *, size_t,
                        const unsigned char *, size_t, const unsigned char *,
                        unsigned char **, size_t *);
typedef int (*extract_fn)(unsigned char *, size_t, const unsigned char *, size_t,
                          const unsigned char *, unsigned char **, size_t *);
typedef int (*capacity_fn)(const unsigned char *, size_t, size_t *);

static int do_encode(const char *in_path, const char *out_path,
                     const char *msg, const char *msgfile,
                     const unsigned char *key, size_t klen,
                     const unsigned char *enc_key) {
    size_t msg_len = 0;
    unsigned char *msgdata = read_message(msg, msgfile, &msg_len);
    if (!msgdata) {
        fprintf(stderr, "error: no message (use -m or -f)\n");
        return 3;
    }
    size_t in_len;
    unsigned char *in = read_file(in_path, &in_len);
    if (!in) {
        if (msgdata != (unsigned char *)msg)
            free(msgdata);
        return 3;
    }
    embed_fn fn = NULL;
    switch (detect_format(in, in_len)) {
    case FMT_BMP:    fn = bmp_embed;    break;
    case FMT_PNG:    fn = png_embed;    break;
    case FMT_GIF:    fn = gif_embed;    break;
    case FMT_JPEG:   fn = jpeg_embed;   break;
    case FMT_NETPBM: fn = ppm_embed;    break;
    case FMT_TGA:    fn = tga_embed;    break;
    case FMT_TIFF:   fn = tiff_embed;   break;
    case FMT_ICO:    fn = ico_embed;    break;
    default:
        fprintf(stderr, "error: unsupported format\n");
        free(in);
        if (msgdata != (unsigned char *)msg)
            free(msgdata);
        return 3;
    }
    unsigned char *out = NULL;
    size_t out_len = 0;
    int rc = fn(in, in_len, msgdata, msg_len, key, klen, enc_key, &out, &out_len);
    if (msgdata != (unsigned char *)msg)
        free(msgdata);
    if (rc != 0) {
        free(in);
        return 3;
    }
    int w = write_file(out_path, out, out_len);
    free(in);
    if (out != in)
        free(out);
    return w ? 3 : 0;
}

static int do_decode(const char *in_path, const unsigned char *key, size_t klen,
                     const unsigned char *enc_key) {
    size_t in_len;
    unsigned char *in = read_file(in_path, &in_len);
    if (!in)
        return 3;
    extract_fn fn = NULL;
    switch (detect_format(in, in_len)) {
    case FMT_BMP:    fn = bmp_extract;    break;
    case FMT_PNG:    fn = png_extract;    break;
    case FMT_GIF:    fn = gif_extract;    break;
    case FMT_JPEG:   fn = jpeg_extract;   break;
    case FMT_NETPBM: fn = ppm_extract;    break;
    case FMT_TGA:    fn = tga_extract;    break;
    case FMT_TIFF:   fn = tiff_extract;   break;
    case FMT_ICO:    fn = ico_extract;    break;
    default:
        fprintf(stderr, "error: unsupported format\n");
        free(in);
        return 3;
    }
    unsigned char *msg = NULL;
    size_t msg_len = 0;
    int rc = fn(in, in_len, key, klen, enc_key, &msg, &msg_len);
    free(in);
    if (rc == 1) {
        fprintf(stderr, "no message found (clean image or wrong key)\n");
        return 1;
    }
    if (rc != 0)
        return 3;
    fwrite(msg, 1, msg_len, stdout);
    fputc('\n', stdout);
    free(msg);
    return 0;
}

static int do_capacity(const char *in_path) {
    size_t in_len;
    unsigned char *in = read_file(in_path, &in_len);
    if (!in)
        return 3;
    capacity_fn fn = NULL;
    switch (detect_format(in, in_len)) {
    case FMT_BMP:    fn = bmp_capacity;    break;
    case FMT_PNG:    fn = png_capacity;    break;
    case FMT_GIF:    fn = gif_capacity;    break;
    case FMT_JPEG:   fn = jpeg_capacity;   break;
    case FMT_NETPBM: fn = ppm_capacity;    break;
    case FMT_TGA:    fn = tga_capacity;    break;
    case FMT_TIFF:   fn = tiff_capacity;   break;
    case FMT_ICO:    fn = ico_capacity;    break;
    default:
        fprintf(stderr, "error: unsupported format\n");
        free(in);
        return 3;
    }
    size_t bytes = 0;
    int rc = fn(in, in_len, &bytes);
    free(in);
    if (rc != 0)
        return 3;
    printf("%zu\n", bytes);
    return 0;
}

static const unsigned char PBKDF2_SALT[16] = "sten-pbkdf2-salt";

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(stderr);
        return 2;
    }
    const char *cmd = argv[1];
    if (!strcmp(cmd, "-h") || !strcmp(cmd, "--help")) {
        usage(stdout);
        return 0;
    }
    const char *in_path = NULL, *out_path = NULL, *key = NULL, *msg = NULL, *msgfile = NULL;
    const char *pass = NULL;

    static struct option lopts[] = {
        { "input",      required_argument, 0, 'i' },
        { "output",     required_argument, 0, 'o' },
        { "message",    required_argument, 0, 'm' },
        { "file",       required_argument, 0, 'f' },
        { "key",        required_argument, 0, 'k' },
        { "passphrase", required_argument, 0, 'p' },
        { "help",       no_argument,       0, 'h' },
        { 0, 0, 0, 0 }
    };
    int c;
    while ((c = getopt_long(argc - 1, argv + 1, "i:o:m:f:k:p:h", lopts, NULL)) != -1) {
        switch (c) {
        case 'i': in_path = optarg;  break;
        case 'o': out_path = optarg; break;
        case 'm': msg = optarg;      break;
        case 'f': msgfile = optarg;  break;
        case 'k': key = optarg;      break;
        case 'p': pass = optarg;     break;
        case 'h': usage(stdout);     return 0;
        default:  usage(stderr);     return 2;
        }
    }
    if (msg && msgfile) {
        fprintf(stderr, "error: use only one of -m or -f\n");
        return 2;
    }
    if (key && pass) {
        fprintf(stderr, "error: use only one of -k or -p\n");
        return 2;
    }

    unsigned char key32[32];
    const unsigned char *kptr = NULL;
    size_t klen = 0;
    const unsigned char *enc_key = NULL;
    if (pass) {
        pbkdf2_sha256((const unsigned char *)pass, strlen(pass),
                      PBKDF2_SALT, sizeof(PBKDF2_SALT),
                      STEN_PBKDF2_ITERS, key32, sizeof(key32));
        kptr = key32;
        klen = sizeof(key32);
        enc_key = key32;
    } else if (key) {
        kptr = (const unsigned char *)key;
        klen = strlen(key);
    }

    if (!strcmp(cmd, "encode")) {
        if (!in_path || !out_path || (!msg && !msgfile && isatty(fileno(stdin)))) {
            usage(stderr);
            return 2;
        }
        return do_encode(in_path, out_path, msg, msgfile, kptr, klen, enc_key);
    }
    if (!strcmp(cmd, "decode")) {
        if (!in_path) {
            usage(stderr);
            return 2;
        }
        return do_decode(in_path, kptr, klen, enc_key);
    }
    if (!strcmp(cmd, "capacity")) {
        if (!in_path) {
            usage(stderr);
            return 2;
        }
        return do_capacity(in_path);
    }
    usage(stderr);
    return 2;
}