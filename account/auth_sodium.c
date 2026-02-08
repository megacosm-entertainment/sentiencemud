/**
 * auth_sodium.c - Modern cryptographic primitives using libsodium
 *
 * Implements Argon2id password hashing and XSalsa20-Poly1305 authenticated
 * encryption to replace legacy crypt() and AES-CBC implementations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>
#include "../merc.h"
#include "auth_sodium.h"

/* Track initialization state */
static bool sodium_initialized = false;

/**
 * init_sodium - Initialize libsodium library
 *
 * Must be called once at startup before using any libsodium functions.
 * Safe to call multiple times (subsequent calls are no-ops).
 *
 * @return true on success, false on failure
 */
bool init_sodium(void)
{
    if (sodium_initialized)
        return true;

    if (sodium_init() < 0) {
        fprintf(stderr, "FATAL: Failed to initialize libsodium\n");
        return false;
    }

    sodium_initialized = true;
    log_message(LOG_LEVEL_INFO, LOG_INIT, "Libsodium initialized successfully");
    return true;
}

/**
 * hash_password_v3 - Hash password using Argon2id
 *
 * Uses memory-hard Argon2id algorithm to hash passwords, providing
 * resistance against GPU and ASIC-based brute force attacks.
 *
 * The hash format is: $argon2id$v=19$m=65536,t=2,p=1$salt$hash
 * This is compatible with standard Argon2id implementations.
 *
 * @param plaintext  Plain text password to hash
 * @param level      Security level (interactive, moderate, sensitive)
 * @return           Allocated hash string (must be freed), or NULL on error
 */
char *hash_password_v3(const char *plaintext, password_security_level_t level)
{
    char *hashed;
    unsigned long long opslimit;
    size_t memlimit;

    if (!sodium_initialized) {
        if (!init_sodium())
            return NULL;
    }

    if (!plaintext || !*plaintext) {
        fprintf(stderr, "hash_password_v3: NULL or empty plaintext\n");
        return NULL;
    }

    /* Select parameters based on security level */
    switch (level) {
        case PWD_SECURITY_INTERACTIVE:
            opslimit = crypto_pwhash_OPSLIMIT_INTERACTIVE;
            memlimit = crypto_pwhash_MEMLIMIT_INTERACTIVE;
            break;
        case PWD_SECURITY_MODERATE:
            opslimit = crypto_pwhash_OPSLIMIT_MODERATE;
            memlimit = crypto_pwhash_MEMLIMIT_MODERATE;
            break;
        case PWD_SECURITY_SENSITIVE:
            opslimit = crypto_pwhash_OPSLIMIT_SENSITIVE;
            memlimit = crypto_pwhash_MEMLIMIT_SENSITIVE;
            break;
        default:
            opslimit = crypto_pwhash_OPSLIMIT_INTERACTIVE;
            memlimit = crypto_pwhash_MEMLIMIT_INTERACTIVE;
            break;
    }

    /* Allocate buffer for hash string */
    hashed = malloc(crypto_pwhash_STRBYTES);
    if (!hashed) {
        fprintf(stderr, "hash_password_v3: Out of memory\n");
        return NULL;
    }

    /* Hash the password - libsodium handles salt generation internally */
    if (crypto_pwhash_str(hashed, plaintext, strlen(plaintext),
                         opslimit, memlimit) != 0) {
        fprintf(stderr, "hash_password_v3: Out of memory or computation failed\n");
        free(hashed);
        return NULL;
    }

    /* Convert to game's string type */
    char *result = str_dup(hashed);
    sodium_memzero(hashed, crypto_pwhash_STRBYTES);
    free(hashed);

    return result;
}

/**
 * verify_password_v3 - Verify password against Argon2id hash
 *
 * Performs constant-time verification of a password against an Argon2id
 * hash. Timing is independent of whether the password matches or not.
 *
 * @param hash       Argon2id hash string (starts with $argon2id$)
 * @param plaintext  Plain text password to verify
 * @return           true if password matches, false otherwise
 */
bool verify_password_v3(const char *hash, const char *plaintext)
{
    if (!sodium_initialized) {
        if (!init_sodium())
            return false;
    }

    if (!hash || !*hash || !plaintext || !*plaintext) {
        fprintf(stderr, "verify_password_v3: NULL or empty input\n");
        return false;
    }

    /* Verify using libsodium - constant time verification */
    return crypto_pwhash_str_verify(hash, plaintext, strlen(plaintext)) == 0;
}

/**
 * encrypt_string_v2 - Encrypt string using authenticated encryption
 *
 * Uses XSalsa20-Poly1305 (crypto_secretbox) for authenticated encryption.
 * The result includes a MAC that detects tampering. Format is:
 *   nonce (24 bytes) || MAC+ciphertext (crypto_secretbox output)
 * Then base64 encoded for storage.
 *
 * @param plaintext  Plain text string to encrypt
 * @param key        32-byte encryption key
 * @return           Base64-encoded encrypted string, or NULL on error
 */
