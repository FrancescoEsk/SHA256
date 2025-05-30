// control_client.c – Modifica dinamicamente il numero massimo di worker del server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include "ipc/msg_utils.h"

#define MSG_KEY 0x5678
#define CONTROL_TYPE 99

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <max_workers>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int new_limit = atoi(argv[1]);
    if (new_limit <= 0) {
        fprintf(stderr, "max_workers must be a positive integer\n");
        return EXIT_FAILURE;
    }

    // Ottieni coda messaggi
    int msgid = create_message_queue(MSG_KEY);
    if (msgid == -1) {
        perror("msgget");
        return EXIT_FAILURE;
    }

    // Prepara messaggio di controllo
    struct message msg;
    msg.mtype = CONTROL_TYPE;
    msg.pid = getpid();
    msg.filesize = (size_t)new_limit;
    memset(msg.hash, 0, sizeof(msg.hash)); // non usato

    // Invia messaggio
    if (send_message(msgid, &msg) == -1) {
        perror("msgsnd");
        return EXIT_FAILURE;
    }

    printf("Inviato nuovo limite al server: %d worker\n", new_limit);
    return EXIT_SUCCESS;
}