/***************************************************************************
 *  Redis Cache Module - Implementation                                    *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <dirent.h>
#include <pthread.h>
#include <jansson.h>
#include "redis_cache.h"
#include "../../merc.h"

/***************************************************************************
 * Global State                                                            *
 ***************************************************************************/

static redisContext *redis_ctx = NULL;
static bool redis_available = false;
static bool redis_json_available = false;
static REDIS_STATS stats = {0};
static pthread_mutex_t redis_mutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned long redis_signature_hash(const char *text)
{
    unsigned long h = 5381;
    const unsigned char *p = (const unsigned char *)text;

    while (p && *p)
        h = ((h << 5) + h) + *p++;

    return h;
}

/***************************************************************************
 * Connection Management                                                   *
 ***************************************************************************/

bool redis_init(void)
{
    struct timeval timeout;
    const char *redis_password;
    const char *redis_host;
    int redis_port;
    redisReply *reply;

    // Check if Redis is enabled
    if (!game_settings.enable_redis) {
        log_string("Redis: Caching disabled in game settings");
        redis_available = false;
        return false;
    }

    log_string("Redis: Initializing connection...");

    // Get connection settings from game_settings (with defaults)
    redis_host = game_settings.redis_host ? game_settings.redis_host : "127.0.0.1";
    redis_port = game_settings.redis_port > 0 ? game_settings.redis_port : 6379;

    // Set timeout with defaults
    timeout.tv_sec = game_settings.redis_timeout_sec > 0 ? game_settings.redis_timeout_sec : 1;
    timeout.tv_usec = game_settings.redis_timeout_usec >= 0 ? game_settings.redis_timeout_usec : 500000;

    log_stringf("Redis: Connecting to %s:%d...", redis_host, redis_port);
    redis_ctx = redisConnectWithTimeout(redis_host, redis_port, timeout);

    if (redis_ctx == NULL || redis_ctx->err) {
        if (redis_ctx) {
            log_stringf("Redis: Connection error: %s", redis_ctx->errstr);
            redisFree(redis_ctx);
            redis_ctx = NULL;
        } else {
            log_string("Redis: Failed to allocate redis context");
        }
        redis_available = false;
        return false;
    }

    // Authenticate if password is configured
    // Environment variable overrides are already applied by json_apply_env_overrides()
    redis_password = game_settings.redis_password;

    if (redis_password && redis_password[0] != '\0') {
        log_string("Redis: Authenticating...");
        pthread_mutex_lock(&redis_mutex);
        reply = redisCommand(redis_ctx, "AUTH %s", redis_password);
        pthread_mutex_unlock(&redis_mutex);

        if (reply == NULL) {
            log_string("Redis: AUTH command failed - connection error");
            redisFree(redis_ctx);
            redis_ctx = NULL;
            redis_available = false;
            return false;
        }

        if (reply->type == REDIS_REPLY_ERROR) {
            log_stringf("Redis: AUTH failed: %s", reply->str);
            freeReplyObject(reply);
            redisFree(redis_ctx);
            redis_ctx = NULL;
            redis_available = false;
            return false;
        }

        if (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0) {
            log_string("Redis: Authentication successful");
        }
        freeReplyObject(reply);
    } else {
        log_string("Redis: No password configured - connecting without authentication");
    }

    // Test connection
    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "PING");
    pthread_mutex_unlock(&redis_mutex);
    if (reply == NULL) {
        log_string("Redis: PING failed");
        redisFree(redis_ctx);
        redis_ctx = NULL;
        redis_available = false;
        return false;
    }

    if (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "PONG") == 0) {
        log_string("Redis: Connection established successfully");
        redis_available = true;
        freeReplyObject(reply);

        // Check for RedisJSON module
        log_string("Redis: Checking for RedisJSON module...");
        pthread_mutex_lock(&redis_mutex);
        reply = redisCommand(redis_ctx, "MODULE LIST");
        pthread_mutex_unlock(&redis_mutex);
        if (reply && reply->type == REDIS_REPLY_ARRAY) {
            for (size_t i = 0; i < reply->elements; i++) {
                redisReply *module = reply->element[i];
                if (module->type == REDIS_REPLY_ARRAY && module->elements >= 2) {
                    redisReply *name_field = module->element[1];
                    if (name_field && name_field->type == REDIS_REPLY_STRING) {
                        if (strcasecmp(name_field->str, "ReJSON") == 0 ||
                            strcasecmp(name_field->str, "JSON") == 0) {
                            redis_json_available = true;
                            log_string("Redis: RedisJSON module detected - full caching with partial updates enabled");
                            break;
                        }
                    }
                }
            }
            freeReplyObject(reply);
        }

        if (!redis_json_available) {
            log_string("Redis: RedisJSON module NOT available - using fallback mode");
            log_string("Redis: Full character caching enabled (fallback mode)");
            log_string("Redis: See REDIS_JSON_SETUP.md for installation instructions");
        }

        return true;
    }

    log_string("Redis: Unexpected PING response");
    freeReplyObject(reply);
    redisFree(redis_ctx);
    redis_ctx = NULL;
    redis_available = false;
    return false;
}

void redis_shutdown(void)
{
    if (redis_ctx) {
        log_string("Redis: Closing connection...");
        redisFree(redis_ctx);
        redis_ctx = NULL;
    }
    redis_available = false;
}

redisContext *redis_get_connection(void)
{
    return redis_ctx;
}

redisContext *redis_new_context(void)
{
    const char *host = game_settings.redis_host ? game_settings.redis_host : "127.0.0.1";
    int         port = game_settings.redis_port  > 0 ? game_settings.redis_port  : 6379;
    struct timeval timeout;
    timeout.tv_sec  = game_settings.redis_timeout_sec  > 0 ? game_settings.redis_timeout_sec  : 1;
    timeout.tv_usec = game_settings.redis_timeout_usec >= 0 ? game_settings.redis_timeout_usec : 500000;

    redisContext *ctx = redisConnectWithTimeout(host, port, timeout);
    if (!ctx || ctx->err) {
        if (ctx) redisFree(ctx);
        return NULL;
    }

    const char *password = game_settings.redis_password;
    if (password && password[0] != '\0') {
        redisReply *reply = (redisReply *)redisCommand(ctx, "AUTH %s", password);
        if (!reply || reply->type == REDIS_REPLY_ERROR) {
            if (reply) freeReplyObject(reply);
            redisFree(ctx);
            return NULL;
        }
        freeReplyObject(reply);
    }

    return ctx;
}

void redis_release_connection(redisContext *c)
{
    // In single-threaded mode, this is a no-op
    // In future multi-threaded version, return to pool
}

bool redis_is_available(void)
{
    return redis_available && redis_ctx != NULL;
}

/***************************************************************************
 * Helper Functions                                                        *
 ***************************************************************************/

char *redis_key(const char *prefix, const char *name, const char *suffix)
{
    static char buf[256];
    snprintf(buf, sizeof(buf), "%s:%s:%s", prefix, name, suffix);
    return buf;
}

/***************************************************************************
 * Character Info Serialization                                            *
 ***************************************************************************/

