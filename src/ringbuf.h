#ifndef RINGBUF_H
#define RINGBUF_H

#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>

#define RINGBUF_CAP 4096

struct ringbuf {
    uint8_t data[RINGBUF_CAP];
    /* 环上的下标，不是 out_fd 的 write() */
    size_t consumer_index; /* 下一字节从环里取出的位置 */
    size_t producer_index; /* 下一字节写入环里的位置 */
    size_t byte_count;     /* 环内当前字节数 */
    int producer_eof;      /* 读线程已读到输入 EOF */
    int has_error;         /* 任一侧失败，两端都应停 */
    pthread_mutex_t lock;
    pthread_cond_t data_available;  /* 有数据可读，或已 EOF */
    pthread_cond_t space_available; /* 有空位可写 */
};

int ringbuf_init(struct ringbuf *ring);
void ringbuf_destroy(struct ringbuf *ring);

/* 生产者：写入 nbytes（可分多次等空位）。成功 0，失败 -1。 */
int ringbuf_push(struct ringbuf *ring, const uint8_t *src, size_t nbytes);

/*
 * 消费者：尽量取出最多 max_nbytes 字节到 dst。
 * 返回：>0 取出长度；0 生产者已 EOF 且环空；-1 失败。
 */
ssize_t ringbuf_pop(struct ringbuf *ring, uint8_t *dst, size_t max_nbytes);

void ringbuf_mark_producer_eof(struct ringbuf *ring);
void ringbuf_mark_error(struct ringbuf *ring);

#endif
