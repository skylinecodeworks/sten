#define _POSIX_C_SOURCE 200809L

#include "rand.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int rand_bytes(unsigned char *out, size_t n) {
    if (n == 0)
        return 0;
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, out + got, n - got);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            close(fd);
            return -1;
        }
        if (r == 0) {
            close(fd);
            return -1;
        }
        got += (size_t)r;
    }
    close(fd);
    return 0;
}