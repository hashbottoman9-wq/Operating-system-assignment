#define _XOPEN_SOURCE 600
#include "include/eduos.h"
#include <unistd.h>

/* ─── Shared counter for race condition demo ─── */
static int shared_counter = 0;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ─── Semaphore for producer-consumer ─── */
static sem_t empty_slots;
static sem_t filled_slots;
static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
#define BUFFER_SIZE 5
static int buffer[BUFFER_SIZE];
static int buf_in  = 0;
static int buf_out = 0;

/* ════════════════════════════════════════
   THREAD POOL
   ════════════════════════════════════════ */

static void *worker_thread(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;
    while (1) {
        pthread_mutex_lock(&pool->queue_mutex);
        while (pool->task_queue_head == NULL && !pool->shutdown)
            pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);

        if (pool->shutdown && pool->task_queue_head == NULL) {
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }

        Task *task = pool->task_queue_head;
        if (task) {
            pool->task_queue_head = task->next;
            if (pool->task_queue_head == NULL)
                pool->task_queue_tail = NULL;
            pool->task_count--;
        }
        pthread_mutex_unlock(&pool->queue_mutex);

        if (task) {
            task->function(task->arg);
            free(task);
        }
    }
    return NULL;
}

ThreadPool *thread_pool_create(void) {
    ThreadPool *pool = (ThreadPool *)malloc(sizeof(ThreadPool));
    if (!pool) { perror("malloc ThreadPool"); return NULL; }

    pool->task_queue_head = NULL;
    pool->task_queue_tail = NULL;
    pool->shutdown        = 0;
    pool->task_count      = 0;

    if (pthread_mutex_init(&pool->queue_mutex, NULL) != 0) {
        perror("pthread_mutex_init"); free(pool); return NULL;
    }
    if (pthread_cond_init(&pool->queue_cond, NULL) != 0) {
        perror("pthread_cond_init"); free(pool); return NULL;
    }

    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            perror("pthread_create"); free(pool); return NULL;
        }
    }
    printf("[ThreadPool] Created pool with %d worker threads\n", THREAD_POOL_SIZE);
    return pool;
}

void thread_pool_submit(ThreadPool *pool, void (*func)(void *), void *arg) {
    Task *task = (Task *)malloc(sizeof(Task));
    if (!task) { perror("malloc Task"); return; }
    task->function = func;
    task->arg      = arg;
    task->next     = NULL;

    pthread_mutex_lock(&pool->queue_mutex);
    if (pool->task_queue_tail)
        pool->task_queue_tail->next = task;
    else
        pool->task_queue_head = task;
    pool->task_queue_tail = task;
    pool->task_count++;
    pthread_cond_signal(&pool->queue_cond);
    pthread_mutex_unlock(&pool->queue_mutex);
}

void thread_pool_destroy(ThreadPool *pool) {
    pthread_mutex_lock(&pool->queue_mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->queue_cond);
    pthread_mutex_unlock(&pool->queue_mutex);

    for (int i = 0; i < THREAD_POOL_SIZE; i++)
        pthread_join(pool->threads[i], NULL);

    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->queue_cond);
    free(pool);
    printf("[ThreadPool] Destroyed pool gracefully\n");
}

/* ════════════════════════════════════════
   RACE CONDITION DEMO (no mutex)
   Compile with: make race
   ════════════════════════════════════════ */
static void *increment_no_mutex(void *arg) {
    (void)arg;
    for (int i = 0; i < 100000; i++)
        shared_counter++;   /* intentional data race */
    return NULL;
}

void demo_race_condition(void) {
    shared_counter = 0;
    pthread_t t[4];
    printf("\n[RACE] Starting race condition demo (NO mutex)...\n");
    for (int i = 0; i < 4; i++)
        pthread_create(&t[i], NULL, increment_no_mutex, NULL);
    for (int i = 0; i < 4; i++)
        pthread_join(t[i], NULL);
    printf("[RACE] Expected: 400000 | Got: %d (non-deterministic)\n\n",
           shared_counter);
}

/* ════════════════════════════════════════
   MUTEX FIXED DEMO
   Compile with: make fixed
   ════════════════════════════════════════ */
