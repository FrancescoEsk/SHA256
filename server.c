// server.c – Riceve richieste da client e gestisce il dispatch ai worker

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <sys/wait.h>

#define SHM_KEY 0x1234
#define MSG_KEY 0x5678
#define SEM_KEY 0x9ABC
#define MAX_WORKERS 5       // valore iniziale, modificabile da control client
#define SEM_MEM 0           // semaforo per accesso memoria
#define SEM_PROC 1          // semaforo per numero processi attivi

struct message {
    long mtype;
    pid_t pid;
    size_t filesize;
    char hash[65]; // solo per la risposta
};

// Variabili globali
int msgid, shmid, semid;
int max_workers = MAX_WORKERS;

// Signal handler per cleanup
void handle_sigint(int sig);

// Funzioni semaforo
void sem_wait(int semid, int semnum);
void sem_signal(int semid, int semnum);

// Funzione per elaborare una richiesta (fork)
void handle_request(struct message req);

// main server
int main() {
    // 1. Setup handler SIGINT per cleanup finale
    // 2. Inizializza coda messaggi (msgget)
    // 3. Inizializza memoria condivisa (shmget)
    // 4. Inizializza semafori (semget + semctl)
    // 5. Loop principale:
    //     a. Legge messaggi (msgrcv)
    //     b. Se controllo → aggiorna max_workers
    //     c. Altrimenti:
    //         - Se spazio disponibile (processi < max_workers):
    //             → fork per gestire richiesta
    //             → nel figlio: leggi file, calcola hash, rispondi
    //         - Altrimenti: metti in coda interna per la schedulazione
    // 6. Cleanup finale (IPC_RMID)

    return 0;
}