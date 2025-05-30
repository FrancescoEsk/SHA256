#ifndef SHA256_UTILS_H
#define SHA256_UTILS_H

#include <stddef.h>

// Calcola hash SHA-256 su un blocco di dati
void compute_sha256(const unsigned char* data, size_t len, char* output_hash);

#endif