CHAR_INFO_CACHE *char_to_info_cache(CHAR_DATA *ch)
{
    CHAR_INFO_CACHE *info;

    if (!ch || IS_NPC(ch)) {
        return NULL;
    }

    info = (CHAR_INFO_CACHE *)calloc(1, sizeof(CHAR_INFO_CACHE));
    if (!info) {
        pbugf(LOG_INIT, "Failed to allocate memory", NULL);
        return NULL;
    }

    // Basic info
    info->name = strdup(ch->name ? ch->name : "Unknown");
    info->level = ch->level;
    info->tot_level = ch->tot_level;
    info->remorts = IS_REMORT(ch) ? 1 : 0;  // Simple remort flag based on race
    info->race = strdup(ch->race ? ch->race->name : "unknown");
    info->title = strdup(ch->pcdata && ch->pcdata->title ? ch->pcdata->title : "");
    info->last_played = (long)current_time;
    info->is_active = (ch->desc != NULL);

    // Build class list - count active classes first
    info->num_classes = 0;
    if (ch->pcdata) {
        if (ch->pcdata->class_mage > 0 || ch->pcdata->second_class_mage > 0) info->num_classes++;
        if (ch->pcdata->class_cleric > 0 || ch->pcdata->second_class_cleric > 0) info->num_classes++;
        if (ch->pcdata->class_thief > 0 || ch->pcdata->second_class_thief > 0) info->num_classes++;
        if (ch->pcdata->class_warrior > 0 || ch->pcdata->second_class_warrior > 0) info->num_classes++;
    }

    if (info->num_classes > 0) {
        info->classes = (char **)calloc(info->num_classes, sizeof(char *));
        int class_idx = 0;
        if (ch->pcdata->class_mage > 0 || ch->pcdata->second_class_mage > 0) {
            info->classes[class_idx++] = strdup("Mage");
        }
        if (ch->pcdata->class_cleric > 0 || ch->pcdata->second_class_cleric > 0) {
            info->classes[class_idx++] = strdup("Cleric");
        }
        if (ch->pcdata->class_thief > 0 || ch->pcdata->second_class_thief > 0) {
            info->classes[class_idx++] = strdup("Thief");
        }
        if (ch->pcdata->class_warrior > 0 || ch->pcdata->second_class_warrior > 0) {
            info->classes[class_idx++] = strdup("Warrior");
        }
    }

    // Stats percentages
    info->health_pct = (ch->max_hit > 0) ? (ch->hit * 100 / ch->max_hit) : 100;
    info->mana_pct = (ch->max_mana > 0) ? (ch->mana * 100 / ch->max_mana) : 100;
    info->gold = ch->gold;
    info->experience = ch->exp;

    return info;
}

void free_char_info_cache(CHAR_INFO_CACHE *info)
{
    int i;

    if (!info) {
        return;
    }

    if (info->name) free(info->name);
    if (info->race) free(info->race);
    if (info->title) free(info->title);

    if (info->classes) {
        for (i = 0; i < info->num_classes; i++) {
            if (info->classes[i]) {
                free(info->classes[i]);
            }
        }
        free(info->classes);
    }

    free(info);
}

/***************************************************************************
 * Character Info Caching                                                  *
 ***************************************************************************/

bool redis_cache_char_info(CHAR_DATA *ch)
{
    redisReply *reply;
    char *key;
    char classes_buf[MAX_STRING_LENGTH];
    CHAR_INFO_CACHE *info;
    int i;

    if (!redis_is_available() || !ch || IS_NPC(ch)) {
        return false;
    }

    info = char_to_info_cache(ch);
    if (!info) {
        return false;
    }

    key = redis_key("char", ch->name, "info");

    // Build classes string
    classes_buf[0] = '\0';
    for (i = 0; i < info->num_classes; i++) {
        if (i > 0) strcat(classes_buf, ",");
        strcat(classes_buf, info->classes[i]);
    }

    // Cache as Redis hash (easy to query individual fields)
    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx,
        "HMSET %s "
        "name %s "
        "level %d "
        "tot_level %d "
        "remorts %d "
        "race %s "
        "classes %s "
        "title %s "
        "last_played %ld "
        "is_active %d "
        "health_pct %d "
        "mana_pct %d "
        "gold %ld "
        "experience %ld",
        key,
        info->name,
        info->level,
        info->tot_level,
        info->remorts,
        info->race,
        classes_buf,
        info->title,
        info->last_played,
        info->is_active ? 1 : 0,
        info->health_pct,
        info->mana_pct,
        info->gold,
        info->experience
    );

    if (reply == NULL) {
        log_stringf("Redis: Failed to cache info for %s", ch->name);
        stats.errors++;
        free_char_info_cache(info);
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    freeReplyObject(reply);

    // Set expiration
    reply = redisCommand(redis_ctx, "EXPIRE %s %d", key, REDIS_TTL_CHAR_INFO);
    if (reply) {
        freeReplyObject(reply);
    }

    stats.sets++;
    pthread_mutex_unlock(&redis_mutex);
    free_char_info_cache(info);

    log_stringf("Redis: Cached info for %s", ch->name);
    return true;
}

CHAR_INFO_CACHE *redis_get_char_info(const char *name)
{
    redisReply *reply;
    char *key;
    CHAR_INFO_CACHE *info;
    char *classes_str;
    char *token;
    int class_count;

    if (!redis_is_available() || !name) {
        return NULL;
    }

    key = redis_key("char", name, "info");

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "HGETALL %s", key);

    if (reply == NULL || reply->type != REDIS_REPLY_ARRAY || reply->elements == 0) {
        if (reply) freeReplyObject(reply);
        stats.misses++;
        pthread_mutex_unlock(&redis_mutex);
        return NULL;
    }

    info = (CHAR_INFO_CACHE *)calloc(1, sizeof(CHAR_INFO_CACHE));
    if (!info) {
        freeReplyObject(reply);
        pthread_mutex_unlock(&redis_mutex);
        return NULL;
    }

    // Parse hash fields (array of key-value pairs)
    for (size_t i = 0; i < reply->elements; i += 2) {
        char *field_name = reply->element[i]->str;
        char *field_value = reply->element[i + 1]->str;

        if (strcmp(field_name, "name") == 0) {
            info->name = strdup(field_value);
        } else if (strcmp(field_name, "level") == 0) {
            info->level = atoi(field_value);
        } else if (strcmp(field_name, "tot_level") == 0) {
            info->tot_level = atoi(field_value);
        } else if (strcmp(field_name, "remorts") == 0) {
            info->remorts = atoi(field_value);
        } else if (strcmp(field_name, "race") == 0) {
            info->race = strdup(field_value);
        } else if (strcmp(field_name, "classes") == 0) {
            // Parse comma-separated classes
            classes_str = strdup(field_value);
            class_count = 0;

            // Count classes
            token = strtok(classes_str, ",");
            while (token != NULL) {
                class_count++;
                token = strtok(NULL, ",");
            }
            free(classes_str);

            if (class_count > 0) {
                info->classes = (char **)calloc(class_count, sizeof(char *));
                info->num_classes = class_count;

                classes_str = strdup(field_value);
                token = strtok(classes_str, ",");
                int idx = 0;
                while (token != NULL && idx < class_count) {
                    info->classes[idx++] = strdup(token);
                    token = strtok(NULL, ",");
                }
                free(classes_str);
            }
        } else if (strcmp(field_name, "title") == 0) {
            info->title = strdup(field_value);
        } else if (strcmp(field_name, "last_played") == 0) {
            info->last_played = atol(field_value);
        } else if (strcmp(field_name, "is_active") == 0) {
            info->is_active = (atoi(field_value) != 0);
        } else if (strcmp(field_name, "health_pct") == 0) {
            info->health_pct = atoi(field_value);
        } else if (strcmp(field_name, "mana_pct") == 0) {
            info->mana_pct = atoi(field_value);
        } else if (strcmp(field_name, "gold") == 0) {
            info->gold = atol(field_value);
        } else if (strcmp(field_name, "experience") == 0) {
            info->experience = atol(field_value);
        }
    }

    freeReplyObject(reply);
    stats.hits++;
    pthread_mutex_unlock(&redis_mutex);

    return info;
}

