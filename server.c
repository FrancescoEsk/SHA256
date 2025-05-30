// server.c – Riceve richieste da client e gestisce il dispatch ai worker

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "ipc/shm_utils.h"
#include "ipc/sem_utils.h"
#include "ipc/msg_utils.h"
#include "hash/sha256_utils.h"

#define SHM_KEY 0x1234
#define MSG_KEY 0x5678
#define SEM_KEY 0x9ABC
#define MAX_WORKERS 5       // valore iniziale, modificabile da control client
#define SEM_MEM 0           // semaforo per accesso memoria
#define SEM_PROC 1          // semaforo per numero processi attivi
#define TMP_PATH_LEN 256
#define MAX_UPLOADS 64

// Variabili globali
int msgid, shmid, semid;
int max_workers = MAX_WORKERS;

// Struttura per tracciare lo stato di upload per ogni client
struct upload_state {
    pid_t pid;
    char tmp_path[TMP_PATH_LEN];
    size_t received_chunks;
    size_t total_chunks;
};
struct upload_state uploads[MAX_UPLOADS];

// Coda prioritaria per richieste pendenti (ordinata per filesize crescente)
struct pending_request {
    struct message req;
    size_t filesize;
};
#define PENDING_QUEUE_SIZE 16
struct pending_request pending_queue[PENDING_QUEUE_SIZE];
int pending_count = 0;

void enqueue_pending(const struct message* req) {
    // Inserimento in ordine crescente di filesize
    int i = pending_count - 1;
    while (i >= 0 && pending_queue[i].filesize > req->filesize) {
        pending_queue[i+1] = pending_queue[i];
        i--;
    }
    pending_queue[i+1].req = *req;
    pending_queue[i+1].filesize = req->filesize;
    pending_count++;
}

int dequeue_pending(struct message* out) {
    if (pending_count == 0) return 0;
    *out = pending_queue[0].req;
    for (int i = 1; i < pending_count; ++i) {
        pending_queue[i-1] = pending_queue[i];
    }
    pending_count--;
    return 1;
}

// Signal handler per cleanup
void handle_sigint(int sig) {
    (void)sig;
    remove_message_queue(msgid);
    remove_shared_memory(shmid);
    semctl(semid, 0, IPC_RMID);
    printf("\nRisorse IPC rimosse. Server terminato.\n");
    exit(0);
}

// Trova o crea uno stato di upload per un dato pid
struct upload_state* get_upload_state(pid_t pid, unsigned int total_chunks) {
    for (int i = 0; i < MAX_UPLOADS; ++i) {
        if (uploads[i].pid == pid)
            return &uploads[i];
    }
    for (int i = 0; i < MAX_UPLOADS; ++i) {
        if (uploads[i].pid == 0) {
            uploads[i].pid = pid;
            snprintf(uploads[i].tmp_path, TMP_PATH_LEN, "/tmp/sha256_tmp_%d", pid);
            uploads[i].received_chunks = 0;
            uploads[i].total_chunks = total_chunks;
            return &uploads[i];
        }
    }
    return NULL;
}

void clear_upload_state(pid_t pid) {
    for (int i = 0; i < MAX_UPLOADS; ++i) {
        if (uploads[i].pid == pid) {
            uploads[i].pid = 0;
            uploads[i].tmp_path[0] = '\0';
            uploads[i].received_chunks = 0;
            uploads[i].total_chunks = 0;
        }
    }
}

