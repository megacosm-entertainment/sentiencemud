#ifndef __MUD_LOG_H__
#define __MUD_LOG_H__

#include <stdbool.h>
#include <stdint.h>
#include "zlog.h"

// Log levels
typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_CRITICAL,
    LOG_LEVEL_BUG,
    LOG_LEVEL_SQL,
    LOG_LEVEL_HTTP,
    LOG_LEVEL_SCRIPT,
    LOG_LEVEL_SECURITY
} log_level;

typedef enum {
    EVENT_SEV_INFO,
    EVENT_SEV_WARN,
    EVENT_SEV_ERROR,
    EVENT_SEV_DEBUG,
    EVENT_SEV_CRITICAL,
    EVENT_SEV_BUG
} event_severity_t;

typedef enum {
    ERROR_CODE_NONE = 0,
    ERROR_CODE_INVALID_ARGUMENT,
    ERROR_CODE_PERMISSION_DENIED,
    ERROR_CODE_NOT_FOUND,
    ERROR_CODE_STATE_CONFLICT,
    ERROR_CODE_DEPENDENCY_FAILURE,
    ERROR_CODE_INTERNAL,
    ERROR_CODE_SCRIPT_RUNTIME,
    ERROR_CODE_SECURITY
} error_code_t;

typedef enum {
    EVENT_DOMAIN_SYSTEM,
    EVENT_DOMAIN_SECURITY,
    EVENT_DOMAIN_COMBAT,
    EVENT_DOMAIN_QUEST,
    EVENT_DOMAIN_OLC,
    EVENT_DOMAIN_ADMIN,
    EVENT_DOMAIN_SCRIPT,
    EVENT_DOMAIN_ECONOMY,
    EVENT_DOMAIN_COMMAND
} event_domain_t;

typedef struct {
    const char *actor_type;
    const char *actor_id;
    const char *action;
    const char *target_type;
    const char *target_id;
    int64_t value;
    int64_t duration_ms;
    const char *extra_json;
} log_context_t;

typedef struct {
    event_severity_t severity;
    const char *category;
    error_code_t error_code;
    const char *public_message;
    const char *staff_message;
    long wiznet_flag;
    long wiznet_skip_flag;
    int wiznet_min_rank;
    const char *plain_message;
    const log_context_t *context;
    const char *source_file;
    long source_line;
    const char *source_func;
    bool skip_flat_file;  // When true, suppress zlog write (stream-only); zero-init = false
} log_event_t;

// Log categories - mapped to zlog categories
#define LOG_INIT      "init"      // Initialization messages
#define LOG_INFO      "info"      // General informational messages
#define LOG_WARN      "warn"      // Warnings
#define LOG_ERROR     "error"     // Errors
#define LOG_DEBUG     "debug"     // Debug messages
#define LOG_CRITICAL  "critical"  // Critical errors
#define LOG_SQL       "sql"       // SQL queries
#define LOG_HTTP      "http"      // HTTP requests
#define LOG_SCRIPT    "script"    // Scripting messages
#define LOG_SECURITY  "security"  // Security-related messages
#define LOG_COMBAT    "combat"    // Combat-related messages
#define LOG_SCRIPTS   "scripts"   // Scripts-related messages
#define LOG_OLC       "olc"       // OLC-related messages
#define LOG_QUEST     "quest"     // Quest-related messages
#define LOG_UNIT_TESTS "unit_tests" // Unit test framework messages
#define LOG_ADMIN      "admin"     // Admin actions and alerts

// Initialization and shutdown
int log_init(const char *config_path);
void log_shutdown(void);
void log_set_unit_test_only(bool enabled);
const char *log_category_for_domain(event_domain_t domain);
void log_emit_event(const log_event_t *event, void *public_recipient);
void log_emit_event_f(const log_event_t *base_event, void *public_recipient, const char *plain_fmt, ...);

// Convenience macros for structured context — opt-in for new call sites.
// All existing call sites (plog, plogf, log_string, bug, etc.) are unchanged.