bool redis_set_char_active(const char *name, bool active)
{
    redisReply *reply;
    char *key;

    if (!redis_is_available() || !name) {
        return false;
    }

    key = redis_key("char", name, "active");

    pthread_mutex_lock(&redis_mutex);
    if (active) {
        reply = redisCommand(redis_ctx, "SET %s 1 EX %d", key, REDIS_TTL_CHAR_ACTIVE);
    } else {
        reply = redisCommand(redis_ctx, "DEL %s", key);
    }
    pthread_mutex_unlock(&redis_mutex);

    if (reply) {
        freeReplyObject(reply);
        return true;
    }

    stats.errors++;
    return false;
}

bool redis_is_char_active(const char *name)
{
    redisReply *reply;
    char *key;
    bool active = false;

    if (!redis_is_available() || !name) {
        return false;
    }

    key = redis_key("char", name, "active");
    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "EXISTS %s", key);

    if (reply && reply->type == REDIS_REPLY_INTEGER) {
        active = (reply->integer == 1);
    }

    if (reply) {
        freeReplyObject(reply);
    }
    pthread_mutex_unlock(&redis_mutex);

    return active;
}

void redis_invalidate_char(const char *name)
{
    redisReply *reply;

    if (!redis_is_available() || !name) {
        return;
    }

    // Delete all character keys (use pattern matching)
    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "DEL %s %s",
        redis_key("char", name, "info"),
        redis_key("char", name, "active"));

    if (reply) {
        log_stringf("Redis: Invalidated cache for %s", name);
        stats.deletes++;
        freeReplyObject(reply);
    }
    pthread_mutex_unlock(&redis_mutex);
}

void redis_warm_cache(int max_chars)
{
    DIR *dir;
    struct dirent *entry;
    char path[256];
    char player_dir_buf[256];
    const char *player_dir = resolve_game_path(PLAYER_DIR, player_dir_buf, sizeof(player_dir_buf));
    char *name;
    int count = 0;

    if (!redis_is_available()) {
        return;
    }

    log_stringf("Redis: Warming cache with up to %d recently active characters...", max_chars);

    // Scan player directories (a-z)
    for (char initial = 'a'; initial <= 'z'; initial++) {
        if (count >= max_chars) break;

        snprintf(path, sizeof(path), "%s%c", player_dir, initial);
        dir = opendir(path);
        if (!dir) continue;

        while ((entry = readdir(dir)) != NULL && count < max_chars) {
            // Skip . and ..
            if (entry->d_name[0] == '.') continue;

            // Get character name (filename without path)
            name = entry->d_name;

            // Check if already cached
            CHAR_INFO_CACHE *existing = redis_get_char_info(name);
            if (existing) {
                free_char_info_cache(existing);
                continue; // Already cached
            }

            // Try to load and cache this character
            // We'll do this by loading just the basic info from the pfile
            // For now, we'll skip this as it would require loading full characters
            // In future phases with JSON, we can load just character.json

            // For Phase 1, cache warming will be done naturally as players login/save
        }

        closedir(dir);
    }

    log_stringf("Redis: Cache warming scan complete (%d characters checked)", count);
}

/***************************************************************************
 * Statistics                                                              *
 ***************************************************************************/

REDIS_STATS *redis_get_stats(void)
{
    redisReply *reply;

    if (!redis_is_available()) {
        return &stats;
    }

    pthread_mutex_lock(&redis_mutex);
    // Get key count
    reply = redisCommand(redis_ctx, "DBSIZE");
    if (reply && reply->type == REDIS_REPLY_INTEGER) {
        stats.keys_stored = reply->integer;
    }
    if (reply) freeReplyObject(reply);

    // Get memory usage
    reply = redisCommand(redis_ctx, "INFO memory");
    if (reply && reply->type == REDIS_REPLY_STRING) {
        char *used_memory = strstr(reply->str, "used_memory:");
        if (used_memory) {
            stats.memory_used = atol(used_memory + 12);
        }
    }
    if (reply) freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);

    return &stats;
}

void redis_print_stats(CHAR_DATA *ch)
{
    REDIS_STATS *s = redis_get_stats();

    if (!redis_is_available()) {
        send_to_char("Redis is not available.\n\r", ch);
        return;
    }

    send_to_char("{Y=== Redis Cache Statistics ==={x\n\r\n\r", ch);

    char buf[MAX_STRING_LENGTH];
    sprintf(buf, "Status:        {G%s{x\n\r", redis_available ? "Connected" : "Disconnected");
    send_to_char(buf, ch);

    sprintf(buf, "RedisJSON:     {%c%s{x\n\r",
            redis_json_available ? 'G' : 'Y',
            redis_json_available ? "Available (optimal)" : "Not available (fallback mode)");
    send_to_char(buf, ch);

    sprintf(buf, "Cache Hits:    {C%ld{x\n\r", s->hits);
    send_to_char(buf, ch);

    sprintf(buf, "Cache Misses:  {R%ld{x\n\r", s->misses);
    send_to_char(buf, ch);

    long total_requests = s->hits + s->misses;
    if (total_requests > 0) {
        sprintf(buf, "Hit Rate:      {Y%.1f%%{x\n\r", (float)s->hits * 100.0 / total_requests);
        send_to_char(buf, ch);
    }

    sprintf(buf, "Sets:          %ld\n\r", s->sets);
    send_to_char(buf, ch);

    sprintf(buf, "Deletes:       %ld\n\r", s->deletes);
    send_to_char(buf, ch);

    sprintf(buf, "Errors:        {R%ld{x\n\r", s->errors);
    send_to_char(buf, ch);

    sprintf(buf, "Keys Stored:   %ld\n\r", s->keys_stored);
    send_to_char(buf, ch);

    sprintf(buf, "Memory Used:   %.2f MB\n\r", s->memory_used / (1024.0 * 1024.0));
    send_to_char(buf, ch);
}

/***************************************************************************
 * Full Character Caching (Phase 2)                                       *
 ***************************************************************************/

