#include "msg_utils.h"
#include <sys/msg.h>
#include <stdio.h>

// ---- CREAZIONE CODA ----
int create_message_queue(key_t key) {
    int msgid = msgget(key, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("msgget failed");
    }
    return msgid;
}

// ---- INVIO MESSAGGIO ----
int send_message(int msgid, struct message* msg) {
    if (msgsnd(msgid, msg, sizeof(struct message) - sizeof(long), 0) == -1) {
        perror("msgsnd failed");
        return -1;
    }
    return 0;
}

// ---- RICEZIONE MESSAGGIO ----
int receive_message(int msgid, long mtype, struct message* msg) {
    ssize_t ret = msgrcv(msgid, msg, sizeof(struct message) - sizeof(long), mtype, 0);
    if (ret == -1) {
        perror("msgrcv failed");
        return -1;
    }
    return 0;
}

// ---- RIMOZIONE CODA ----
void remove_message_queue(int msgid) {
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl(IPC_RMID) failed");
    }
}