// main server
int main() {
    // 1. Setup handler SIGINT per cleanup finale
    printf("[SERVER] Avvio e inizializzazione risorse IPC...\n");
    signal(SIGINT, handle_sigint);
    // 2. Inizializza coda messaggi (msgget)
    msgid = create_message_queue(MSG_KEY);
    // 3. Inizializza memoria condivisa (shmget)
    shmid = create_shared_memory(SHM_KEY, 65536);
    // 4. Inizializza semafori (semget + semctl)
    semid = create_semaphore_set(SEM_KEY, 2); // 2 semafori: memoria e worker
    semctl(semid, SEM_MEM, SETVAL, 1);
    semctl(semid, SEM_PROC, SETVAL, max_workers);
    memset(uploads, 0, sizeof(uploads));
    printf("[SERVER] In ascolto di richieste client...\n");

    int last_pid = -1;
    unsigned int last_chunk = 0, last_total = 0;

    // 5. Loop principale:
    while (1) {
        struct message req;
        // SCHEDULAZIONE: se ci sono richieste pendenti, processa SEMPRE la più grande tra quelle pendenti e la richiesta corrente
        int workers_free = semctl(semid, SEM_PROC, GETVAL);
        if (pending_count > 0 && workers_free > 0) {
            // Trova la richiesta più grande tra la coda e la nuova richiesta (se presente)
            struct message *to_dispatch = NULL;
            int max_idx = -1;
            size_t max_size = 0;
            for (int i = 0; i < pending_count; ++i) {
                if (pending_queue[i].filesize > max_size) {
                    max_size = pending_queue[i].filesize;
                    max_idx = i;
                }
            }
            // Leggi la nuova richiesta
            if (receive_message(msgid, 1, &req) != -1) {
                if (req.mtype == 99) {
                    max_workers = (int)req.filesize;
                    semctl(semid, SEM_PROC, SETVAL, max_workers);
                    printf("[SERVER] Aggiornato max_workers a %d\n", max_workers);
                    continue;
                }
                if (req.filesize > max_size) {
                    // La nuova richiesta è la più grande
                    to_dispatch = &req;
                } else {
                    // La più grande è nella coda
                    to_dispatch = &pending_queue[max_idx].req;
                    // Rimuovi dalla coda
                    for (int j = max_idx + 1; j < pending_count; ++j) {
                        pending_queue[j-1] = pending_queue[j];
                    }
                    pending_count--;
                    // Accoda la nuova richiesta
                    enqueue_pending(&req);
                }
            } else {
                // Nessuna nuova richiesta, prendi la più grande dalla coda
                to_dispatch = &pending_queue[max_idx].req;
                for (int j = max_idx + 1; j < pending_count; ++j) {
                    pending_queue[j-1] = pending_queue[j];
                }
                pending_count--;
            }
            // Dispatch della richiesta selezionata
            sem_wait(semid, SEM_PROC);
            printf("[SERVER] Dispatch richiesta pendente (PID=%d, size=%zu)\n", to_dispatch->pid, to_dispatch->filesize);
            struct upload_state* up = get_upload_state(to_dispatch->pid, to_dispatch->total_chunks);
            if (!up) continue;
            FILE *tmpf = (to_dispatch->chunk_id == 0) ? fopen(up->tmp_path, "wb") : fopen(up->tmp_path, "ab");
            if (!tmpf) continue;
            // MUTUA ESCLUSIONE SULLA MEMORIA CONDIVISA
            sem_wait(semid, SEM_MEM);
            int client_shmid = create_shared_memory(to_dispatch->shm_key, 65536);
            void *shmaddr = attach_shared_memory(client_shmid);
            if (!shmaddr) { fclose(tmpf); sem_signal(semid, SEM_MEM); continue; }
            fwrite(shmaddr, 1, to_dispatch->filesize, tmpf);
            fflush(tmpf);
            fclose(tmpf);
            detach_shared_memory(shmaddr);
            sem_signal(semid, SEM_MEM);
            up->received_chunks++;
            // Ack per ogni chunk, anche l'ultimo
            struct message ack;
            ack.mtype = to_dispatch->pid;
            ack.pid = to_dispatch->pid;
            memset(ack.hash, 0, sizeof(ack.hash));
            send_message(msgid, &ack);
            if (!to_dispatch->last_chunk) {
                sem_signal(semid, SEM_PROC);
                continue;
            }
            pid_t pid = fork();
            if (pid == 0) {
                char hash[65] = {0};
                compute_sha256_from_file(up->tmp_path, hash);
                struct message resp;
                resp.mtype = to_dispatch->pid;
                resp.pid = to_dispatch->pid;
                resp.filesize = to_dispatch->filesize;
                strncpy(resp.hash, hash, 65);
                send_message(msgid, &resp);
                printf("\n[SERVER] Hash fornito al client PID=%d\n", to_dispatch->pid);
                remove(up->tmp_path);
                sem_signal(semid, SEM_PROC);
                exit(0);
            }
            clear_upload_state(to_dispatch->pid);
            while (waitpid(-1, NULL, WNOHANG) > 0);
            continue;
        }
        // Legge messaggi (msgrcv)
        if (receive_message(msgid, 1, &req) == -1) continue;
        // Stampa solo se cambia PID o chunk, ma stampa SOLO l'inizio e la fine upload
        if (req.chunk_id == 0) {
            printf("[SERVER] Inizio upload da client PID=%d, size=%zu, chunk %u/%u\n",
                   req.pid, req.filesize * req.total_chunks, req.chunk_id+1, req.total_chunks);
        }
        if (req.last_chunk) {
            printf("[SERVER] Fine upload da client PID=%d, size=%zu, chunk %u/%u\n",
                   req.pid, req.filesize * req.total_chunks, req.chunk_id+1, req.total_chunks);
        }
        // Gestione messaggio di controllo
        if (req.mtype == 99) { // CONTROL_TYPE
            max_workers = (int)req.filesize;
            semctl(semid, SEM_PROC, SETVAL, max_workers);
            printf("[SERVER] Aggiornato max_workers a %d\n", max_workers);
            continue;
        }

        // Se non ci sono worker disponibili, accoda la richiesta
        if (semctl(semid, SEM_PROC, GETVAL) == 0) {
            enqueue_pending(&req);
            continue;
        }

        struct upload_state* up = get_upload_state(req.pid, req.total_chunks);
        if (!up) {
            printf("[SERVER] ERRORE: troppi upload simultanei!\n");
            continue;
        }

        FILE *tmpf = NULL;
        if (req.chunk_id == 0)
            tmpf = fopen(up->tmp_path, "wb");
        else
            tmpf = fopen(up->tmp_path, "ab");
        if (!tmpf) {
            perror("[SERVER] Errore apertura file temporaneo");
            continue;
        }

        sem_wait(semid, SEM_MEM);
        int client_shmid = create_shared_memory(req.shm_key, 65536);
        void *shmaddr = attach_shared_memory(client_shmid);
        if (!shmaddr) {
            fclose(tmpf);
            continue;
        }
        fwrite(shmaddr, 1, req.filesize, tmpf);
        fflush(tmpf);
        fclose(tmpf);
        detach_shared_memory(shmaddr);
        sem_signal(semid, SEM_MEM);

        up->received_chunks++;
        // Ack per ogni chunk, anche l'ultimo
        struct message ack;
        ack.mtype = req.pid;
        ack.pid = req.pid;
        memset(ack.hash, 0, sizeof(ack.hash));
        send_message(msgid, &ack);
        if (!req.last_chunk) {
            continue;
        }

        // Ultimo chunk: dispatch worker per hash
        sem_wait(semid, SEM_PROC);
        pid_t pid = fork();
        if (pid == 0) {
            char hash[65] = {0};
            compute_sha256_from_file(up->tmp_path, hash);
            struct message resp;
            resp.mtype = req.pid;
            resp.pid = req.pid;
            resp.filesize = req.filesize;
            strncpy(resp.hash, hash, 65);
            send_message(msgid, &resp);
            printf("\n[SERVER] Hash fornito al client PID=%d\n", req.pid);
            remove(up->tmp_path);
            sem_signal(semid, SEM_PROC);
            exit(0);
        }
        clear_upload_state(req.pid);
        // Rimuovi figli zombie
        while (waitpid(-1, NULL, WNOHANG) > 0);
    }
    // 6. Cleanup finale (IPC_RMID)
    handle_sigint(0);
    return 0;
}
