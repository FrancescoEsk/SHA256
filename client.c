// client.c – Invia un file al server e riceve il digest SHA-256

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ipc/shm_utils.h"
#include "ipc/sem_utils.h"
#include "ipc/msg_utils.h"

#define SHM_KEY 0x1234      // chiave per memoria condivisa
#define MSG_KEY 0x5678      // chiave per coda messaggi
#define SEM_KEY 0x9ABC      // chiave per semafori
#define MAX_FILE_SIZE 65536 // max dimensione file (64 KB)
#define CLIENT_TYPE 1       // tipo messaggio client->server

// ===================== FUNZIONI DI UTILITÀ =====================

// Funzione di utilità: cleanup risorse e termina
void cleanup_and_exit(char *chunkbuf, void *shmaddr, FILE *fp, int code) {
    if (chunkbuf) free(chunkbuf);
    if (shmaddr) detach_shared_memory(shmaddr);
    if (fp) fclose(fp);
    exit(code);
}

// Funzione di utilità: invia un chunk al server e attende ack
int invia_chunk_al_server(int semid, void *shmaddr, char *chunkbuf, size_t chunk_size, int msgid, struct message *msg, FILE *fp) {
    sem_wait(semid, 0);
    memcpy(shmaddr, chunkbuf, chunk_size);
    sem_signal(semid, 0);

    if (send_message(msgid, msg) == -1) {
        cleanup_and_exit(chunkbuf, shmaddr, fp, EXIT_FAILURE);
    }

    struct message ack;
    if (receive_message(msgid, msg->pid, &ack) == -1) {
        cleanup_and_exit(chunkbuf, shmaddr, fp, EXIT_FAILURE);
    }
    return 1;
}

int main(int argc, char *argv[]) {

    // ===================== PARSING ARGOMENTI =====================
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // ===================== APERTURA FILE E CALCOLO DIMENSIONE =====================
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Errore apertura file");
        exit(EXIT_FAILURE);
    }

    fseek(fp, 0, SEEK_END);
    size_t filesize = ftell(fp);
    rewind(fp);
    printf("[CLIENT] File '%s' letto (%zu byte).\n", argv[1], filesize);

    // ===================== INIZIALIZZAZIONE MEMORIA CONDIVISA E SEMAFORI =====================
    key_t my_shm_key = SHM_KEY + getpid();
    int shmid = create_shared_memory(my_shm_key, MAX_FILE_SIZE);
    if (shmid == -1) {
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    void *shmaddr = attach_shared_memory(shmid);
    if (!shmaddr) {
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    int semid = create_semaphore_set(SEM_KEY, 2);
    if (semid == -1) {
        detach_shared_memory(shmaddr);
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    // ===================== CALCOLO CHUNK E APERTURA CODA MESSAGGI =====================
    size_t total_chunks = (filesize + MAX_FILE_SIZE - 1) / MAX_FILE_SIZE;

    int msgid = create_message_queue(MSG_KEY);
    if (msgid == -1) {
        detach_shared_memory(shmaddr);
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    char *chunkbuf = malloc(MAX_FILE_SIZE);
    if (!chunkbuf) {
        perror("malloc");
        detach_shared_memory(shmaddr);
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    // ===================== INVIO CHUNK AL SERVER =====================
    size_t last_printed = 0;
    for (size_t i = 0; i < total_chunks; ++i) {
        size_t chunk_size = (i == total_chunks - 1) ? (filesize - i * MAX_FILE_SIZE) : MAX_FILE_SIZE;
        size_t nread = fread(chunkbuf, 1, chunk_size, fp);

        if (nread != chunk_size) {
            fprintf(stderr, "Errore lettura chunk %zu dal file.\n", i);
            cleanup_and_exit(chunkbuf, shmaddr, fp, EXIT_FAILURE);
        }
        if (i != last_printed) {
            printf("[CLIENT] Invio chunk %zu/%zu\r", i+1, total_chunks);
            fflush(stdout);
            last_printed = i;
        }

        struct message msg = {
            1,                          // tipo messaggio (1 = client->server)
            getpid(),
            chunk_size,
            {0},                     // hash (non usato in invio, solo in risposta)
            i,
            total_chunks,
            (i == total_chunks - 1) ? 1 : 0, // last chunk: 1 se è l'ultimo chunk, altrimenti 0
            my_shm_key
        };

        invia_chunk_al_server(semid, shmaddr, chunkbuf, chunk_size, msgid, &msg, fp);
    }
    printf("\n");
    free(chunkbuf);
    fclose(fp);

    // ===================== ATTESA RISPOSTA SHA256 DAL SERVER =====================
    printf("[CLIENT] In attesa di risposta dal server...\n");
    struct message resp;

    if (receive_message(msgid, getpid(), &resp) == -1) {
        detach_shared_memory(shmaddr);
        exit(EXIT_FAILURE);
    }

    printf("[CLIENT] SHA-256 ricevuto: %s\n", resp.hash);

    detach_shared_memory(shmaddr);
    remove_shared_memory(shmid);
    printf("[CLIENT] Operazione completata.\n");
    return 0;
}
