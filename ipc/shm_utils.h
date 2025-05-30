#ifndef SHM_UTILS_H
#define SHM_UTILS_H
#include <sys/types.h>
#include <sys/ipc.h>

int create_shared_memory(key_t key, size_t size);
void* attach_shared_memory(int shmid);
void detach_shared_memory(void* addr);
void remove_shared_memory(int shmid);

#endif