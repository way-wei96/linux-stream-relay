#include "relay.h"

#include "poll_relay.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

/*
 * 本半步练习用的共享状态：两线程都只通过 mutex 碰计数。
 * 还没有 cond，也没有改 IO 模型。
 */
struct shared {
    pthread_mutex_t mu;
    unsigned long relay_loops; /* 中继线程：每轮 poll 循环 +1 */
    unsigned long side_count;  /* 旁路线程：空转时 +1 */
};

struct relay_arg {
    int in_fd;
    int out_fd;
    int status;
    struct shared *shared;
};

static void shared_inc_relay_loops(void *arg)
{
    struct shared *s = arg;

    pthread_mutex_lock(&s->mu);
    s->relay_loops++;
    pthread_mutex_unlock(&s->mu);
}

static void shared_inc_side_count(struct shared *s)
{
    pthread_mutex_lock(&s->mu);
    s->side_count++;
    pthread_mutex_unlock(&s->mu);
}

static void *relay_task(void *arg)
{
    struct relay_arg *a = arg;

    a->status = poll_relay(a->in_fd, a->out_fd,
                           shared_inc_relay_loops, a->shared);
    return NULL;
}

/* 旁路任务：反复在同一把锁上给 side_count +1，不碰 IO。 */
static void *side_task(void *arg)
{
    struct shared *s = arg;
    int i;

    for (i = 0; i < 10000; i++)
        shared_inc_side_count(s);
    return NULL;
}

int relay_bytes(int in_fd, int out_fd)
{
    struct shared shared;
    struct relay_arg arg;
    pthread_t relay_tid;
    pthread_t side_tid;
    int have_relay = 0;
    int have_side = 0;
    int status = -1;

    memset(&shared, 0, sizeof shared);
    pthread_mutex_init(&shared.mu, NULL);

    arg.in_fd = in_fd;
    arg.out_fd = out_fd;
    arg.status = -1;
    arg.shared = &shared;

    if (pthread_create(&relay_tid, NULL, relay_task, &arg) != 0) {
        perror("pthread_create");
        goto cleanup;
    }
    have_relay = 1;

    if (pthread_create(&side_tid, NULL, side_task, &shared) != 0) {
        perror("pthread_create");
        goto cleanup;
    }
    have_side = 1;
    goto cleanup;

cleanup:
    if (have_relay)
        pthread_join(relay_tid, NULL);
    if (have_side)
        pthread_join(side_tid, NULL);
    if (have_relay && have_side)
        status = arg.status;

    (void)shared.relay_loops;
    (void)shared.side_count;
    pthread_mutex_destroy(&shared.mu);
    return status;
}
