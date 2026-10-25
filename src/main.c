#include <errno.h>
#include <stdio.h>
#include <unistd.h>

/* 把 buf 里 n 字节写完；短写则继续。成功 0，失败 -1。 */
static int write_all(int fd, const unsigned char *buf, ssize_t n)
{
    ssize_t off = 0;

    while (off < n) {
        ssize_t w = write(fd, buf + off, (size_t)(n - off));

        if (w < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        off += w;
    }
    return 0;
}

/* 原始字节中继：in_fd → out_fd，直到 EOF。成功 0，失败 -1。 */
static int relay_bytes(int in_fd, int out_fd)
{
    unsigned char buf[256];
    ssize_t n;

    for (;;) {
        n = read(in_fd, buf, sizeof buf);
        if (n > 0) {
            if (write_all(out_fd, buf, n) < 0) {
                perror("write");
                return -1;
            }
            continue;
        }
        if (n == 0)
            return 0; /* EOF */
        if (errno == EINTR)
            continue;
        perror("read");
        return -1;
    }
}

int main(void)
{
    if (relay_bytes(STDIN_FILENO, STDOUT_FILENO) < 0)
        return 1;
    return 0;
}