bool redis_has_json_module(void)
{
    return redis_json_available;
}

bool redis_cache_char_full(CHAR_DATA *ch, json_t *char_json)
{
    redisReply *reply;
    char *key;
    char *json_str;

    if (!redis_is_available() || !ch || IS_NPC(ch) || !char_json) {
        return false;
    }

    key = redis_key("char", ch->name, "full");

    pthread_mutex_lock(&redis_mutex);
    if (redis_json_available) {
        // Use RedisJSON module for native JSON storage
        json_str = json_dumps(char_json, JSON_COMPACT);
        if (!json_str) {
            log_stringf("Redis: Failed to serialize JSON for %s", ch->name);
            stats.errors++;
            pthread_mutex_unlock(&redis_mutex);
            return false;
        }

        reply = redisCommand(redis_ctx, "JSON.SET %s $ %s", key, json_str);
        free(json_str);

        if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
            if (reply) {
                log_stringf("Redis: JSON.SET failed for %s: %s", ch->name, reply->str);
                freeReplyObject(reply);
            } else {
                log_stringf("Redis: JSON.SET connection error for %s", ch->name);
            }
            stats.errors++;
            pthread_mutex_unlock(&redis_mutex);
            return false;
        }
        freeReplyObject(reply);

    } else {
        // Fallback: Store as regular string
        json_str = json_dumps(char_json, JSON_COMPACT);
        if (!json_str) {
            log_stringf("Redis: Failed to serialize JSON for %s", ch->name);
            stats.errors++;
            pthread_mutex_unlock(&redis_mutex);
            return false;
        }

        reply = redisCommand(redis_ctx, "SET %s %s", key, json_str);
        free(json_str);

        if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
            if (reply) {
                log_stringf("Redis: SET failed for %s: %s", ch->name, reply->str);
                freeReplyObject(reply);
            } else {
                log_stringf("Redis: SET connection error for %s", ch->name);
            }
            stats.errors++;
            pthread_mutex_unlock(&redis_mutex);
            return false;
        }
        freeReplyObject(reply);
    }

    // Set expiration
    reply = redisCommand(redis_ctx, "EXPIRE %s %d", key, REDIS_TTL_CHAR_FULL);
    if (reply) {
        freeReplyObject(reply);
    }

    stats.sets++;
    pthread_mutex_unlock(&redis_mutex);
    log_stringf("Redis: Cached full character data for %s (%s mode)",
                ch->name, redis_json_available ? "RedisJSON" : "fallback");
    return true;
}

json_t *redis_get_char_full(const char *name)
{
    redisReply *reply;
    char *key;
    json_t *result = NULL;
    json_error_t error;

    if (!redis_is_available() || !name) {
        return NULL;
    }

    key = redis_key("char", name, "full");

    pthread_mutex_lock(&redis_mutex);
    if (redis_json_available) {
        // Use RedisJSON module
        reply = redisCommand(redis_ctx, "JSON.GET %s $", key);

        if (reply == NULL || reply->type == REDIS_REPLY_NIL) {
            if (reply) freeReplyObject(reply);
            stats.misses++;
            pthread_mutex_unlock(&redis_mutex);
            return NULL;
        }

        if (reply->type == REDIS_REPLY_STRING) {
            // RedisJSON returns JSON array: ["..."] or null
            // We need to parse the outer array and extract first element
            json_t *wrapper = json_loads(reply->str, 0, &error);
            if (wrapper && json_is_array(wrapper) && json_array_size(wrapper) > 0) {
                // Extract first element and incref it before decref wrapper
                result = json_array_get(wrapper, 0);
                if (result) {
                    json_incref(result);  // Caller will decref
                }
                json_decref(wrapper);
            }
            freeReplyObject(reply);

            if (result) {
                stats.hits++;
                log_stringf("Redis: Retrieved full character data for %s (RedisJSON mode)", name);
            } else {
                stats.misses++;
                log_stringf("Redis: Failed to parse RedisJSON response for %s", name);
            }
            pthread_mutex_unlock(&redis_mutex);
            return result;
        }

        freeReplyObject(reply);
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return NULL;

    } else {
        // Fallback: Get regular string
        reply = redisCommand(redis_ctx, "GET %s", key);

        if (reply == NULL || reply->type == REDIS_REPLY_NIL) {
            if (reply) freeReplyObject(reply);
            stats.misses++;
            pthread_mutex_unlock(&redis_mutex);
            return NULL;
        }

        if (reply->type == REDIS_REPLY_STRING) {
            result = json_loads(reply->str, 0, &error);
            freeReplyObject(reply);

            if (result) {
                stats.hits++;
                log_stringf("Redis: Retrieved full character data for %s (fallback mode)", name);
                pthread_mutex_unlock(&redis_mutex);
                return result;
            } else {
                stats.errors++;
                log_stringf("Redis: Failed to parse JSON for %s: %s", name, error.text);
                pthread_mutex_unlock(&redis_mutex);
                return NULL;
            }
        }

        freeReplyObject(reply);
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return NULL;
    }
}

bool redis_update_char_gold(const char *name, long gold)
{
    redisReply *reply;
    char *key;

    if (!redis_is_available() || !name || !redis_json_available) {
        return false;
    }

    key = redis_key("char", name, "full");

    // Use JSONPath to update just the gold field
    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "JSON.SET %s $.character.gold %ld", key, gold);

    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        if (reply) {
            log_stringf("Redis: Failed to update gold for %s: %s", name, reply->str);
            freeReplyObject(reply);
        }
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);
    log_stringf("Redis: Updated gold for %s to %ld (partial update)", name, gold);
    return true;
}

bool redis_update_char_exp(const char *name, long exp)
{
    redisReply *reply;
    char *key;

    if (!redis_is_available() || !name || !redis_json_available) {
        return false;
    }

    key = redis_key("char", name, "full");

    // Use JSONPath to update just the experience field
    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "JSON.SET %s $.character.exp %ld", key, exp);

    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        if (reply) {
            log_stringf("Redis: Failed to update exp for %s: %s", name, reply->str);
            freeReplyObject(reply);
        }
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);
    log_stringf("Redis: Updated exp for %s to %ld (partial update)", name, exp);
    return true;
}

bool redis_update_char_position(const char *name, int room_vnum)
{
    redisReply *reply;
    char *key;

    if (!redis_is_available() || !name || !redis_json_available) {
        return false;
    }

    key = redis_key("char", name, "full");

    // Use JSONPath to update room position
    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "JSON.SET %s $.character.room %d", key, room_vnum);

    if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
        if (reply) {
            log_stringf("Redis: Failed to update position for %s: %s", name, reply->str);
            freeReplyObject(reply);
        }
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);
    log_stringf("Redis: Updated position for %s to room %d (partial update)", name, room_vnum);
    return true;
}

/***************************************************************************
 * Account Caching (Phase 3)                                              *
 ***************************************************************************/