char *encrypt_string_v2(const char *plaintext, const unsigned char *key)
{
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    unsigned char *ciphertext;
    size_t plaintext_len;
    size_t ciphertext_len;
    size_t output_len;
    unsigned char *output;
    char *base64;
    char *result;

    if (!sodium_initialized) {
        if (!init_sodium())
            return NULL;
    }

    if (!plaintext || !*plaintext || !key) {
        fprintf(stderr, "encrypt_string_v2: NULL input\n");
        return NULL;
    }

    plaintext_len = strlen(plaintext);
    ciphertext_len = crypto_secretbox_MACBYTES + plaintext_len;

    /* Allocate buffers */
    ciphertext = malloc(ciphertext_len);
    if (!ciphertext)
        return NULL;

    /* Generate random nonce */
    randombytes_buf(nonce, sizeof(nonce));

    /* Encrypt with authentication */
    if (crypto_secretbox_easy(ciphertext, (const unsigned char *)plaintext,
                             plaintext_len, nonce, key) != 0) {
        fprintf(stderr, "encrypt_string_v2: Encryption failed\n");
        free(ciphertext);
        return NULL;
    }

    /* Combine nonce + ciphertext for storage */
    output_len = sizeof(nonce) + ciphertext_len;
    output = malloc(output_len);
    if (!output) {
        free(ciphertext);
        return NULL;
    }

    memcpy(output, nonce, sizeof(nonce));
    memcpy(output + sizeof(nonce), ciphertext, ciphertext_len);

    /* Base64 encode for storage */
    base64 = (char *)base64_encode(output, output_len, &output_len);

    /* Clean up */
    sodium_memzero(ciphertext, ciphertext_len);
    free(ciphertext);
    sodium_memzero(output, sizeof(nonce) + ciphertext_len);
    free(output);

    if (!base64)
        return NULL;

    /* Convert to game's string type */
    result = str_dup(base64);
    free(base64);

    return result;
}

/**
 * decrypt_string_v2 - Decrypt authenticated encrypted string
 *
 * Decrypts a string encrypted with encrypt_string_v2. Verifies the MAC
 * and rejects tampered or corrupted ciphertext.
 *
 * @param ciphertext  Base64-encoded encrypted string
 * @param key         32-byte encryption key (same as used for encryption)
 * @return            Decrypted plain text string, or NULL on error/tamper
 */
char *decrypt_string_v2(const char *ciphertext, const unsigned char *key)
{
    unsigned char *binary;
    size_t binary_len;
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    unsigned char *encrypted_data;
    size_t encrypted_len;
    unsigned char *plaintext;
    size_t plaintext_len;
    char *result;

    if (!sodium_initialized) {
        if (!init_sodium())
            return NULL;
    }

    if (!ciphertext || !*ciphertext || !key) {
        fprintf(stderr, "decrypt_string_v2: NULL input\n");
        return NULL;
    }

    /* Base64 decode */
    binary = base64_decode(ciphertext, strlen(ciphertext), &binary_len);
    if (!binary || binary_len <= crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES) {
        fprintf(stderr, "decrypt_string_v2: Invalid ciphertext format\n");
        if (binary) free(binary);
        return NULL;
    }

    /* Extract nonce */
    memcpy(nonce, binary, sizeof(nonce));

    /* Extract encrypted data */
    encrypted_len = binary_len - sizeof(nonce);
    encrypted_data = binary + sizeof(nonce);

    /* Allocate plaintext buffer */
    plaintext_len = encrypted_len - crypto_secretbox_MACBYTES;
    plaintext = malloc(plaintext_len + 1); /* +1 for null terminator */
    if (!plaintext) {
        free(binary);
        return NULL;
    }

    /* Decrypt and verify MAC */
    if (crypto_secretbox_open_easy(plaintext, encrypted_data, encrypted_len,
                                   nonce, key) != 0) {
        fprintf(stderr, "decrypt_string_v2: Decryption failed (tampered or wrong key)\n");
        free(plaintext);
        free(binary);
        return NULL;
    }

    /* Null terminate */
    plaintext[plaintext_len] = '\0';

    /* Convert to game's string type */
    result = str_dup((char *)plaintext);

    /* Clean up */
    sodium_memzero(plaintext, plaintext_len);
    free(plaintext);
    free(binary);

    return result;
}

/**
 * is_argon2id_hash - Check if string is an Argon2id hash
 *
 * @param hash  String to check
 * @return      true if hash starts with $argon2id$
 */
bool is_argon2id_hash(const char *hash)
{
    if (!hash || !*hash)
        return false;

    return strncmp(hash, "$argon2id$", 10) == 0;
}

/**
 * detect_password_version - Detect password hash version
 *
 * Analyzes hash format to determine which algorithm was used.
 *
 * @param hash  Hash string to analyze
 * @return      Password version constant (PWD_VER_*)
 */
int detect_password_version(const char *hash)
{
    if (!hash || !*hash)
        return PWD_VER_PLAINTEXT;

    /* Argon2id - starts with $argon2id$ */
    if (is_argon2id_hash(hash))
        return PWD_VER_ARGON2ID;

    /* SHA-256 or SHA-512 crypt - starts with $5$ or $6$ */
    if (strncmp(hash, "$5$", 3) == 0 || strncmp(hash, "$6$", 3) == 0)
        return PWD_VER_CRYPT_SYSTEM;

    /* Custom SHA-256 - typically 64 hex chars, no $ prefix */
    if (strlen(hash) == 64) {
        bool all_hex = true;
        for (const char *p = hash; *p; p++) {
            if (!isxdigit(*p)) {
                all_hex = false;
                break;
            }
        }
        if (all_hex)
            return PWD_VER_SHA256_CUSTOM;
    }

    /* DES crypt - 13 characters */
    if (strlen(hash) == 13)
        return PWD_VER_CRYPT_SYSTEM;

    /* Default to crypt system for other formats */
    return PWD_VER_CRYPT_SYSTEM;
}

/**
 * constant_time_compare - Constant-time string comparison
 *
 * Compares two strings in constant time to prevent timing attacks.
 * Uses libsodium's sodium_memcmp for security.
 *
 * @param a    First string
 * @param b    Second string
 * @param len  Length to compare
 * @return     true if strings match, false otherwise
 */
bool constant_time_compare(const char *a, const char *b, size_t len)
{
    if (!a || !b)
        return false;

    if (!sodium_initialized) {
        if (!init_sodium())
            return false;
    }

    return sodium_memcmp(a, b, len) == 0;
}
