/*
 * secret.c - Secret/credential retrieval with Doppler mount support
 *
 * Provides unified access to secrets from either:
 * - JSON secrets file mounted via Doppler
 * - Environment variables with configurable prefix
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <jansson.h>

#include "merc.h"
#include "secret.h"

#define SECRET_BUFFER_SIZE 16384

// Static state for secret data (cached after first load)
static bool secrets_loaded = false;
static json_t *secrets_json = NULL;

/*
 * Load secrets from the configured mount
 *
 * Called once on first access if secrets_mount is set.
 *
 * NOTE: This reads the entire file into a buffer before parsing because
 * Doppler's --mount option uses FIFOs (named pipes), and json_loadf()
 * doesn't work reliably with FIFOs (they're not seekable).
 */
static bool load_secrets_from_mount(void)
{
    FILE *fp;
    json_error_t error;
    char *buffer = NULL;
    size_t buffer_size = 0;
    size_t total_read = 0;
    size_t chunk_size = 4096;
    size_t bytes_read;

    if (!game_settings.secrets_mount || game_settings.secrets_mount[0] == '\0') {
        log_string("Secret: No secrets_mount configured");
        return false;
    }

    log_stringf("Secret: Attempting to load secrets from mount: %s", game_settings.secrets_mount);

    if (access(game_settings.secrets_mount, R_OK) != 0) {
        log_stringf("Secret: Mount file not accessible: %s (errno=%d)", game_settings.secrets_mount, errno);
        return false;
    }

    // Check file type before opening. Doppler uses FIFOs (named pipes) for
    // --mount, and fopen() on a FIFO blocks forever if Doppler isn't running.
    struct stat st;
    if (stat(game_settings.secrets_mount, &st) != 0) {
        log_stringf("Secret: Cannot stat mount file: %s (errno=%d)", game_settings.secrets_mount, errno);
        return false;
    }

    if (S_ISFIFO(st.st_mode)) {
        // Open FIFO non-blocking to avoid hanging if no writer (stale Doppler mount)
        int fd = open(game_settings.secrets_mount, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            log_stringf("Secret: Failed to open FIFO: %s (errno=%d)", game_settings.secrets_mount, errno);
            return false;
        }
        fp = fdopen(fd, "r");
        if (!fp) {
            log_stringf("Secret: Failed to fdopen FIFO: %s (errno=%d)", game_settings.secrets_mount, errno);
            close(fd);
            return false;
        }
    } else {
        fp = fopen(game_settings.secrets_mount, "r");
        if (!fp) {
            log_stringf("Secret: Failed to open mount file: %s (errno=%d)", game_settings.secrets_mount, errno);
            return false;
        }
    }

    // Read entire file into buffer - necessary for FIFOs which aren't seekable
    // and don't work with json_loadf()
    buffer_size = chunk_size;
    buffer = malloc(buffer_size);
    if (!buffer) {
        log_string("Secret: Failed to allocate buffer for secrets");
        fclose(fp);
        return false;
    }

    while ((bytes_read = fread(buffer + total_read, 1, chunk_size, fp)) > 0) {
        total_read += bytes_read;

        // Expand buffer if needed
        if (total_read + chunk_size > buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                log_string("Secret: Failed to expand buffer for secrets");
                free(buffer);
                fclose(fp);
                return false;
            }
            buffer = new_buffer;
        }
    }
    fclose(fp);

    // Null-terminate the buffer
    if (total_read >= buffer_size) {
        char *new_buffer = realloc(buffer, buffer_size + 1);
        if (!new_buffer) {
            free(buffer);
            return false;
        }
        buffer = new_buffer;
    }
    buffer[total_read] = '\0';

    log_stringf("Secret: Read %zu bytes from mount", total_read);

    if (total_read == 0) {
        log_string("Secret: Mount file was empty");
        free(buffer);
        return false;
    }

    // Parse JSON from buffer (works with FIFOs unlike json_loadf)
    secrets_json = json_loads(buffer, 0, &error);
    free(buffer);

    if (!secrets_json) {
        log_stringf("Warning: Failed to parse secrets JSON from %s: %s (line %d, col %d)",
                   game_settings.secrets_mount, error.text, error.line, error.column);
        return false;
    }

    // Log the number of keys loaded for diagnostics
    if (json_is_object(secrets_json)) {
        size_t key_count = json_object_size(secrets_json);
        log_stringf("Secret: Loaded %zu keys from mount: %s", key_count, game_settings.secrets_mount);
    } else {
        log_stringf("Warning: Secrets mount did not contain a JSON object");
        json_decref(secrets_json);
        secrets_json = NULL;
        return false;
    }

    return true;
}

