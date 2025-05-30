cmake_minimum_required(VERSION 3.15)
project(SHA256_IPC C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include directories per header personalizzati
include_directories(ipc hash)

# Eseguibile: client
add_executable(client
        client.c
        ipc/shm_utils.c
        ipc/sem_utils.c
        ipc/msg_utils.c
        hash/sha256_utils.c
)

# Eseguibile: server
add_executable(server
        server.c
        ipc/shm_utils.c
        ipc/sem_utils.c
        ipc/msg_utils.c
        hash/sha256_utils.c
)

# (opzionale) Eseguibile: control_client
add_executable(control_client
        control_client.c
        ipc/sem_utils.c
        ipc/msg_utils.c
)