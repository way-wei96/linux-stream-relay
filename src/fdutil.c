#include "fdutil.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int set_nonblock(int any_fd)
{
    int flags = fcntl(any_fd, F_GETFL);

    if (flags < 0)
        return -1;
    if (fcntl(any_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;
    return 0;
}

void close_if_opened(int io_fd, int stdio_fd)
{
    if (io_fd != stdio_fd)
        close(io_fd);
}

int open_input(const char *path)
{
    int in_fd;

    if (path == NULL || strcmp(path, "-") == 0)
        return STDIN_FILENO;

    in_fd = open(path, O_RDONLY);
    if (in_fd < 0) {
        perror("open");
        return -1;
    }
    return in_fd;
}

int open_output(const char *path)
{
    int out_fd;

    if (path == NULL || strcmp(path, "-") == 0)
        return STDOUT_FILENO;

    out_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out_fd < 0) {
        perror("open");
        return -1;
    }
    return out_fd;
}
