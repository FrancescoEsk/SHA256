#include "sem_utils.h"
#include <sys/sem.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

void sem_wait(int semid, int semnum) {
    // TODO: Implementa operazione P
}

void sem_signal(int semid, int semnum) {
    // TODO: Implementa operazione V
}

int create_semaphore_set(key_t key, int num_sems) {
    // TODO: Crea e inizializza semafori
    return -1;
}
