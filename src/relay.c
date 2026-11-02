#include "relay.h"

#include "ringbuf.h"
#include "stop_flag.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

/*
 * 读、写线程共用：endpoint_fd 对读线程是 in_fd，对写线程是 out_fd。
 */
struct worker_arg {
    int endpoint_fd;
    struct ringbuf *shared_ring;
    int task_status; /* 0 成功，-1 失败 */
};

static void worker_arg_init(struct worker_arg *arg, int endpoint_fd,
                            struct ringbuf *shared_ring)
{
    arg->endpoint_fd = endpoint_fd;
    arg->shared_ring = shared_ring;
    arg->task_status = -1;
}

static void worker_finish(struct worker_arg *task, int task_status)
{
    task->task_status = task_status;
}

/* 读/写一步的结果；数值保持 1/0/-1，便于 while (step(...) > 0)。 */
enum step_result {
    STEP_FAILED = -1,
    STEP_DONE = 0,
    STEP_CONTINUE = 1
};

/* 把 buf 里 nbytes 全部写到 out_fd；短写则继续。成功 0，失败 -1。 */
static int write_fully(int out_fd, const uint8_t *buf, size_t nbytes)
{
    size_t bytes_written = 0;

    while (bytes_written < nbytes) {
        ssize_t nwritten = write(out_fd, buf + bytes_written,
                                 nbytes - bytes_written);

        if (nwritten < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        bytes_written += (size_t)nwritten;
    }
    return 0;
}

static void reader_stop_gracefully(struct worker_arg *task)
{
    ringbuf_mark_producer_eof(task->shared_ring);
    worker_finish(task, 0);
}

/*
 * 读线程一步。本半步：只在「开读前 / read 被信号打断」时看停止标志。
 */
static enum step_result reader_step(struct worker_arg *task, uint8_t *chunk,
                                    size_t chunk_cap)
{
    ssize_t nread;

    if (stop_flag_is_set()) {
        reader_stop_gracefully(task);
        return STEP_DONE;
    }

    nread = read(task->endpoint_fd, chunk, chunk_cap);

    if (nread < 0) {
        if (errno == EINTR) {
            if (stop_flag_is_set()) {
                reader_stop_gracefully(task);
                return STEP_DONE;
            }
            return STEP_CONTINUE;
        }
        perror("read");
        ringbuf_mark_error(task->shared_ring);
        worker_finish(task, -1);
        return STEP_FAILED;
    }
    if (nread == 0) {
        reader_stop_gracefully(task);
        return STEP_DONE;
    }
    if (ringbuf_push(task->shared_ring, chunk, (size_t)nread) < 0) {
        worker_finish(task, -1);
        return STEP_FAILED;
    }
    return STEP_CONTINUE;
}

/* 写线程一步。本半步尚不查停止标志。 */
static enum step_result writer_step(struct worker_arg *task, uint8_t *chunk,
                                    size_t chunk_cap)
{
    ssize_t nbytes_from_ring = ringbuf_pop(task->shared_ring, chunk, chunk_cap);

    if (nbytes_from_ring < 0) {
        worker_finish(task, -1);
        return STEP_FAILED;
    }
    if (nbytes_from_ring == 0) {
        worker_finish(task, 0);
        return STEP_DONE;
    }
    if (write_fully(task->endpoint_fd, chunk, (size_t)nbytes_from_ring) < 0) {
        perror("write");
        ringbuf_mark_error(task->shared_ring);
        worker_finish(task, -1);
        return STEP_FAILED;
    }
    return STEP_CONTINUE;
}

static void *reader_task(void *arg)
{
    struct worker_arg *task = arg;
    uint8_t chunk[1024];

    while (reader_step(task, chunk, sizeof chunk) > 0) {
    }
    return NULL;
}

static void *writer_task(void *arg)
{
    struct worker_arg *task = arg;
    uint8_t chunk[1024];

    while (writer_step(task, chunk, sizeof chunk) > 0) {
    }
    return NULL;
}

static int start_worker(pthread_t *tid, void *(*task)(void *),
                        struct worker_arg *arg)
{
    if (pthread_create(tid, NULL, task, arg) != 0) {
        perror("pthread_create");
        return -1;
    }
    return 0;
}

static int both_workers_ok(int reader_started, int writer_started,
                           const struct worker_arg *reader_arg,
                           const struct worker_arg *writer_arg)
{
    return reader_started && writer_started &&
           reader_arg->task_status == 0 && writer_arg->task_status == 0;
}

int relay_bytes(int in_fd, int out_fd)
{
    struct ringbuf shared_ring;
    struct worker_arg reader_arg;
    struct worker_arg writer_arg;
    pthread_t reader_tid;
    pthread_t writer_tid;
    int reader_started = 0;
    int writer_started = 0;
    int relay_status = -1;

    if (ringbuf_init(&shared_ring) < 0) {
        perror("ringbuf_init");
        return -1;
    }

    worker_arg_init(&reader_arg, in_fd, &shared_ring);
    worker_arg_init(&writer_arg, out_fd, &shared_ring);

    if (start_worker(&reader_tid, reader_task, &reader_arg) < 0)
        goto cleanup;
    reader_started = 1;

    if (start_worker(&writer_tid, writer_task, &writer_arg) < 0) {
        ringbuf_mark_error(&shared_ring);
        goto cleanup;
    }
    writer_started = 1;
    goto cleanup;

cleanup:
    if (reader_started)
        pthread_join(reader_tid, NULL);
    if (writer_started)
        pthread_join(writer_tid, NULL);

    if (both_workers_ok(reader_started, writer_started, &reader_arg, &writer_arg))
        relay_status = 0;

    ringbuf_destroy(&shared_ring);
    return relay_status;
}
