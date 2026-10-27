#include "fdutil.h"
#include "relay.h"

#include <stdio.h>
#include <unistd.h>

/* 解析 [infile [outfile]]。成功 0，用法错 -1。 */
static int parse_args(int argc, char **argv,
                      const char **in_path, const char **out_path)
{
    *in_path = NULL;
    *out_path = NULL;

    if (argc > 3) {
        fprintf(stderr, "usage: %s [infile [outfile]]\n", argv[0]);
        return -1;
    }
    if (argc >= 2)
        *in_path = argv[1];
    if (argc == 3)
        *out_path = argv[2];
    return 0;
}

int main(int argc, char **argv)
{
    const char *in_path;
    const char *out_path;
    int in_fd;
    int out_fd;
    int status = 0;

    if (parse_args(argc, argv, &in_path, &out_path) < 0)
        return 1;

    in_fd = open_input(in_path);
    if (in_fd < 0)
        return 1;

    out_fd = open_output(out_path);
    if (out_fd < 0) {
        close_if_opened(in_fd, STDIN_FILENO);
        return 1;
    }

    if (set_nonblock(in_fd) < 0 || set_nonblock(out_fd) < 0) {
        perror("fcntl");
        status = 1;
    } else if (relay_bytes(in_fd, out_fd) < 0) {
        status = 1;
    }

    close_if_opened(in_fd, STDIN_FILENO);
    close_if_opened(out_fd, STDOUT_FILENO);
    return status;
}
