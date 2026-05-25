#ifndef BB_POOL_H
#define BB_POOL_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

// Frame buffer descriptor (DMA-BUF backed for zero-copy hardware pipelines)
typedef struct {
    void        *data;          // mmap'd buffer
    size_t       size;
    int          fd;            // dma-buf fd (-1 if malloc-backed)
    atomic_int   refcount;
    uint64_t     pts;           // presentation timestamp (microseconds)
    uint32_t     frame_num;     // monotonic frame counter
    int          fourcc;        // pixel format (V4L2_PIX_FMT_*)
} bb_frame_t;

// Lock-free MPSC frame pool (multiple producers, single consumer variant)
// For SPSC (ingest→encode), use bb_pool_sp_acquire / bb_pool_sp_commit
// For MPSC (encode→stream, display), use bb_pool_mp_acquire / bb_pool_mp_enqueue
typedef struct {
    bb_frame_t   *frames;
    int           count;
    atomic_int    write_idx;    // SPSC: producer index
    atomic_int    read_idx;     // SPSC: consumer index
    pthread_mutex_t mp_mutex;   // MPSC: protect multi-producer enqueue
    pthread_cond_t  mp_cond;    // MPSC: signal consumer
    atomic_int    mp_pending;   // MPSC: frames ready to consume
} bb_frame_pool_t;

// Pool lifecycle
int  bb_pool_init(bb_frame_pool_t *p, int count, size_t frame_size, int use_dmabuf);
void bb_pool_destroy(bb_frame_pool_t *p);

// SPSC (single producer, single consumer) — lock-free path
bb_frame_t *bb_pool_sp_acquire(bb_frame_pool_t *p);   // producer: get empty slot
void         bb_pool_sp_commit(bb_frame_pool_t *p);     // producer: mark ready
bb_frame_t *bb_pool_sp_dequeue(bb_frame_pool_t *p);    // consumer: get ready frame
void         bb_pool_sp_release(bb_frame_t *f);         // consumer: mark done

// MPSC (multi producer, single consumer) — mutex-protected
bb_frame_t *bb_pool_mp_acquire(bb_frame_pool_t *p);    // producer: get empty slot
void         bb_pool_mp_enqueue(bb_frame_pool_t *p, bb_frame_t *f); // producer: push ready
bb_frame_t *bb_pool_mp_dequeue(bb_frame_pool_t *p);    // consumer: block until ready
void         bb_pool_mp_release(bb_frame_t *f);         // consumer: mark done

// Refcount for shared frames (display + stream both consuming same raw frame)
void bb_frame_ref(bb_frame_t *f);
void bb_frame_unref(bb_frame_t *f);

#endif // BB_POOL_H
