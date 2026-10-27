#include "fdutil.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL);

    if (flags < 0)
        return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;
    return 0;
}

void close_if_opened(int fd, int std_fd)
{
    if (fd != std_fd)
        close(fd);
}

int open_input(const char *path)
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

int open_output(const char *path)
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