bool redis_cache_account_full(const char *account_name, json_t *account_json)
{
    redisReply *reply;
    char *key;
    char *json_str;

    if (!redis_is_available() || !account_name || !account_json) {
        return false;
    }

    key = redis_key("account", account_name, "full");

    pthread_mutex_lock(&redis_mutex);
    if (redis_json_available) {
        json_str = json_dumps(account_json, JSON_COMPACT);
        if (!json_str) {
            stats.errors++;
            pthread_mutex_unlock(&redis_mutex);
            return false;
        }

        reply = redisCommand(redis_ctx, "JSON.SET %s $ %s", key, json_str);
        free(json_str);

        if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
            if (reply) freeReplyObject(reply);
            stats.errors++;
            pthread_mutex_unlock(&redis_mutex);
            return false;
        }
        freeReplyObject(reply);

    } else {
        json_str = json_dumps(account_json, JSON_COMPACT);
        if (!json_str) {
            stats.errors++;
            pthread_mutex_unlock(&redis_mutex);
            return false;
        }

        reply = redisCommand(redis_ctx, "SET %s %s", key, json_str);
        free(json_str);

        if (reply == NULL || reply->type == REDIS_REPLY_ERROR) {
            if (reply) freeReplyObject(reply);
            stats.errors++;
            pthread_mutex_unlock(&redis_mutex);
            return false;
        }
        freeReplyObject(reply);
    }

    // Set expiration
    reply = redisCommand(redis_ctx, "EXPIRE %s %d", key, REDIS_TTL_CHAR_FULL);
    if (reply) {
        freeReplyObject(reply);
    }

    stats.sets++;
    pthread_mutex_unlock(&redis_mutex);
    log_stringf("Redis: Cached full account data for %s", account_name);
    return true;
}

json_t *redis_get_account_full(const char *account_name)
{
    redisReply *reply;
    char *key;
    json_t *result = NULL;
    json_error_t error;

    if (!redis_is_available() || !account_name) {
        return NULL;
    }

    key = redis_key("account", account_name, "full");

    pthread_mutex_lock(&redis_mutex);
    if (redis_json_available) {
        reply = redisCommand(redis_ctx, "JSON.GET %s $", key);

        if (reply == NULL || reply->type == REDIS_REPLY_NIL) {
            if (reply) freeReplyObject(reply);
            stats.misses++;
            pthread_mutex_unlock(&redis_mutex);
            return NULL;
        }

        if (reply->type == REDIS_REPLY_STRING) {
            json_t *wrapper = json_loads(reply->str, 0, &error);
            if (wrapper && json_is_array(wrapper) && json_array_size(wrapper) > 0) {
                result = json_array_get(wrapper, 0);
                if (result) {
                    json_incref(result);
                }
                json_decref(wrapper);
            }
            freeReplyObject(reply);

            if (result) {
                stats.hits++;
                log_stringf("Redis: Retrieved full account data for %s", account_name);
            } else {
                stats.misses++;
            }
            pthread_mutex_unlock(&redis_mutex);
            return result;
        }

        freeReplyObject(reply);
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return NULL;

    } else {
        reply = redisCommand(redis_ctx, "GET %s", key);

        if (reply == NULL || reply->type == REDIS_REPLY_NIL) {
            if (reply) freeReplyObject(reply);
            stats.misses++;
            pthread_mutex_unlock(&redis_mutex);
            return NULL;
        }

        if (reply->type == REDIS_REPLY_STRING) {
            result = json_loads(reply->str, 0, &error);
            freeReplyObject(reply);

            if (result) {
                stats.hits++;
                log_stringf("Redis: Retrieved full account data for %s", account_name);
                pthread_mutex_unlock(&redis_mutex);
                return result;
            } else {
                stats.errors++;
                pthread_mutex_unlock(&redis_mutex);
                return NULL;
            }
        }

        freeReplyObject(reply);
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return NULL;
    }
}

/***************************************************************************
 * World State Caching (Phase 2 - Persistence)                             *
 ***************************************************************************/

/*
 * Key format:
 *   persist:room:<room_id>     - Room state (vnum, wilds_uid_x_y_z, or clone_vnum_id0_id1)
 *   persist:mobile:<id0>_<id1> - Mobile state
 *   persist:object:<id0>_<id1> - Object state
 *   persist:dirty              - List of dirty keys to write to disk
 */

static char *redis_persist_key(const char *type, const char *id)
{
    static char buf[256];
    snprintf(buf, sizeof(buf), "persist:%s:%s", type, id);
    return buf;
}

/*
 * Cache a JSON string to Redis with TTL
 */
bool redis_cache_persist_data(const char *key, const char *json_str)
{
    redisReply *reply;

    if (!redis_is_available() || !key || !json_str) {
        return false;
    }

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "SETEX %s %d %s",
                        key, REDIS_TTL_WORLD_STATE, json_str);

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        log_stringf("Redis: SETEX error for %s: %s", key, reply->str);
        freeReplyObject(reply);
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    freeReplyObject(reply);
    stats.sets++;
    pthread_mutex_unlock(&redis_mutex);
    return true;
}

/*
 * Get cached JSON string from Redis
 * Returns NULL if not found or error (caller must free with free())
 */
char *redis_get_persist_data(const char *key)
{
    redisReply *reply;
    char *result = NULL;

    if (!redis_is_available() || !key) {
        return NULL;
    }

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "GET %s", key);

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return NULL;
    }

    if (reply->type == REDIS_REPLY_NIL) {
        freeReplyObject(reply);
        stats.misses++;
        pthread_mutex_unlock(&redis_mutex);
        return NULL;
    }

    if (reply->type == REDIS_REPLY_STRING) {
        result = strdup(reply->str);
        freeReplyObject(reply);
        stats.hits++;
        pthread_mutex_unlock(&redis_mutex);
        return result;
    }

    freeReplyObject(reply);
    stats.errors++;
    pthread_mutex_unlock(&redis_mutex);
    return NULL;
}

/*
 * Delete a cached key from Redis
 */
bool redis_delete_persist_data(const char *key)
{
    redisReply *reply;

    if (!redis_is_available() || !key) {
        return false;
    }

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "DEL %s", key);

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);
    return true;
}

/*
 * Push a key to the dirty queue for background writing
 */
bool redis_queue_dirty_key(const char *key)
{
    redisReply *reply;
    bool should_enqueue = false;

    if (!redis_is_available() || !key) {
        return false;
    }

    pthread_mutex_lock(&redis_mutex);

    /*
     * Deduplicate queue entries:
     * - SADD returns 1 when key is newly marked pending
     * - SADD returns 0 when key is already pending
     */
    reply = redisCommand(redis_ctx, "SADD persist:dirty:pending %s", key);

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        log_stringf("Redis: SADD error for dirty queue pending set: %s", reply->str);
        freeReplyObject(reply);
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    if (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0) {
        should_enqueue = true;
    }

    freeReplyObject(reply);

    if (!should_enqueue) {
        pthread_mutex_unlock(&redis_mutex);
        return true;
    }

    /* Use LPUSH to add to the front of the dirty queue */
    reply = redisCommand(redis_ctx, "LPUSH persist:dirty %s", key);

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        log_stringf("Redis: LPUSH error for dirty queue: %s", reply->str);
        freeReplyObject(reply);
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    freeReplyObject(reply);

    pthread_mutex_unlock(&redis_mutex);
    return true;
}

