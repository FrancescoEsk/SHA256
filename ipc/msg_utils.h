#ifndef MSG_UTILS_H
#define MSG_UTILS_H
#include <sys/types.h>
#include <sys/ipc.h>
#define HASH_SIZE 65

struct message {
    long mtype;
    pid_t pid;
    size_t filesize;
    char hash[HASH_SIZE];
    unsigned int chunk_id;      // id del chunk (0-based)
    unsigned int total_chunks;  // numero totale di chunk
    int last_chunk;             // 1 se ultimo chunk, 0 altrimenti
    key_t shm_key;              // CHIAVE MEMORIA CONDIVISA DEL CLIENT
};

int create_message_queue(key_t key);
int send_message(int msgid, struct message* msg);
int receive_message(int msgid, long mtype, struct message* msg);
void remove_message_queue(int msgid);
#endif

