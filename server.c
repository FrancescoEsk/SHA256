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

// ===================== VARIABILI GLOBALI =====================
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

// ===================== FUNZIONI DI UTILITÀ =====================

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

// Funzione di utilità: scrive un buffer su file, flush e chiude
void scrivi_e_chiudi(FILE *f, const void *buf, size_t size) {
    fwrite(buf, 1, size, f);
    fflush(f);
    fclose(f);
}

// Funzione di utilità: scrive un chunk su file temporaneo usando la shm
int scrivi_chunk_su_file(const struct message* req, const struct upload_state* up, int semid) {
    FILE *tmpf = (req->chunk_id == 0) ? fopen(up->tmp_path, "wb") : fopen(up->tmp_path, "ab");

    if (!tmpf) {
        perror("[SERVER] Errore apertura file temporaneo");
        return 0;
    }

    sem_wait(semid, SEM_MEM);
    int client_shmid = create_shared_memory(req->shm_key, 65536);
    void *shmaddr = attach_shared_memory(client_shmid);

    if (!shmaddr) {
        fclose(tmpf);
        sem_signal(semid, SEM_MEM);
        return 0;
    }

    scrivi_e_chiudi(tmpf, shmaddr, req->filesize);
    detach_shared_memory(shmaddr);
    sem_signal(semid, SEM_MEM);
    return 1;
}

// Funzione di utilità: invia ack al client
void invia_ack(const struct message* req, int msgid) {
    struct message ack;
    ack.mtype = req->pid;
    ack.pid = req->pid;
    memset(ack.hash, 0, sizeof(ack.hash));
    send_message(msgid, &ack);
}

// Funzione di utilità: prepara una risposta SHA256 per il client
struct message crea_risposta_hash(pid_t pid, size_t filesize, const char* hash) {
    struct message resp = {
        pid,           // mtype
        pid,           // pid
        filesize,      // filesize
        {0},           // hash (verrà riempito dopo)
        0,             // chunk_id (non usato in risposta)
        0,             // total_chunks (non usato in risposta)
        0,             // last_chunk (non usato in risposta)
        0              // shm_key (non usato in risposta)
    };

    strncpy(resp.hash, hash, 65);
    return resp;
}

// ===================== MAIN SERVER =====================

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

    // ===================== LOOP PRINCIPALE =====================
    while (1) {
        struct message req;
        // ===================== SCHEDULAZIONE E SELEZIONE RICHIESTA =====================
        // Se ci sono richieste pendenti, processa SEMPRE la più grande tra quelle pendenti e la richiesta corrente
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
                    // ===================== CONTROLLO: MODIFICA NUMERO WORKER =====================
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

            // ===================== DISPATCH DELLA RICHIESTA SELEZIONATA =====================
            sem_wait(semid, SEM_PROC);
            printf("[SERVER] Dispatch richiesta pendente (PID=%d, size=%zu)\n", to_dispatch->pid, to_dispatch->filesize);
            struct upload_state* up = get_upload_state(to_dispatch->pid, to_dispatch->total_chunks);
            if (!up) continue;

            // Crea file temporaneo per il chunk
            FILE *tmpf = (to_dispatch->chunk_id == 0) ? fopen(up->tmp_path, "wb") : fopen(up->tmp_path, "ab");
            if (!tmpf) continue;

            // ===================== SCRITTURA SU FILE TEMPORANEO (MUTUA ESCLUSIONE SHM) =====================
            sem_wait(semid, SEM_MEM);
            int client_shmid = create_shared_memory(to_dispatch->shm_key, 65536);
            void *shmaddr = attach_shared_memory(client_shmid);

            if (!shmaddr) {
                fclose(tmpf);
                sem_signal(semid, SEM_MEM);
                continue;
            }

            scrivi_e_chiudi(tmpf, shmaddr, to_dispatch->filesize);
            detach_shared_memory(shmaddr);
            sem_signal(semid, SEM_MEM);
            up->received_chunks++;

            // ===================== INVIO ACK AL CLIENT =====================
            struct message ack;
            ack.mtype = to_dispatch->pid;
            ack.pid = to_dispatch->pid;
            memset(ack.hash, 0, sizeof(ack.hash));
            send_message(msgid, &ack);

            if (!to_dispatch->last_chunk) {
                sem_signal(semid, SEM_PROC);
                continue;
            }

            // ===================== ULTIMO CHUNK: CALCOLO HASH E RISPOSTA =====================
            pid_t pid = fork();

            if (pid == 0) {
                // Processo figlio: calcola hash SHA256 e invia risposta al client
                char hash[65] = {0};
                compute_sha256_from_file(up->tmp_path, hash);
                struct message resp = crea_risposta_hash(to_dispatch->pid, to_dispatch->filesize, hash);
                send_message(msgid, &resp);
                printf("\n[SERVER] Hash fornito al client PID=%d\n", to_dispatch->pid);

                remove(up->tmp_path); // Elimina file temporaneo
                sem_signal(semid, SEM_PROC); // Libera un worker
                exit(0);
            }

            clear_upload_state(to_dispatch->pid);
            while (waitpid(-1, NULL, WNOHANG) > 0)
            continue;
        }

        // ===================== GESTIONE RICHIESTE SENZA PENDENZE =====================
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

        if (!scrivi_chunk_su_file(&req, up, semid)) {
            continue;
        }

        up->received_chunks++;
        invia_ack(&req, msgid);
        if (!req.last_chunk) {
            continue;
        }

        // Ultimo chunk: dispatch worker per hash
        sem_wait(semid, SEM_PROC);
        pid_t pid = fork();

        if (pid == 0) {
            // Processo figlio: calcola hash SHA256 del file temporaneo
            char hash[65] = {0};
            compute_sha256_from_file(up->tmp_path, hash);
            struct message resp = crea_risposta_hash(req.pid, req.filesize, hash);
            send_message(msgid, &resp);
            printf("\n[SERVER] Hash fornito al client PID=%d\n", req.pid);

            remove(up->tmp_path); // Elimina file temporaneo dopo l'invio della risposta
            sem_signal(semid, SEM_PROC); // Libera un worker
            exit(0);
        }

        clear_upload_state(req.pid);

        // Rimuovi figli zombie
        while (waitpid(-1, NULL, WNOHANG) > 0);
    }
    // 6. Cleanup finale non necessario.
    // Il ciclo while è infinito e gestisce SIGINT per rimuovere risorse IPC.
}