static void *increment_with_mutex(void *arg) {
    (void)arg;
    for (int i = 0; i < 100000; i++) {
        pthread_mutex_lock(&counter_mutex);
        shared_counter++;
        pthread_mutex_unlock(&counter_mutex);
    }
    return NULL;
}

void demo_mutex_fixed(void) {
    shared_counter = 0;
    pthread_t t[4];
    printf("\n[FIXED] Starting mutex-protected demo...\n");
    for (int i = 0; i < 4; i++)
        pthread_create(&t[i], NULL, increment_with_mutex, NULL);
    for (int i = 0; i < 4; i++)
        pthread_join(t[i], NULL);
    printf("[FIXED] Expected: 400000 | Got: %d (always correct)\n\n",
           shared_counter);
}

/* ════════════════════════════════════════
   PRODUCER-CONSUMER (semaphore)
   ════════════════════════════════════════ */
static void *producer(void *arg) {
    (void)arg;
    for (int i = 0; i < 10; i++) {
        sem_wait(&empty_slots);
        pthread_mutex_lock(&buffer_mutex);
        buffer[buf_in] = i;
        buf_in = (buf_in + 1) % BUFFER_SIZE;
        printf("[PRODUCER] produced item %d\n", i);
        pthread_mutex_unlock(&buffer_mutex);
        sem_post(&filled_slots);
    }
    return NULL;
}

static void *consumer(void *arg) {
    (void)arg;
    for (int i = 0; i < 10; i++) {
        sem_wait(&filled_slots);
        pthread_mutex_lock(&buffer_mutex);
        int item = buffer[buf_out];
        buf_out = (buf_out + 1) % BUFFER_SIZE;
        printf("[CONSUMER] consumed item %d\n", item);
        pthread_mutex_unlock(&buffer_mutex);
        sem_post(&empty_slots);
    }
    return NULL;
}

void demo_producer_consumer(void) {
    printf("\n[PC] Starting producer-consumer demo...\n");
    sem_init(&empty_slots, 0, BUFFER_SIZE);
    sem_init(&filled_slots, 0, 0);

    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer, NULL);
    pthread_create(&cons, NULL, consumer, NULL);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    sem_destroy(&empty_slots);
    sem_destroy(&filled_slots);
    printf("[PC] Producer-consumer demo complete\n\n");
}

/* ════════════════════════════════════════
   DEADLOCK DEMO & FIX
   Two mutexes acquired in consistent order
   to prevent deadlock
   ════════════════════════════════════════ */
static pthread_mutex_t mutex_A = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_B = PTHREAD_MUTEX_INITIALIZER;

static void *thread_lock_AB(void *arg) {
    (void)arg;
    /* Always lock A then B — consistent order prevents deadlock */
    pthread_mutex_lock(&mutex_A);
    printf("[DEADLOCK-FIX] Thread 1 locked A, waiting for B...\n");
    usleep(1000);
    pthread_mutex_lock(&mutex_B);
    printf("[DEADLOCK-FIX] Thread 1 locked B\n");
    pthread_mutex_unlock(&mutex_B);
    pthread_mutex_unlock(&mutex_A);
    return NULL;
}

static void *thread_lock_AB2(void *arg) {
    (void)arg;
    /* Also locks A then B — same order, no deadlock */
    pthread_mutex_lock(&mutex_A);
    printf("[DEADLOCK-FIX] Thread 2 locked A, waiting for B...\n");
    usleep(1000);
    pthread_mutex_lock(&mutex_B);
    printf("[DEADLOCK-FIX] Thread 2 locked B\n");
    pthread_mutex_unlock(&mutex_B);
    pthread_mutex_unlock(&mutex_A);
    return NULL;
}

void demo_deadlock_fix(void) {
    printf("\n[DEADLOCK-FIX] Demonstrating deadlock prevention...\n");
    printf("[DEADLOCK-FIX] Both threads lock mutex_A before mutex_B\n");
    pthread_t t1, t2;
    pthread_create(&t1, NULL, thread_lock_AB,  NULL);
    pthread_create(&t2, NULL, thread_lock_AB2, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("[DEADLOCK-FIX] No deadlock occurred!\n\n");
}
