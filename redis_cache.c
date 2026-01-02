/***************************************************************************
 *  Redis Cache Module - Implementation                                    *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <dirent.h>
#include "redis_cache.h"
#include "merc.h"

/***************************************************************************
 * Global State                                                            *
 ***************************************************************************/

static redisContext *redis_ctx = NULL;
static bool redis_available = false;
static REDIS_STATS stats = {0};

/***************************************************************************
 * Connection Management                                                   *
 ***************************************************************************/

bool redis_init(void)
{
    struct timeval timeout = { REDIS_TIMEOUT_SEC, REDIS_TIMEOUT_USEC };

    log_string("Redis: Initializing connection...");

    redis_ctx = redisConnectWithTimeout(REDIS_HOST, REDIS_PORT, timeout);

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

    // Test connection
    redisReply *reply = redisCommand(redis_ctx, "PING");
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
    int i;

    if (!ch || IS_NPC(ch)) {
        return NULL;
    }

    info = (CHAR_INFO_CACHE *)calloc(1, sizeof(CHAR_INFO_CACHE));
    if (!info) {
        bug("char_to_info_cache: failed to allocate memory", 0);
        return NULL;
    }

    // Basic info
    info->name = strdup(ch->name ? ch->name : "Unknown");
    info->level = ch->level;
    info->tot_level = ch->tot_level;
    info->remorts = IS_REMORT(ch) ? 1 : 0;  // Simple remort flag based on race
    info->race = strdup(race_table[ch->race].name);
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
        return false;
    }

    freeReplyObject(reply);

    // Set expiration
    reply = redisCommand(redis_ctx, "EXPIRE %s %d", key, REDIS_TTL_CHAR_INFO);
    if (reply) {
        freeReplyObject(reply);
    }

    stats.sets++;
    free_char_info_cache(info);

    log_stringf("Redis: Cached info for %s", ch->name);
    return true;
}

CHAR_INFO_CACHE *redis_get_char_info(const char *name)
{
    redisReply *reply, *field;
    char *key;
    CHAR_INFO_CACHE *info;
    char *classes_str;
    char *token;
    int class_count;

    if (!redis_is_available() || !name) {
        return NULL;
    }

    key = redis_key("char", name, "info");

    reply = redisCommand(redis_ctx, "HGETALL %s", key);

    if (reply == NULL || reply->type != REDIS_REPLY_ARRAY || reply->elements == 0) {
        if (reply) freeReplyObject(reply);
        stats.misses++;
        return NULL;
    }

    info = (CHAR_INFO_CACHE *)calloc(1, sizeof(CHAR_INFO_CACHE));
    if (!info) {
        freeReplyObject(reply);
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

    if (active) {
        reply = redisCommand(redis_ctx, "SET %s 1 EX %d", key, REDIS_TTL_CHAR_ACTIVE);
    } else {
        reply = redisCommand(redis_ctx, "DEL %s", key);
    }

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
    reply = redisCommand(redis_ctx, "EXISTS %s", key);

    if (reply && reply->type == REDIS_REPLY_INTEGER) {
        active = (reply->integer == 1);
    }

    if (reply) {
        freeReplyObject(reply);
    }

    return active;
}

void redis_invalidate_char(const char *name)
{
    redisReply *reply;

    if (!redis_is_available() || !name) {
        return;
    }

    // Delete all character keys (use pattern matching)
    reply = redisCommand(redis_ctx, "DEL %s %s",
        redis_key("char", name, "info"),
        redis_key("char", name, "active"));

    if (reply) {
        log_stringf("Redis: Invalidated cache for %s", name);
        stats.deletes++;
        freeReplyObject(reply);
    }
}

void redis_warm_cache(int max_chars)
{
    DIR *dir;
    struct dirent *entry;
    char path[256];
    char *name;
    int count = 0;

    if (!redis_is_available()) {
        return;
    }

    log_stringf("Redis: Warming cache with up to %d recently active characters...", max_chars);

    // Scan player directories (a-z)
    for (char initial = 'a'; initial <= 'z'; initial++) {
        if (count >= max_chars) break;

        sprintf(path, "%s%c", PLAYER_DIR, initial);
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
