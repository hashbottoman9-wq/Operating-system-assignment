#ifndef EDUOS_H
#define EDUOS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

/* ─── Process States ─── */
#define STATE_NEW        0
#define STATE_READY      1
#define STATE_RUNNING    2
#define STATE_WAITING    3
#define STATE_TERMINATED 4

/* ─── Thread Pool Size ─── */
#define THREAD_POOL_SIZE 4

/* ─── Max Processes ─── */
#define MAX_PROCESSES 64

/* ─── Process Control Block ─── */
typedef struct {
    pid_t  pid;
    char   name[64];
    int    state;
    int    priority;
    int    burst_time;
    int    arrival_time;
    int    remaining_time;
    int    memory_req_kb;
    int    thread_count;
    time_t creation_time;
    int    exit_code;
    pid_t  parent_pid;
    int    owner_id;
} PCB;

/* ─── PCB Table ─── */
extern PCB    pcb_table[MAX_PROCESSES];
extern int    pcb_count;
extern pthread_mutex_t pcb_mutex;

/* ─── Task for Thread Pool ─── */
typedef struct Task {
    void        (*function)(void *arg);
    void        *arg;
    struct Task *next;
} Task;

/* ─── Thread Pool ─── */
typedef struct {
    pthread_t       threads[THREAD_POOL_SIZE];
    Task           *task_queue_head;
    Task           *task_queue_tail;
    pthread_mutex_t queue_mutex;
    pthread_cond_t  queue_cond;
    int             shutdown;
    int             task_count;
} ThreadPool;

/* ─── Shared Memory Struct ─── */
typedef struct {
    int    owner_id;
    int    pid;
    char   name[64];
    int    state;
    int    burst_time;
    int    remaining_time;
    pthread_mutex_t shm_mutex;
} SharedMetrics;

/* ─── Function Declarations: process_manager.c ─── */
pid_t edu_fork(PCB *parent);
void  edu_exec(pid_t pid, char *prog_name, int burst_time);
int   edu_wait(pid_t parent_pid);
void  edu_exit(pid_t pid, int exit_code);
void  edu_ps(void);
void  save_pcb_snapshot(void);
int   find_pcb(pid_t pid);

/* ─── Function Declarations: thread_manager.c ─── */
ThreadPool *thread_pool_create(void);
void        thread_pool_submit(ThreadPool *pool, void (*func)(void *), void *arg);
void        thread_pool_destroy(ThreadPool *pool);
void        demo_race_condition(void);
void        demo_mutex_fixed(void);
void        demo_producer_consumer(void);
void        demo_deadlock_fix(void);

/* ─── Function Declarations: ipc_module.c ─── */
void demo_shared_memory(int owner_id);
void demo_pipe_ipc(void);

/* ─── Utility ─── */
const char *state_to_string(int state);

#endif /* EDUOS_H */
