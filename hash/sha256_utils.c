#include "sha256_utils.h"
#include <openssl/sha.h>
#include <stdio.h>
#include <sys/stat.h>

// ---- SHA256 SU BUFFER ----
void compute_sha256(const unsigned char* data, size_t len, char* output_hash) {
    // ---- VARIABILI LOCALI ----
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;

    // ---- INIZIALIZZAZIONE SHA256 ----
    if (SHA256_Init(&sha256) != 1) {
        printf("SHA256_Init failed\n");
        return;
    }

    // ---- AGGIORNAMENTO SHA256 CON I DATI ----
    if (SHA256_Update(&sha256, data, len) != 1) {
        printf("SHA256_Update failed\n");
        return;
    }

    // ---- FINALIZZAZIONE SHA256 ----
    if (SHA256_Final(hash, &sha256) != 1) {
        printf("SHA256_Final failed\n");
        return;
    }

    // ---- CONVERSIONE HASH IN STRINGA ESADECIMALE ----
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(output_hash + (i * 2), "%02x", hash[i]);
    }
    output_hash[64] = '\0';
}

// ---- SHA256 SU FILE ----
void compute_sha256_from_file(const char* path, char* output_hash) {
    // ---- APERTURA FILE ----
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        printf("Errore apertura file per SHA256: %s\n", path);
        output_hash[0] = '\0';
        return;
    }

    // ---- VARIABILI LOCALI ----
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;

    // ---- INIZIALIZZAZIONE SHA256 ----
    if (SHA256_Init(&sha256) != 1) {
        printf("SHA256_Init failed\n");
        fclose(fp);
        output_hash[0] = '\0';
        return;
    }

    // ---- LETTURA E AGGIORNAMENTO SHA256 ----
    unsigned char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (SHA256_Update(&sha256, buf, n) != 1) {
            printf("SHA256_Update failed\n");
            fclose(fp);
            output_hash[0] = '\0';
            return;
        }
    }

    // ---- FINALIZZAZIONE SHA256 ----
    if (SHA256_Final(hash, &sha256) != 1) {
        printf("SHA256_Final failed\n");
        fclose(fp);
        output_hash[0] = '\0';
        return;
    }
    fclose(fp);

    // ---- CONVERSIONE HASH IN STRINGA ESADECIMALE ----
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(output_hash + (i * 2), "%02x", hash[i]);
    }
    output_hash[64] = '\0';
}
