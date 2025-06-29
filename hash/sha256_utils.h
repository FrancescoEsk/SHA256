#ifndef SHA256_UTILS_H
#define SHA256_UTILS_H

#include <stddef.h>

// Calcola l'hash SHA-256 di un blocco di dati in memoria (buffer)
// `output_hash` deve avere almeno 65 byte (64 caratteri esadecimali + 1 per il terminatore null)
void compute_sha256(const unsigned char* data, size_t len, char* output_hash);

// Calcola l'hash SHA-256 del contenuto di un file
// `output_hash` deve avere almeno 65 byte (64 caratteri esadecimali + 1 per il terminatore null)
void compute_sha256_from_file(const char* path, char* output_hash);

#endif
