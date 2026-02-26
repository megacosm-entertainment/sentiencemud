/*
 * secret.h - Secret/credential retrieval with Doppler mount support
 *
 * Provides unified access to secrets from either:
 * - JSON secrets file mounted via Doppler (e.g., /mnt/secrets)
 * - Environment variables (SENTIENCE_* or custom prefix)
 * - File paths (fallback for certificates, keys)
 *
 * The secrets mount (if configured) takes precedence over environment variables.
 */

#ifndef __SECRET_H__
#define __SECRET_H__

/*
 * Read a secret from the configured source
 *
 * Attempts to retrieve a secret using this precedence:
 * 1. Secrets JSON file at secrets_mount (if set and readable)
 * 2. Environment variable with the given name (usually SENTIENCE_*)
 * 3. NULL if neither found
 *
 * Args:
 *   secret_name: Full environment variable name to look for
 *                (e.g., "SENTIENCE_SSL_KEY_DATA")
 *
 * Returns:
 *   Pointer to the secret value, or NULL if not found
 *   The returned pointer is valid only until the next call to this function
 *   Do NOT free the returned pointer
 */
const char *secret_get(const char *secret_name);

/*
 * Read a secret and return as newly allocated string
 *
 * Args:
 *   secret_name: Full environment variable name to look for
 *
 * Returns:
 *   Newly allocated string containing the secret, or NULL if not found
 *   MUST be freed with free_string() when done
 */
char *secret_get_alloc(const char *secret_name);

/*
 * Check if secrets are being loaded from a mount
 *
 * Returns:
 *   true if secrets_mount is set and readable, false otherwise
 */
bool secret_using_mount(void);

/*
 * Safely clear sensitive data from memory
 *
 * Uses volatile writes to prevent compiler optimizations
 *
 * Args:
 *   buf: Buffer containing sensitive data
 *   size: Number of bytes to clear
 */
void secret_clear_buffer(char *buf, size_t size);

/*
 * Cleanup function to free secret data on shutdown
 *
 * Call during graceful shutdown to release memory
 */
void secret_cleanup(void);

/*
 * Callback function type for secret_iterate
 *
 * Args:
 *   key: The secret key name (e.g., "SENTIENCE_EMAIL_HOST")
 *   value: The secret value
 *   user_data: User-provided data pointer
 *
 * Returns:
 *   true if the secret was processed/applied, false if skipped
 */
typedef bool (*secret_iterate_callback)(const char *key, const char *value, void *user_data);

/*
 * Iterate through all secrets in the mount, calling the callback for each
 *
 * This is more efficient than checking each setting individually when
 * you have many settings but few overrides.
 *
 * Args:
 *   callback: Function to call for each secret
 *   user_data: User-provided data passed to callback
 *
 * Returns:
 *   Number of secrets where callback returned true, or -1 if no mount loaded
 */
int secret_iterate(secret_iterate_callback callback, void *user_data);

#endif
