#include "ringbuf.h"

#include <string.h>

enum ring_copy_direction {
    RING_COPY_INTO, /* 外部 → 环（生产 / push） */
    RING_COPY_OUT   /* 环 → 外部（消费 / pop） */
};

static size_t min_size(size_t a, size_t b)
{
    return a < b ? a : b;
}

static size_t advance_index(size_t index, size_t nbytes)
{
    return (index + nbytes) % RINGBUF_CAP;
}

/* 从 start 起拷 nbytes：第一段到数组末尾，第二段绕回开头。 */
static void wrap_segments(size_t start_index, size_t nbytes,
                          size_t *first_len, size_t *second_len)
{
    *first_len = min_size(nbytes, RINGBUF_CAP - start_index);
    *second_len = nbytes - *first_len;
}

/*
 * 环形区与外部缓冲之间拷贝，并推进对应下标 / byte_count。
 * external：INTO 时为 src（按 const uint8_t * 用）；OUT 时为 dst。
 * 用 void * 是因为两种方向的指针 const 不同，无法共用一个 uint8_t *。
 * 调用方已持锁，且保证环上空间或数据足够。
 */
static void ring_copy(struct ringbuf *ring, void *external, size_t nbytes,
                      enum ring_copy_direction direction)
{
    size_t start_index = (direction == RING_COPY_INTO)
                             ? ring->producer_index
                             : ring->consumer_index;
    size_t first_len;
    size_t second_len;

    wrap_segments(start_index, nbytes, &first_len, &second_len);

    if (direction == RING_COPY_INTO) {
        const uint8_t *src = external;

        memcpy(ring->data + start_index, src, first_len);
        if (second_len > 0)
            memcpy(ring->data, src + first_len, second_len);
        ring->producer_index = advance_index(ring->producer_index, nbytes);
        ring->byte_count += nbytes;
    } else {
        uint8_t *dst = external;

        memcpy(dst, ring->data + start_index, first_len);
        if (second_len > 0)
            memcpy(dst + first_len, ring->data, second_len);
        ring->consumer_index = advance_index(ring->consumer_index, nbytes);
        ring->byte_count -= nbytes;
    }
}

/* 已持锁。等到有空位或出错。成功 0，失败 -1。 */
static int wait_for_space(struct ringbuf *ring)
{
    while (ring->byte_count == RINGBUF_CAP && !ring->has_error)
        pthread_cond_wait(&ring->space_available, &ring->lock);
    return ring->has_error ? -1 : 0;
}

/*
 * 已持锁。等到有数据、EOF 或出错。
 * 返回：1 有数据可取；0 EOF 且空；-1 失败。
 */
static int wait_for_data(struct ringbuf *ring)
{
    while (ring->byte_count == 0 && !ring->producer_eof && !ring->has_error)
        pthread_cond_wait(&ring->data_available, &ring->lock);
    if (ring->has_error)
        return -1;
    if (ring->byte_count == 0 && ring->producer_eof)
        return 0;
    return 1;
}

int ringbuf_init(struct ringbuf *ring)
{
    memset(ring, 0, sizeof *ring);
    if (pthread_mutex_init(&ring->lock, NULL) != 0)
        return -1;
    if (pthread_cond_init(&ring->data_available, NULL) != 0) {
        pthread_mutex_destroy(&ring->lock);
        return -1;
    }
    if (pthread_cond_init(&ring->space_available, NULL) != 0) {
        pthread_cond_destroy(&ring->data_available);
        pthread_mutex_destroy(&ring->lock);
        return -1;
    }
    return 0;
}

void ringbuf_destroy(struct ringbuf *ring)
{
    pthread_cond_destroy(&ring->space_available);
    pthread_cond_destroy(&ring->data_available);
    pthread_mutex_destroy(&ring->lock);
}

void ringbuf_mark_producer_eof(struct ringbuf *ring)
{
    pthread_mutex_lock(&ring->lock);
    ring->producer_eof = 1;
    pthread_cond_broadcast(&ring->data_available);
    pthread_mutex_unlock(&ring->lock);
}

void ringbuf_mark_error(struct ringbuf *ring)
{
    pthread_mutex_lock(&ring->lock);
    ring->has_error = 1;
    pthread_cond_broadcast(&ring->data_available);
    pthread_cond_broadcast(&ring->space_available);
    pthread_mutex_unlock(&ring->lock);
}

int ringbuf_push(struct ringbuf *ring, const uint8_t *src, size_t nbytes)
{
    size_t bytes_copied = 0;

    while (bytes_copied < nbytes) {
        size_t free_space;
        size_t chunk_nbytes;

        pthread_mutex_lock(&ring->lock);
        if (wait_for_space(ring) < 0) {
            pthread_mutex_unlock(&ring->lock);
            return -1;
        }
        free_space = RINGBUF_CAP - ring->byte_count;
        chunk_nbytes = min_size(nbytes - bytes_copied, free_space);
        ring_copy(ring, (void *)(src + bytes_copied), chunk_nbytes,
                  RING_COPY_INTO);
        bytes_copied += chunk_nbytes;
        pthread_cond_signal(&ring->data_available);
        pthread_mutex_unlock(&ring->lock);
    }
    return 0;
}

ssize_t ringbuf_pop(struct ringbuf *ring, uint8_t *dst, size_t max_nbytes)
{
    size_t chunk_nbytes;
    int wait_status;

    if (max_nbytes == 0)
        return 0;

    pthread_mutex_lock(&ring->lock);
    wait_status = wait_for_data(ring);
    if (wait_status <= 0) {
        pthread_mutex_unlock(&ring->lock);
        return wait_status;
    }

    chunk_nbytes = min_size(max_nbytes, ring->byte_count);
    ring_copy(ring, dst, chunk_nbytes, RING_COPY_OUT);
    pthread_cond_signal(&ring->space_available);
    pthread_mutex_unlock(&ring->lock);
    return (ssize_t)chunk_nbytes;
}
