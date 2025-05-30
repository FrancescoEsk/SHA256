// client.c – Invia un file al server e riceve il digest SHA-256

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#define SHM_KEY 0x1234      // chiave per memoria condivisa
#define MSG_KEY 0x5678      // chiave per coda messaggi
#define SEM_KEY 0x9ABC      // chiave per semafori
#define MAX_FILE_SIZE 65536 // max dimensione file (64 KB)
#define CLIENT_TYPE 1       // tipo messaggio client->server

// struttura per i messaggi da/verso il server
struct message {
    long mtype;             // tipo messaggio
    pid_t pid;              // PID del client
    size_t filesize;        // dimensione del file
    char hash[65];          // risposta dal server
};

// funzione utility per i semafori (P e V)
void sem_wait(int semid, int semnum);
void sem_signal(int semid, int semnum);

// main client
int main(int argc, char *argv[]) {
    // 1. Verifica argomenti: path file
    // 2. Leggi il file in memoria (fino a MAX_FILE_SIZE)
    // 3. Attacca memoria condivisa con shmget + shmat
    // 4. Lock semaforo prima di scrivere (sem_wait)
    // 5. Scrivi il file nella memoria condivisa
    // 6. Unlock semaforo (sem_signal)
    // 7. Costruisci messaggio con PID e dimensione file
    // 8. Invia messaggio con msgsnd
    // 9. Attendi risposta con msgrcv
    // 10. Stampa hash ricevuto
    // 11. Detach memoria con shmdt

    return 0;
}