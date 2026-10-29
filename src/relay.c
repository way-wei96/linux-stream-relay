#include "relay.h"

#include "poll_relay.h"

#include <pthread.h>
#include <stdio.h>

struct relay_arg {
    int in_fd;
    int out_fd;
    int status;
};

static void *relay_task(void *arg)
{
    struct relay_arg *a = arg;

    a->status = poll_relay(a->in_fd, a->out_fd, NULL, NULL);
    return NULL;
}

int relay_bytes(int in_fd, int out_fd)
{
    struct relay_arg arg;
    pthread_t tid;

    arg.in_fd = in_fd;
    arg.out_fd = out_fd;
    arg.status = -1;

    if (pthread_create(&tid, NULL, relay_task, &arg) != 0) {
        perror("pthread_create");
        return -1;
    }
    pthread_join(tid, NULL);
    return arg.status;
}
