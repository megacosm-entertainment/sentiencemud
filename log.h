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

// Initialization and shutdown
int log_init(const char *config_path);
void log_shutdown(void);

// Logging functions
void log_message(log_level level, const char *category, const char *message);
void log_message_f(log_level level, const char *category, const char *format, ...);

#endif // __MUD_LOG_H__
