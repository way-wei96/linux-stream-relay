#include "relay.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct relay_buf {
    unsigned char data[256];
    size_t len;
    int in_done;
};

static int poll_add(struct pollfd *pfds, int nfd, int fd, short events)
{
    pfds[nfd].fd = fd;
    pfds[nfd].events = events;
    pfds[nfd].revents = 0;
    return nfd + 1;
}

static int poll_wait(struct pollfd *pfds, int nfd)
{
    for (;;) {
        if (poll(pfds, nfd, -1) < 0) {
            if (errno == EINTR)
                continue;
            perror("poll");
            return -1;
        }
        return 0;
    }
}

static int poll_has_error(short revents, const char *side)
{
    if (!(revents & (POLLERR | POLLNVAL)))
        return 0;
    fprintf(stderr, "poll: %s error\n", side);
    return 1;
}

static int buf_read(int in_fd, struct relay_buf *b)
{
    ssize_t n = read(in_fd, b->data + b->len, sizeof b->data - b->len);

    if (n > 0) {
        b->len += (size_t)n;
        return 0;
    }
    if (n == 0) {
        b->in_done = 1;
        return 0;
    }
    if (errno == EINTR || errno == EAGAIN)
        return 0;
    perror("read");
    return -1;
}

static int buf_write(int out_fd, struct relay_buf *b)
{
    ssize_t w = write(out_fd, b->data, b->len);

    if (w > 0) {
        b->len -= (size_t)w;
        if (b->len > 0)
            memmove(b->data, b->data + w, b->len);
        return 0;
    }
    if (w < 0 && (errno == EINTR || errno == EAGAIN))
        return 0;
    perror("write");
    return -1;
}

static int handle_input(int in_fd, short revents, struct relay_buf *b)
{
    if (poll_has_error(revents, "input"))
        return -1;
    if (!(revents & (POLLIN | POLLHUP)))
        return 0;
    return buf_read(in_fd, b);
}

static int handle_output(int out_fd, short revents, struct relay_buf *b)
{
    if (poll_has_error(revents, "output"))
        return -1;
    if (!(revents & POLLOUT))
        return 0;
    return buf_write(out_fd, b);
}

int relay_bytes(int in_fd, int out_fd)
{
    struct relay_buf b = {0};
    struct pollfd pfds[2];

    for (;;) {
        int nfd = 0;
        int in_idx = -1;
        int out_idx = -1;

        if (b.in_done && b.len == 0)
            return 0;

        if (!b.in_done && b.len < sizeof b.data) {
            in_idx = nfd;
            nfd = poll_add(pfds, nfd, in_fd, POLLIN);
        }
        if (b.len > 0) {
            out_idx = nfd;
            nfd = poll_add(pfds, nfd, out_fd, POLLOUT);
        }

        if (poll_wait(pfds, nfd) < 0)
            return -1;
        if (in_idx >= 0 && handle_input(in_fd, pfds[in_idx].revents, &b) < 0)
            return -1;
        if (out_idx >= 0 && handle_output(out_fd, pfds[out_idx].revents, &b) < 0)
            return -1;
    }
}
