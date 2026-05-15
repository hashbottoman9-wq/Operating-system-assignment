#include "include/eduos.h"

#define SHM_NAME "/eduos_shm"

/* ════════════════════════════════════════
   SHARED MEMORY IPC
   ════════════════════════════════════════ */
void demo_shared_memory(int requester_owner_id) {
    printf("\n[SHM] Starting shared memory demo...\n");

    /* Create shared memory */
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) { perror("shm_open"); return; }

    if (ftruncate(fd, sizeof(SharedMetrics)) == -1) {
        perror("ftruncate"); close(fd); return;
    }

    SharedMetrics *shm = (SharedMetrics *)mmap(NULL, sizeof(SharedMetrics),
                          PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); close(fd); return; }

    /* Initialize mutex in shared memory */
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm->shm_mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    /* Write data */
    pthread_mutex_lock(&shm->shm_mutex);
    shm->owner_id     = 42;
    shm->pid          = 1001;
    strncpy(shm->name, "TestProcess", sizeof(shm->name) - 1);
    shm->state        = STATE_RUNNING;
    shm->burst_time   = 10;
    shm->remaining_time = 7;
    pthread_mutex_unlock(&shm->shm_mutex);

    printf("[SHM] Written: pid=%d name=%s state=%s\n",
           shm->pid, shm->name, state_to_string(shm->state));

    /* Access control check */
    pthread_mutex_lock(&shm->shm_mutex);
    if (shm->owner_id != requester_owner_id) {
        printf("[SHM] ACCESS DENIED: requester owner_id=%d does not match owner_id=%d\n",
               requester_owner_id, shm->owner_id);
    } else {
        printf("[SHM] ACCESS GRANTED: Reading -> pid=%d burst=%d remaining=%d\n",
               shm->pid, shm->burst_time, shm->remaining_time);
    }
    pthread_mutex_unlock(&shm->shm_mutex);

    /* Cleanup */
    pthread_mutex_destroy(&shm->shm_mutex);
    munmap(shm, sizeof(SharedMetrics));
    close(fd);
    shm_unlink(SHM_NAME);
    printf("[SHM] Shared memory demo complete\n\n");
}

/* ════════════════════════════════════════
   ANONYMOUS PIPE IPC
   ════════════════════════════════════════ */
void demo_pipe_ipc(void) {
    printf("\n[PIPE] Starting anonymous pipe demo...\n");

    int pipefd[2];
    if (pipe(pipefd) == -1) { perror("pipe"); return; }

    /* Serialize a PCB to send through pipe */
    PCB send_pcb;
    send_pcb.pid            = 2001;
    send_pcb.parent_pid     = 1000;
    send_pcb.priority       = 2;
    send_pcb.burst_time     = 8;
    send_pcb.arrival_time   = 0;
    send_pcb.remaining_time = 8;
    send_pcb.memory_req_kb  = 512;
    send_pcb.thread_count   = 2;
    send_pcb.state          = STATE_READY;
    send_pcb.exit_code      = 0;
    send_pcb.owner_id       = 42;
    strncpy(send_pcb.name, "PipeTestProc", sizeof(send_pcb.name) - 1);

    pid_t pid = fork();
    if (pid == -1) { perror("fork"); return; }

    if (pid == 0) {
        /* Child: read from pipe */
        close(pipefd[1]);
        PCB recv_pcb;
        ssize_t n = read(pipefd[0], &recv_pcb, sizeof(PCB));
        if (n != sizeof(PCB)) {
            fprintf(stderr, "[PIPE] Child: read error\n");
            close(pipefd[0]);
            _exit(1);
        }
        printf("[PIPE] Child received: pid=%d name=%s state=%s burst=%d\n",
               (int)recv_pcb.pid, recv_pcb.name,
               state_to_string(recv_pcb.state), recv_pcb.burst_time);
        close(pipefd[0]);
        _exit(0);
    } else {
        /* Parent: write to pipe */
        close(pipefd[0]);
        ssize_t n = write(pipefd[1], &send_pcb, sizeof(PCB));
        if (n != sizeof(PCB)) perror("write pipe");
        else printf("[PIPE] Parent sent PCB: pid=%d name=%s\n",
                    (int)send_pcb.pid, send_pcb.name);
        close(pipefd[1]);
        wait(NULL);
    }
    printf("[PIPE] Pipe IPC demo complete\n\n");
}
