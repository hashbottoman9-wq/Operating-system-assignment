#include "include/eduos.h"

/* ─── Global PCB Table ─── */
PCB pcb_table[MAX_PROCESSES];
int pcb_count = 0;
pthread_mutex_t pcb_mutex = PTHREAD_MUTEX_INITIALIZER;

static pid_t pid_counter = 1000;

/* ─── Utility: state to string ─── */
const char *state_to_string(int state) {
    switch (state) {
        case STATE_NEW:        return "NEW";
        case STATE_READY:      return "READY";
        case STATE_RUNNING:    return "RUNNING";
        case STATE_WAITING:    return "WAITING";
        case STATE_TERMINATED: return "TERMINATED";
        default:               return "UNKNOWN";
    }
}

/* ─── Utility: find PCB index by PID ─── */
int find_pcb(pid_t pid) {
    for (int i = 0; i < pcb_count; i++) {
        if (pcb_table[i].pid == pid) return i;
    }
    return -1;
}

/* ─── Save PCB snapshot to JSON ─── */
void save_pcb_snapshot(void) {
    FILE *f = fopen("pcb_snapshot.json", "w");
    if (!f) { perror("fopen pcb_snapshot.json"); return; }

    fprintf(f, "[\n");
    for (int i = 0; i < pcb_count; i++) {
        PCB *p = &pcb_table[i];
        fprintf(f, "  {\n");
        fprintf(f, "    \"pid\": %d,\n",            (int)p->pid);
        fprintf(f, "    \"name\": \"%s\",\n",        p->name);
        fprintf(f, "    \"state\": \"%s\",\n",       state_to_string(p->state));
        fprintf(f, "    \"priority\": %d,\n",        p->priority);
        fprintf(f, "    \"burst_time\": %d,\n",      p->burst_time);
        fprintf(f, "    \"arrival_time\": %d,\n",    p->arrival_time);
        fprintf(f, "    \"remaining_time\": %d,\n",  p->remaining_time);
        fprintf(f, "    \"memory_req_kb\": %d,\n",   p->memory_req_kb);
        fprintf(f, "    \"thread_count\": %d,\n",    p->thread_count);
        fprintf(f, "    \"exit_code\": %d,\n",       p->exit_code);
        fprintf(f, "    \"parent_pid\": %d,\n",      (int)p->parent_pid);
        fprintf(f, "    \"owner_id\": %d,\n",        p->owner_id);
        fprintf(f, "    \"creation_time\": %ld\n",   (long)p->creation_time);
        fprintf(f, "  }%s\n", (i < pcb_count - 1) ? "," : "");
    }
    fprintf(f, "]\n");
    fclose(f);
}

/* ─── edu_fork ─── */
pid_t edu_fork(PCB *parent) {
    pthread_mutex_lock(&pcb_mutex);

    if (pcb_count >= MAX_PROCESSES) {
        fprintf(stderr, "[ERROR] PCB table full\n");
        pthread_mutex_unlock(&pcb_mutex);
        return -1;
    }

    PCB *child = &pcb_table[pcb_count++];
    memcpy(child, parent, sizeof(PCB));

    child->pid          = ++pid_counter;
    child->parent_pid   = parent->pid;
    child->state        = STATE_NEW;
    child->creation_time = time(NULL);
    child->exit_code    = 0;

    printf("[%ld] edu_fork: created child PID %d from parent PID %d | state=NEW\n",
           (long)time(NULL), (int)child->pid, (int)parent->pid);

    child->state = STATE_READY;
    printf("[%ld] edu_fork: PID %d state -> READY\n",
           (long)time(NULL), (int)child->pid);

    pthread_mutex_unlock(&pcb_mutex);
    save_pcb_snapshot();
    return child->pid;
}

