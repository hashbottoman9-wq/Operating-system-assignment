#include "include/eduos.h"

int main(void) {
    printf("╔══════════════════════════════════════╗\n");
    printf("║        EduOS Simulator v1.0          ║\n");
    printf("║  Student: Hope Bottoman              ║\n");
    printf("║  Reg No:  25-311-351-026             ║\n");
    printf("╚══════════════════════════════════════╝\n\n");

    /* ─── 1. Process Management Demo ─── */
    printf("═══════════════════════════════════════\n");
    printf(" PART 1: PROCESS MANAGEMENT\n");
    printf("═══════════════════════════════════════\n");

    /* Create parent PCB */
    PCB parent;
    parent.pid            = 1000;
    parent.parent_pid     = 0;
    parent.priority       = 1;
    parent.burst_time     = 20;
    parent.arrival_time   = 0;
    parent.remaining_time = 20;
    parent.memory_req_kb  = 1024;
    parent.thread_count   = 1;
    parent.state          = STATE_READY;
    parent.exit_code      = 0;
    parent.owner_id       = 42;
    parent.creation_time  = time(NULL);
    strncpy(parent.name, "init", sizeof(parent.name) - 1);

    /* Add parent to PCB table */
    pthread_mutex_lock(&pcb_mutex);
    pcb_table[pcb_count++] = parent;
    pthread_mutex_unlock(&pcb_mutex);

    /* Fork two children */
    pid_t child1 = edu_fork(&parent);
    pid_t child2 = edu_fork(&parent);

    /* Exec on children */
    edu_exec(child1, "calculator", 5);
    edu_exec(child2, "text_editor", 8);

    /* Print process table */
    edu_ps();

    /* Exit children */
    edu_exit(child1, 0);
    edu_exit(child2, 0);

    /* Parent waits */
    edu_wait(parent.pid);

    /* Exit parent */
    edu_exit(parent.pid, 0);

    /* Final ps */
    edu_ps();

    /* ─── 2. Thread Pool Demo ─── */
    printf("═══════════════════════════════════════\n");
    printf(" PART 2: THREAD POOL\n");
    printf("═══════════════════════════════════════\n");

    ThreadPool *pool = thread_pool_create();

    /* Submit some tasks */
    for (int i = 0; i < 8; i++) {
        int *arg = malloc(sizeof(int));
        if (!arg) { perror("malloc"); continue; }
        *arg = i;
        thread_pool_submit(pool, (void (*)(void *))free, arg);
    }

    sleep(1);
    thread_pool_destroy(pool);

    /* ─── 3. Race Condition Demo ─── */
    printf("═══════════════════════════════════════\n");
    printf(" PART 3: RACE CONDITION & MUTEX FIX\n");
    printf("═══════════════════════════════════════\n");

    demo_race_condition();
    demo_mutex_fixed();

    /* ─── 4. Producer Consumer Demo ─── */
    printf("═══════════════════════════════════════\n");
    printf(" PART 4: PRODUCER-CONSUMER\n");
    printf("═══════════════════════════════════════\n");

    demo_producer_consumer();

    /* ─── 5. Deadlock Fix Demo ─── */
    printf("═══════════════════════════════════════\n");
    printf(" PART 5: DEADLOCK PREVENTION\n");
    printf("═══════════════════════════════════════\n");

    demo_deadlock_fix();

    /* ─── 6. IPC Demo ─── */
    printf("═══════════════════════════════════════\n");
    printf(" PART 6: IPC - SHARED MEMORY & PIPES\n");
    printf("═══════════════════════════════════════\n");

    demo_shared_memory(42);
    demo_shared_memory(99);
    demo_pipe_ipc();

    printf("═══════════════════════════════════════\n");
    printf(" EduOS Simulation Complete!\n");
    printf("═══════════════════════════════════════\n");

    return 0;
}