// Security/authentication event with actor identity
#define log_security_actor(actor_type, actor_id, msg) \
    do { \
        log_context_t _lctx = { \
            .actor_type = (actor_type), .actor_id = (actor_id), .action = "security" \
        }; \
        log_event_t _lev = { \
            .severity = EVENT_SEV_INFO, .category = LOG_SECURITY, \
            .plain_message = (msg), .context = &_lctx, \
            .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__ \
        }; \
        log_emit_event(&_lev, NULL); \
    } while(0)

// Combat hit with attacker/victim/damage
#define log_combat_hit(attacker, victim, damage) \
    do { \
        log_context_t _lctx = { \
            .actor_type = "char", .actor_id = (attacker), .action = "hit", \
            .target_type = "char", .target_id = (victim), .value = (damage) \
        }; \
        log_event_t _lev = { \
            .severity = EVENT_SEV_INFO, .category = LOG_COMBAT, \
            .plain_message = "Combat hit", .context = &_lctx, \
            .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__ \
        }; \
        log_emit_event(&_lev, NULL); \
    } while(0)

// Performance warning with component and duration
#define log_perf(component, duration_ms_val, msg) \
    do { \
        log_context_t _lctx = { \
            .actor_type = "system", .actor_id = (component), \
            .duration_ms = (duration_ms_val) \
        }; \
        log_event_t _lev = { \
            .severity = EVENT_SEV_WARN, .category = LOG_DEBUG, \
            .plain_message = (msg), .context = &_lctx, \
            .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__ \
        }; \
        log_emit_event(&_lev, NULL); \
    } while(0)

// Character action with optional target and extra JSON
#define log_char_action(action_str, char_name, target_name, extra) \
    do { \
        log_context_t _lctx = { \
            .actor_type = "char", .actor_id = (char_name), .action = (action_str), \
            .target_type = "char", .target_id = (target_name), .extra_json = (extra) \
        }; \
        log_event_t _lev = { \
            .severity = EVENT_SEV_INFO, .category = LOG_INFO, \
            .plain_message = (action_str), .context = &_lctx, \
            .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__ \
        }; \
        log_emit_event(&_lev, NULL); \
    } while(0)

// Logging functions - now macros to capture caller info
#define log_message(level, category, message) \
    _log_message(level, category, message, __FILE__, __LINE__, __func__)

#define log_message_f(level, category, format, ...) \
    _log_message_f(level, category, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

// Internal functions
void _log_message(log_level level, const char *category, const char *message, const char *file, long line, const char *func);
void _log_message_f(log_level level, const char *category, const char *file, long line, const char *func, const char *format, ...);

// Stack trace logging (debug builds only - requires MUD_DEBUG)
// On release builds, these functions fall back to normal logging without traces.
// Maximum depth of stack trace to capture
#define LOG_STACKTRACE_MAX_DEPTH 64

// Get a stack trace as a newly allocated string (caller must free)
// skip_frames: number of frames to skip from the top (0 = include all)
// Returns NULL on failure or if not a debug build
char *log_get_stacktrace(int skip_frames);

// Log a message with an attached stack trace
#define log_stacktrace(level, category, message) \
    _log_stacktrace(level, category, message, __FILE__, __LINE__, __func__)

#define log_stacktrace_f(level, category, format, ...) \
    _log_stacktrace_f(level, category, __FILE__, __LINE__, __func__, format, ##__VA_ARGS__)

// Internal stack trace functions
void _log_stacktrace(log_level level, const char *category, const char *message, const char *file, long line, const char *func);
void _log_stacktrace_f(log_level level, const char *category, const char *file, long line, const char *func, const char *format, ...);

// Crash handler - installs signal handlers for SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL
// On crash: logs stack trace, then re-raises signal to generate core dump
// core_dir: directory for core dumps (NULL = current directory)
// Call this early in main() after log_init()
void log_install_crash_handler(const char *core_dir);

#endif // __MUD_LOG_H__
