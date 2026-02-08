/**
 * auth_sodium.h - Modern cryptographic primitives using libsodium
 *
 * Provides Argon2id password hashing and authenticated encryption
 * to replace legacy crypt() and unauthenticated AES-CBC implementations.
 */

#ifndef AUTH_SODIUM_H
#define AUTH_SODIUM_H

#include <stdbool.h>
#include <stddef.h>

/* Password hashing security levels */
typedef enum {
    PWD_SECURITY_INTERACTIVE,  /* Normal login (64 MiB, ~100ms) */
    PWD_SECURITY_MODERATE,     /* Moderate security (256 MiB, ~400ms) */
    PWD_SECURITY_SENSITIVE     /* High security (1 GiB, ~2s) */
} password_security_level_t;

/* Password version for Argon2id */
#define PWD_VER_ARGON2ID 3

/* Initialize libsodium library - call once at startup */
bool init_sodium(void);

/* Password hashing with Argon2id */
char *hash_password_v3(const char *plaintext, password_security_level_t level);
bool verify_password_v3(const char *hash, const char *plaintext);

/* Authenticated encryption (XSalsa20-Poly1305) */
char *encrypt_string_v2(const char *plaintext, const unsigned char *key);
char *decrypt_string_v2(const char *ciphertext, const unsigned char *key);

/* Password hash detection */
bool is_argon2id_hash(const char *hash);
int detect_password_version(const char *hash);

/* Utility functions */
bool constant_time_compare(const char *a, const char *b, size_t len);

#endif /* AUTH_SODIUM_H */
