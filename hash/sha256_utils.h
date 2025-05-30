#ifndef SHA256_UTILS_H
#define SHA256_UTILS_H

#include <stddef.h>

// Calcola hash SHA-256 su un blocco di dati
// `output_hash` deve avere almeno 65 byte (64 + 1 per il null terminator)
void compute_sha256(const unsigned char* data, size_t len, char* output_hash);

// Calcola hash SHA-256 su un file di qualsiasi dimensione
// `output_hash` deve avere almeno 65 byte (64 + 1 per il null terminator)
void compute_sha256_from_file(const char* path, char* output_hash);

#endif

