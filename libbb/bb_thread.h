#ifndef BB_THREAD_H
#define BB_THREAD_H

#include <pthread.h>
#include <stdint.h>

// Thread configuration
typedef struct {
    pthread_t    tid;
    const char  *name;
    void       *(*entry)(void *);
    void        *arg;
    int          cpu_affinity;     // -1 = any, 0-3 = A53 core
    int          sched_policy;     // SCHED_FIFO, SCHED_RR, SCHED_OTHER
    int          priority;         // 1-99 for RT, 0 for SCHED_OTHER
    size_t       stack_size;       // 0 = default
} bb_thread_t;

// Thread lifecycle
int  bb_thread_spawn(bb_thread_t *t);
int  bb_thread_join(bb_thread_t *t);
void bb_thread_set_name(const char *name);

// Set real-time scheduling for the calling thread
int  bb_thread_set_rt(int priority);

// Pin calling thread to a specific CPU core
int  bb_thread_pin(int cpu);

#endif // BB_THREAD_H
