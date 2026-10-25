#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
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

/*
 * 决定从哪读：path == NULL 或 "-" 用 stdin，否则 open 文件。
 * 成功返回 fd；失败返回 -1。
 * 调用方：若返回值不是 STDIN_FILENO，用完需 close。
 */
static int open_input(const char *path)
{
    int fd;

    if (path == NULL || strcmp(path, "-") == 0)
        return STDIN_FILENO;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }
    return fd;
}

/*
 * 决定写到哪：path == NULL 或 "-" 用 stdout，否则创建/截断文件。
 * 成功返回 fd；失败返回 -1。
 * 调用方：若返回值不是 STDOUT_FILENO，用完需 close。
 */
static int open_output(const char *path)
{
    int fd;

    if (path == NULL || strcmp(path, "-") == 0)
        return STDOUT_FILENO;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open");
        return -1;
    }
    return fd;
}

int main(int argc, char **argv)
{
    const char *in_path = NULL;
    const char *out_path = NULL;
    int in_fd;
    int out_fd;
    int status = 0;

    if (argc > 3) {
        fprintf(stderr, "usage: %s [infile [outfile]]\n", argv[0]);
        return 1;
    }
    if (argc >= 2)
        in_path = argv[1];
    if (argc == 3)
        out_path = argv[2];

    in_fd = open_input(in_path);
    if (in_fd < 0)
        return 1;

    out_fd = open_output(out_path);
    if (out_fd < 0) {
        if (in_fd != STDIN_FILENO)
            close(in_fd);
        return 1;
    }

    if (relay_bytes(in_fd, out_fd) < 0)
        status = 1;

    if (in_fd != STDIN_FILENO)
        close(in_fd);
    if (out_fd != STDOUT_FILENO)
        close(out_fd);

    return status;
}
