# SHA256_IPC

Progetto per calcolo SHA-256 tramite IPC (memoria condivisa, semafori, code di messaggi) in C.

## Requisiti
- CMake >= 3.15
- GCC o Clang
- OpenSSL (libreria di sviluppo, es: `libssl-dev` su Linux)

## Compilazione

1. Apri un terminale nella cartella del progetto.
2. Esegui:
   ```sh
   cmake -S . -B build
   cmake --build build
   ```
3. Gli eseguibili `client`, `server` e `control_client` saranno generati nella cartella `build/`.

## Esecuzione

- **Avvia il server**:
  ```sh
  ./build/server
  ```

- **Invia un file dal client**:
  ```sh
  ./build/client <percorso_file>
  ```

- **Modifica il numero massimo di worker (opzionale)**:
  ```sh
  ./build/control_client <max_workers>
  ```

## Note
- Il server deve essere avviato prima del client.
- Il client mostra l'hash SHA-256 calcolato dal server.
- Il progetto è compatibile sia con CLion che con compilazione manuale da terminale.
- 