/*
 * Pop a key from the dirty queue (non-blocking)
 * Returns NULL if queue is empty
 * Caller must free the returned string
 * Note: timeout_sec kept for API compatibility but unused
 */
char *redis_pop_dirty_key(int timeout_sec)
{
    redisReply *reply;
    char *result = NULL;
    (void)timeout_sec;  /* unused - we use non-blocking RPOP now */

    if (!redis_is_available()) {
        return NULL;
    }

    pthread_mutex_lock(&redis_mutex);
    /* Use RPOP (non-blocking) to avoid holding mutex during blocking wait */
    reply = redisCommand(redis_ctx, "RPOP persist:dirty");

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return NULL;
    }

    /* RPOP returns the value string or nil if empty */
    if (reply->type == REDIS_REPLY_STRING) {
        result = strdup(reply->str);

        /* Mark key as no longer pending once it is popped for processing */
        redisReply *srem_reply = redisCommand(redis_ctx, "SREM persist:dirty:pending %s", reply->str);
        if (srem_reply) {
            freeReplyObject(srem_reply);
        }
    }

    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);
    return result;
}

/*
 * Get the current size of the dirty queue
 */
long redis_dirty_queue_size(void)
{
    redisReply *reply;
    long size = 0;

    if (!redis_is_available()) {
        return 0;
    }

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "LLEN persist:dirty");

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return 0;
    }

    if (reply->type == REDIS_REPLY_INTEGER) {
        size = reply->integer;
    }

    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);
    return size;
}

/*
 * High-level functions for caching persist entities
 */

bool redis_cache_room_state(const char *room_id, const char *json_str)
{
    char *key = redis_persist_key("room", room_id);
    if (!redis_cache_persist_data(key, json_str)) {
        return false;
    }
    return redis_queue_dirty_key(key);
}

bool redis_cache_mobile_state(unsigned long id0, unsigned long id1, const char *json_str)
{
    char id_buf[64];
    char *key;

    snprintf(id_buf, sizeof(id_buf), "%lu_%lu", id0, id1);
    key = redis_persist_key("mobile", id_buf);

    if (!redis_cache_persist_data(key, json_str)) {
        return false;
    }
    return redis_queue_dirty_key(key);
}

bool redis_cache_object_state(unsigned long id0, unsigned long id1, const char *json_str)
{
    char id_buf[64];
    char *key;

    snprintf(id_buf, sizeof(id_buf), "%lu_%lu", id0, id1);
    key = redis_persist_key("object", id_buf);

    if (!redis_cache_persist_data(key, json_str)) {
        return false;
    }
    return redis_queue_dirty_key(key);
}

char *redis_get_room_state(const char *room_id)
{
    return redis_get_persist_data(redis_persist_key("room", room_id));
}

char *redis_get_mobile_state(unsigned long id0, unsigned long id1)
{
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "%lu_%lu", id0, id1);
    return redis_get_persist_data(redis_persist_key("mobile", id_buf));
}

char *redis_get_object_state(unsigned long id0, unsigned long id1)
{
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "%lu_%lu", id0, id1);
    return redis_get_persist_data(redis_persist_key("object", id_buf));
}

/***************************************************************************
 * Area Caching                                                            *
 ***************************************************************************/

/*
 * Area cache key format: area:full:{normalized_name}
 *   e.g., area:full:midgaard, area:full:newthalos
 * Area dirty queue: uses main persist:dirty queue
 * Area warm queue: area:warm:queue (for async cache warming on boot)
 *
 * Normalized name = filename without .json suffix
 */

/* Normalize area filename by stripping .json suffix */
static void normalize_area_name(const char *filename, char *buf, size_t buf_size)
{
    size_t len;

    if (!filename || !buf || buf_size == 0) {
        if (buf && buf_size > 0) buf[0] = '\0';
        return;
    }

    strncpy(buf, filename, buf_size - 1);
    buf[buf_size - 1] = '\0';

    /* Strip .json suffix if present */
    len = strlen(buf);
    if (len > 5 && strcmp(buf + len - 5, ".json") == 0) {
        buf[len - 5] = '\0';
    }
}

static char *redis_area_key(const char *area_name)
{
    static char buf[128];
    snprintf(buf, sizeof(buf), "area:full:%s", area_name);
    return buf;
}

bool redis_cache_area_full(const char *filename, const char *json_str)
{
    char area_name[64];
    char *key;
    redisReply *reply;

    if (!redis_is_available() || !json_str || !filename || !filename[0]) {
        return false;
    }

    normalize_area_name(filename, area_name, sizeof(area_name));
    key = redis_area_key(area_name);

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "SET %s %s EX %d", key, json_str, REDIS_TTL_AREA_FULL);

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        log_stringf("Redis: SET error for area %s: %s", area_name, reply->str);
        freeReplyObject(reply);
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    freeReplyObject(reply);
    stats.sets++;
    pthread_mutex_unlock(&redis_mutex);
    return true;
}

char *redis_get_area_full(const char *filename)
{
    char area_name[64];
    char *key;
    char *result = NULL;
    redisReply *reply;

    if (!redis_is_available() || !filename || !filename[0]) {
        return NULL;
    }

    normalize_area_name(filename, area_name, sizeof(area_name));
    key = redis_area_key(area_name);

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "GET %s", key);

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return NULL;
    }

    if (reply->type == REDIS_REPLY_STRING) {
        result = strdup(reply->str);
        stats.hits++;
    } else {
        stats.misses++;
    }

    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);
    return result;
}

void redis_invalidate_area(const char *filename)
{
    char area_name[64];
    char *key;
    redisReply *reply;

    if (!redis_is_available() || !filename || !filename[0]) {
        return;
    }

    normalize_area_name(filename, area_name, sizeof(area_name));
    key = redis_area_key(area_name);

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "DEL %s", key);

    if (reply) {
        log_stringf("Redis: Invalidated area cache for %s", area_name);
        stats.deletes++;
        freeReplyObject(reply);
    }
    pthread_mutex_unlock(&redis_mutex);
}

bool redis_cache_area_state(const char *filename, const char *json_str)
{
    if (!json_str || !filename || !filename[0]) {
        return false;
    }

    /*
     * Areas are written to disk synchronously by json_area_save().
     * Cache writes must not enqueue persist:dirty work for area keys,
     * otherwise the background persist worker can replay and rewrite
     * large portions of AREA_DIR unexpectedly.
     */
    return redis_cache_area_full(filename, json_str);
}

/*
 * Async cache warming - queue area for background caching
 * This avoids blocking boot while caching potentially large areas
 */
void redis_queue_area_cache_warm(const char *filename, const char *json_str)
{
    redisReply *reply;
    char area_name[64];
    char key_buf[128];

    if (!redis_is_available() || !json_str || !filename || !filename[0]) {
        return;
    }

    normalize_area_name(filename, area_name, sizeof(area_name));

    /* Store the JSON temporarily with a warm: prefix */
    snprintf(key_buf, sizeof(key_buf), "area:warm:%s", area_name);

    pthread_mutex_lock(&redis_mutex);
    /* Store JSON data with short TTL (just for warming) */
    reply = redisCommand(redis_ctx, "SET %s %s EX 300", key_buf, json_str);
    if (reply) {
        freeReplyObject(reply);
    }

    /* Queue the area name for processing */
    reply = redisCommand(redis_ctx, "LPUSH area:warm:queue %s", area_name);
    if (reply) {
        freeReplyObject(reply);
    }
    pthread_mutex_unlock(&redis_mutex);
}