/* ─── edu_exec ─── */
void edu_exec(pid_t pid, char *prog_name, int burst_time) {
    pthread_mutex_lock(&pcb_mutex);

    int idx = find_pcb(pid);
    if (idx == -1) {
        fprintf(stderr, "[ERROR] edu_exec: PID %d not found\n", (int)pid);
        pthread_mutex_unlock(&pcb_mutex);
        return;
    }

    strncpy(pcb_table[idx].name, prog_name, sizeof(pcb_table[idx].name) - 1);
    pcb_table[idx].burst_time     = burst_time;
    pcb_table[idx].remaining_time = burst_time;
    pcb_table[idx].state          = STATE_RUNNING;

    printf("[%ld] edu_exec: PID %d loaded program '%s' burst=%d | state=RUNNING\n",
           (long)time(NULL), (int)pid, prog_name, burst_time);

    pthread_mutex_unlock(&pcb_mutex);
    save_pcb_snapshot();
}

/* ─── edu_wait ─── */
int edu_wait(pid_t parent_pid) {
    printf("[%ld] edu_wait: PID %d waiting for children to terminate...\n",
           (long)time(NULL), (int)parent_pid);

    int idx = find_pcb(parent_pid);
    if (idx != -1) {
        pcb_table[idx].state = STATE_WAITING;
        save_pcb_snapshot();
    }

    /* Wait until all children are terminated */
    int all_done = 0;
    while (!all_done) {
        all_done = 1;
        pthread_mutex_lock(&pcb_mutex);
        for (int i = 0; i < pcb_count; i++) {
            if (pcb_table[i].parent_pid == parent_pid &&
                pcb_table[i].state != STATE_TERMINATED) {
                all_done = 0;
                break;
            }
        }
        pthread_mutex_unlock(&pcb_mutex);
        if (!all_done) usleep(100000);
    }

    /* Find last child exit code */
    int exit_code = 0;
    pthread_mutex_lock(&pcb_mutex);
    for (int i = 0; i < pcb_count; i++) {
        if (pcb_table[i].parent_pid == parent_pid) {
            exit_code = pcb_table[i].exit_code;
        }
    }
    if (idx != -1) pcb_table[idx].state = STATE_READY;
    pthread_mutex_unlock(&pcb_mutex);

    printf("[%ld] edu_wait: PID %d all children done, exit_code=%d\n",
           (long)time(NULL), (int)parent_pid, exit_code);
    return exit_code;
}

/* ─── edu_exit ─── */
void edu_exit(pid_t pid, int exit_code) {
    pthread_mutex_lock(&pcb_mutex);

    int idx = find_pcb(pid);
    if (idx == -1) {
        fprintf(stderr, "[ERROR] edu_exit: PID %d not found\n", (int)pid);
        pthread_mutex_unlock(&pcb_mutex);
        return;
    }

    pcb_table[idx].state     = STATE_TERMINATED;
    pcb_table[idx].exit_code = exit_code;

    printf("[%ld] edu_exit: PID %d terminated with exit_code=%d\n",
           (long)time(NULL), (int)pid, exit_code);

    pthread_mutex_unlock(&pcb_mutex);
    save_pcb_snapshot();
}

/* ─── edu_ps ─── */
void edu_ps(void) {
    pthread_mutex_lock(&pcb_mutex);
    printf("\n%-8s %-8s %-20s %-12s %-8s %-8s %-8s\n",
           "PID", "PPID", "NAME", "STATE", "PRIORITY", "BURST", "MEMORY");
    printf("%-8s %-8s %-20s %-12s %-8s %-8s %-8s\n",
           "---", "----", "----", "-----", "--------", "-----", "------");
    for (int i = 0; i < pcb_count; i++) {
        PCB *p = &pcb_table[i];
        printf("%-8d %-8d %-20s %-12s %-8d %-8d %-8d\n",
               (int)p->pid, (int)p->parent_pid, p->name,
               state_to_string(p->state), p->priority,
               p->burst_time, p->memory_req_kb);
    }
    printf("\n");
    pthread_mutex_unlock(&pcb_mutex);
}
