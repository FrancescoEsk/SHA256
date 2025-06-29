#include "shm_utils.h"
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>

// ---- CREAZIONE SHARED MEMORY ----
int create_shared_memory(key_t key, size_t size) {
    int shmid = shmget(key, size, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed");
    }
    return shmid;
}

// ---- ATTACCO SHARED MEMORY ----
void* attach_shared_memory(int shmid) {
    void* addr = shmat(shmid, NULL, 0);
    if (addr == (void*)-1) {
        perror("shmat failed");
        return NULL;
    }
    return addr;
}

// ---- DETACH SHARED MEMORY ----
void detach_shared_memory(void* addr) {
    if (shmdt(addr) == -1) {
        perror("shmdt failed");
    }
}

// ---- RIMOZIONE SHARED MEMORY ----
void remove_shared_memory(int shmid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl(IPC_RMID) failed");
    }
}
