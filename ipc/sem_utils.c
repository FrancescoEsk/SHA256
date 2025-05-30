#include "sem_utils.h"
#include <sys/sem.h>
#include <stdio.h>
#include <errno.h>

void sem_wait(int semid, int semnum) {
    struct sembuf op;
    op.sem_num = semnum;
    op.sem_op = -1;
    op.sem_flg = 0;
    if (semop(semid, &op, 1) == -1) {
        perror("semop wait failed");
    }
}

void sem_signal(int semid, int semnum) {
    struct sembuf op;
    op.sem_num = semnum;
    op.sem_op = 1;
    op.sem_flg = 0;
    if (semop(semid, &op, 1) == -1) {
        perror("semop signal failed");
    }
}

int create_semaphore_set(key_t key, int num_sems) {
    int semid = semget(key, num_sems, IPC_CREAT | IPC_EXCL | 0666);
    if (semid == -1) {
        if (errno == EEXIST) {
            // Già esiste, ottieni l'id
            semid = semget(key, num_sems, 0);
        } else {
            perror("semget failed");
            return -1;
        }
    }
    return semid;
}
