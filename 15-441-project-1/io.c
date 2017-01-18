#include <unistd.h>
#include <stdio.h>

#include "io.h"
#include "utils.h"

static int maxfd;
static fd_set readfs;
static fd_set writefs;

void io_init() {
    maxfd = -1;
    FD_ZERO(&readfs);
    FD_ZERO(&writefs);
}

void io_wait() {
    if (maxfd == -1) {
        return;
    }
    int rv = select(maxfd + 1, &readfs, &writefs, NULL, NULL);
    if (rv == -1) {
        perror("select");
    }
    io_init();
}

void io_need_read(int fd) {
    maxfd = max(maxfd, fd);
    FD_SET(fd, &readfs);
}

void io_need_write(int fd) {
    maxfd = max(maxfd, fd);
    FD_SET(fd, &writefs);
}

int io_wait_read(int fd) {
    return FD_ISSET(fd, &readfs) > 0;
}

int io_wait_write(int fd) {
    return FD_ISSET(fd, &writefs) > 0;
}
