#ifndef SEM_UTILS_H
#define SEM_UTILS_H
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
// Inizializza un set di semafori
int create_semaphore_set(key_t key, int num_sems);

// Attende (P) su un semaforo
void sem_wait(int semid, int semnum);

// Segnala (V) su un semaforo
void sem_signal(int semid, int semnum);

#endif