bool redis_process_area_cache_warm(void)
{
    redisReply *reply;
    char area_name[64];
    char key_buf[128];
    char *json_str;
    struct timeval start_time, end_time;
    long elapsed_ms;

    if (!redis_is_available()) {
        return false;
    }

    gettimeofday(&start_time, NULL);

    pthread_mutex_lock(&redis_mutex);
    /* Pop one area name from the warm queue */
    reply = redisCommand(redis_ctx, "RPOP area:warm:queue");

    if (reply == NULL || reply->type != REDIS_REPLY_STRING) {
        if (reply) freeReplyObject(reply);
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    strncpy(area_name, reply->str, sizeof(area_name) - 1);
    area_name[sizeof(area_name) - 1] = '\0';
    freeReplyObject(reply);

    /* Get the temporarily stored JSON */
    snprintf(key_buf, sizeof(key_buf), "area:warm:%s", area_name);
    reply = redisCommand(redis_ctx, "GET %s", key_buf);

    if (reply == NULL || reply->type != REDIS_REPLY_STRING) {
        if (reply) freeReplyObject(reply);
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    json_str = reply->str;

    /* Move to permanent cache location */
    redisReply *set_reply = redisCommand(redis_ctx, "SET %s %s EX %d",
        redis_area_key(area_name), json_str, REDIS_TTL_AREA_FULL);
    if (set_reply) {
        freeReplyObject(set_reply);
        stats.sets++;
    }

    /* Delete the temporary warm key */
    redisReply *del_reply = redisCommand(redis_ctx, "DEL %s", key_buf);
    if (del_reply) {
        freeReplyObject(del_reply);
    }

    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);

    gettimeofday(&end_time, NULL);
    elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                (end_time.tv_usec - start_time.tv_usec) / 1000;

    if (elapsed_ms > 50) {
        log_stringf("PERFORMANCE redis_process_area_cache_warm: area %s took %ldms", area_name, elapsed_ms);
    } else {
        log_stringf("Redis: Warmed cache for area %s (%ldms)", area_name, elapsed_ms);
    }
    return true;
}

long redis_area_cache_warm_queue_size(void)
{
    redisReply *reply;
    long size = 0;

    if (!redis_is_available()) {
        return 0;
    }

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "LLEN area:warm:queue");

    if (reply && reply->type == REDIS_REPLY_INTEGER) {
        size = reply->integer;
    }

    if (reply) {
        freeReplyObject(reply);
    }
    pthread_mutex_unlock(&redis_mutex);
    return size;
}

/***************************************************************************
 * Leaderboard Operations (Sorted Sets)                                   *
 ***************************************************************************/

/* Board names used for redis_leaderboard_remove_all */
static const char *leaderboard_boards[] = {
    "pkers", "cpkers", "wealthiest", "monsters", "quests", "deaths",
    NULL
};

/**
 * redis_leaderboard_update - Set a player's score in a leaderboard sorted set
 *
 * Uses ZADD which creates or overwrites the member's score.
 */
bool redis_leaderboard_update(const char *board_name, const char *player_name, double score)
{
    redisReply *reply;
    char key[256];

    if (!redis_is_available() || !board_name || !player_name) {
        return false;
    }

    snprintf(key, sizeof(key), "leaderboard:%s", board_name);

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "ZADD %s %f %s", key, score, player_name);

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        log_stringf("Redis: ZADD error on %s: %s", key, reply->str);
        freeReplyObject(reply);
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    freeReplyObject(reply);
    stats.sets++;
    pthread_mutex_unlock(&redis_mutex);
    return true;
}

/**
 * redis_leaderboard_get_top - Retrieve top N entries (highest score first)
 *
 * Uses ZREVRANGE with WITHSCORES. Reply is an array of alternating
 * name/score strings.
 */
int redis_leaderboard_get_top(const char *board_name, int max_entries,
                              char **names, double *scores)
{
    redisReply *reply;
    char key[256];
    int count = 0;

    if (!redis_is_available() || !board_name || !names || !scores || max_entries <= 0) {
        return 0;
    }

    snprintf(key, sizeof(key), "leaderboard:%s", board_name);

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "ZREVRANGE %s 0 %d WITHSCORES", key, max_entries - 1);

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return 0;
    }

    if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 2) {
        count = (int)(reply->elements / 2);
        for (int i = 0; i < count; i++) {
            names[i] = strdup(reply->element[i * 2]->str);
            scores[i] = strtod(reply->element[i * 2 + 1]->str, NULL);
        }
        stats.hits++;
    } else {
        stats.misses++;
    }

    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);
    return count;
}

/**
 * redis_leaderboard_get_bottom - Retrieve bottom N entries (lowest score first)
 *
 * Uses ZRANGE with WITHSCORES. Used for the worst-ratio leaderboard.
 */
int redis_leaderboard_get_bottom(const char *board_name, int max_entries,
                                 char **names, double *scores)
{
    redisReply *reply;
    char key[256];
    int count = 0;

    if (!redis_is_available() || !board_name || !names || !scores || max_entries <= 0) {
        return 0;
    }

    snprintf(key, sizeof(key), "leaderboard:%s", board_name);

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "ZRANGE %s 0 %d WITHSCORES", key, max_entries - 1);

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return 0;
    }

    if (reply->type == REDIS_REPLY_ARRAY && reply->elements >= 2) {
        count = (int)(reply->elements / 2);
        for (int i = 0; i < count; i++) {
            names[i] = strdup(reply->element[i * 2]->str);
            scores[i] = strtod(reply->element[i * 2 + 1]->str, NULL);
        }
        stats.hits++;
    } else {
        stats.misses++;
    }

    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);
    return count;
}

/**
 * redis_leaderboard_remove - Remove a player from a specific leaderboard
 */
bool redis_leaderboard_remove(const char *board_name, const char *player_name)
{
    redisReply *reply;
    char key[256];

    if (!redis_is_available() || !board_name || !player_name) {
        return false;
    }

    snprintf(key, sizeof(key), "leaderboard:%s", board_name);

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "ZREM %s %s", key, player_name);

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    freeReplyObject(reply);
    stats.deletes++;
    pthread_mutex_unlock(&redis_mutex);
    return true;
}

/**
 * redis_leaderboard_remove_all - Remove a player from ALL leaderboards
 *
 * Called on character deletion/retirement. Also cleans up ratio hash data.
 */