/*
 * Get a secret from JSON if loaded, otherwise NULL
 */
static const char *get_from_json(const char *secret_name)
{
    json_t *value;

    if (!secrets_json) {
        return NULL;
    }

    value = json_object_get(secrets_json, secret_name);
    if (!value) {
        return NULL;
    }

    if (!json_is_string(value)) {
        return NULL;
    }

    return json_string_value(value);
}

/*
 * Read a secret from the configured source
 */
const char *secret_get(const char *secret_name)
{
    const char *value;

    if (!secret_name || secret_name[0] == '\0') {
        return NULL;
    }

    // Try JSON mount first (if not yet loaded, attempt to load)
    if (!secrets_loaded && game_settings.secrets_mount && 
        game_settings.secrets_mount[0] != '\0') {
        load_secrets_from_mount();
        secrets_loaded = true;
    }

    if (secrets_json) {
        value = get_from_json(secret_name);
        if (value) {
            log_stringf("Override applied: %s from secrets mount", secret_name);
            return value;
        }
    }

    // Fall back to environment variable
    value = getenv(secret_name);
    if (value) {
        log_stringf("Override applied: %s from environment variable", secret_name);
    }
    return value;
}

/*
 * Read a secret and return as newly allocated string
 */
char *secret_get_alloc(const char *secret_name)
{
    const char *value = secret_get(secret_name);

    if (value && value[0] != '\0') {
        return str_dup(value);
    }

    return NULL;
}

/*
 * Check if secrets are being loaded from a mount
 */
bool secret_using_mount(void)
{
    // Ensure we've tried to load
    if (!secrets_loaded && game_settings.secrets_mount && 
        game_settings.secrets_mount[0] != '\0') {
        log_stringf("Secret: Checking for mount on first call: %s", game_settings.secrets_mount);
        load_secrets_from_mount();
        secrets_loaded = true;
    }

    bool result = (secrets_json != NULL);
    log_stringf("Secret: secret_using_mount() = %s", result ? "true" : "false");
    return result;
}

/*
 * Safely clear sensitive data from memory
 *
 * Uses volatile to prevent compiler from optimizing away the memset
 */
void secret_clear_buffer(char *buf, size_t size)
{
    volatile unsigned char *vbuf = (volatile unsigned char *)buf;
    size_t i;

    if (!buf) {
        return;
    }

    for (i = 0; i < size; i++) {
        vbuf[i] = 0;
    }
}

/*
 * Iterate through all secrets in the mount, calling the callback for each
 *
 * Returns the number of secrets processed, or -1 if no mount is loaded
 */
int secret_iterate(secret_iterate_callback callback, void *user_data)
{
    const char *key;
    json_t *value;
    int count = 0;

    // Ensure secrets are loaded
    if (!secrets_loaded && game_settings.secrets_mount &&
        game_settings.secrets_mount[0] != '\0') {
        load_secrets_from_mount();
        secrets_loaded = true;
    }

    if (!secrets_json) {
        return -1;  // No mount loaded
    }

    json_object_foreach(secrets_json, key, value) {
        if (json_is_string(value)) {
            const char *str_value = json_string_value(value);
            if (callback(key, str_value, user_data)) {
                count++;
            }
        }
    }

    return count;
}

/*
 * Cleanup function (can be called on shutdown)
 */
void secret_cleanup(void)
{
    if (secrets_json) {
        json_decref(secrets_json);
        secrets_json = NULL;
    }
    secrets_loaded = false;
}
