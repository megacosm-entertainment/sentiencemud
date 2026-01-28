#ifndef __MUD_LOG_H__
#define __MUD_LOG_H__

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

// Initialization and shutdown
int log_init(const char *config_path);
void log_shutdown(void);
void log_set_unit_test_only(bool enabled);

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