void redis_leaderboard_remove_all(const char *player_name)
{
    redisReply *reply;
    char kills_field[256], deaths_field[256];

    if (!redis_is_available() || !player_name) {
        return;
    }

    /* Remove from each sorted set */
    for (int i = 0; leaderboard_boards[i] != NULL; i++) {
        redis_leaderboard_remove(leaderboard_boards[i], player_name);
    }

    /* Remove ratio hash data */
    snprintf(kills_field, sizeof(kills_field), "%s:kills", player_name);
    snprintf(deaths_field, sizeof(deaths_field), "%s:deaths", player_name);

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "HDEL leaderboard:ratio:data %s %s",
                         kills_field, deaths_field);
    if (reply) {
        freeReplyObject(reply);
    }
    stats.deletes++;
    pthread_mutex_unlock(&redis_mutex);
}

/**
 * redis_leaderboard_set_ratio_data - Store kills/deaths for ratio computation
 *
 * Uses HSET on a single hash key with per-player fields.
 */
bool redis_leaderboard_set_ratio_data(const char *player_name,
                                       int total_kills, int total_deaths)
{
    redisReply *reply;
    char kills_field[256], deaths_field[256];

    if (!redis_is_available() || !player_name) {
        return false;
    }

    snprintf(kills_field, sizeof(kills_field), "%s:kills", player_name);
    snprintf(deaths_field, sizeof(deaths_field), "%s:deaths", player_name);

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx,
                         "HSET leaderboard:ratio:data %s %d %s %d",
                         kills_field, total_kills,
                         deaths_field, total_deaths);

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return false;
    }

    freeReplyObject(reply);
    stats.sets++;
    pthread_mutex_unlock(&redis_mutex);
    return true;
}

/**
 * redis_leaderboard_get_ratio_data - Compute ratio leaderboard from hash data
 *
 * Reads all kills/deaths from leaderboard:ratio:data and computes
 * KDR = kills / max(1,deaths), then returns sorted ascending.
 */
int redis_leaderboard_get_ratio_data(int max_entries, char **names,
                                      double *scores, int min_total_fights)
{
    redisReply *reply;
    int count = 0;

    if (!redis_is_available() || !names || !scores || max_entries <= 0) {
        return 0;
    }

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "HGETALL leaderboard:ratio:data");

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return 0;
    }

    if (reply->type != REDIS_REPLY_ARRAY || reply->elements < 2) {
        stats.misses++;
        freeReplyObject(reply);
        pthread_mutex_unlock(&redis_mutex);
        return 0;
    }

    stats.hits++;

    typedef struct {
        char name[256];
        int kills;
        int deaths;
        bool has_kills;
        bool has_deaths;
    } ratio_entry_t;

    int max_players = (int)(reply->elements / 2);
    ratio_entry_t *entries = calloc(max_players, sizeof(ratio_entry_t));
    int num_players = 0;

    if (!entries) {
        freeReplyObject(reply);
        pthread_mutex_unlock(&redis_mutex);
        return 0;
    }

    for (size_t i = 0; i + 1 < reply->elements; i += 2) {
        const char *field = reply->element[i]->str;
        int value = atoi(reply->element[i + 1]->str);
        const char *sep = strrchr(field, ':');
        if (!sep) continue;

        char player[256];
        size_t name_len = (size_t)(sep - field);
        if (name_len >= sizeof(player)) continue;
        memcpy(player, field, name_len);
        player[name_len] = '\0';

        const char *suffix = sep + 1;
        int idx = -1;
        for (int j = 0; j < num_players; j++) {
            if (strcmp(entries[j].name, player) == 0) {
                idx = j;
                break;
            }
        }
        if (idx < 0) {
            idx = num_players++;
            strlcpy(entries[idx].name, player, sizeof(entries[idx].name));
        }

        if (strcmp(suffix, "kills") == 0) {
            entries[idx].kills = value;
            entries[idx].has_kills = true;
        } else if (strcmp(suffix, "deaths") == 0) {
            entries[idx].deaths = value;
            entries[idx].has_deaths = true;
        }
    }

    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);

    typedef struct {
        char name[256];
        double ratio;
    } ratio_result_t;

    ratio_result_t *results = calloc(num_players, sizeof(ratio_result_t));
    int num_results = 0;

    if (!results) {
        free(entries);
        return 0;
    }

    for (int i = 0; i < num_players; i++) {
        if (!entries[i].has_kills || !entries[i].has_deaths) continue;

        int total = entries[i].kills + entries[i].deaths;
        if (total < min_total_fights) continue;
        if (entries[i].deaths == 0) continue;

        results[num_results].ratio = (double)entries[i].kills * 100.0 / (double)total;
        strncpy(results[num_results].name, entries[i].name, sizeof(results[num_results].name) - 1);
        results[num_results].name[sizeof(results[num_results].name) - 1] = '\0';
        num_results++;
    }

    free(entries);

    for (int i = 0; i < num_results - 1; i++) {
        for (int j = i + 1; j < num_results; j++) {
            if (results[j].ratio < results[i].ratio) {
                ratio_result_t tmp = results[i];
                results[i] = results[j];
                results[j] = tmp;
            }
        }
    }

    count = num_results < max_entries ? num_results : max_entries;
    for (int i = 0; i < count; i++) {
        names[i] = strdup(results[i].name);
        scores[i] = results[i].ratio;
    }

    free(results);
    return count;
}

/**
 * redis_leaderboard_count - Get number of entries in a leaderboard
 */
long redis_leaderboard_count(const char *board_name)
{
    redisReply *reply;
    char key[256];
    long count = 0;

    if (!redis_is_available() || !board_name) {
        return 0;
    }

    snprintf(key, sizeof(key), "leaderboard:%s", board_name);

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "ZCARD %s", key);

    if (reply == NULL) {
        stats.errors++;
        pthread_mutex_unlock(&redis_mutex);
        return 0;
    }

    if (reply->type == REDIS_REPLY_INTEGER) {
        count = reply->integer;
    }

    freeReplyObject(reply);
    pthread_mutex_unlock(&redis_mutex);
    return count;
}

bool redis_audit_error_increment(const char *signature)
{
    redisReply *reply;
    char key[128];
    unsigned long hash;

    if (!redis_is_available() || IS_NULLSTR(signature))
        return false;

    hash = redis_signature_hash(signature);
    snprintf(key, sizeof(key), "audit:error:%08lx:count", hash);

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "INCR %s", key);
    pthread_mutex_unlock(&redis_mutex);

    if (!reply)
        return false;

    freeReplyObject(reply);
    return true;
}

long redis_audit_error_get_count(const char *signature)
{
    redisReply *reply;
    char key[128];
    unsigned long hash;
    long value = -1;

    if (!redis_is_available() || IS_NULLSTR(signature))
        return -1;

    hash = redis_signature_hash(signature);
    snprintf(key, sizeof(key), "audit:error:%08lx:count", hash);

    pthread_mutex_lock(&redis_mutex);
    reply = redisCommand(redis_ctx, "GET %s", key);
    pthread_mutex_unlock(&redis_mutex);

    if (!reply)
        return -1;

    if (reply->type == REDIS_REPLY_STRING || reply->type == REDIS_REPLY_INTEGER)
        value = atol(reply->str ? reply->str : "0");
    else if (reply->type == REDIS_REPLY_NIL)
        value = 0;

    freeReplyObject(reply);
    return value;
}
