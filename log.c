#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include "zlog.h"

int log_init(const char *config_path) {
    int rc = zlog_init(config_path);
    if (rc) {
        printf("Failed to initialize logger, rc=%d\n", rc);
        return -1;
    }
    return 0;
}

void log_shutdown(void) {
    zlog_fini();
}

void log_message(log_level level, const char *category, const char *message) {
    zlog_category_t *c = zlog_get_category(category);
    if (c) {
        switch (level) {
            case LOG_LEVEL_INFO:
                zlog_info(c, "%s", message);
                break;
            case LOG_LEVEL_WARN:
                zlog_warn(c, "%s", message);
                break;
            case LOG_LEVEL_ERROR:
                zlog_error(c, "%s", message);
                break;
            case LOG_LEVEL_DEBUG:
                zlog_debug(c, "%s", message);
                break;
            case LOG_LEVEL_CRITICAL:
                zlog_fatal(c, "%s", message);
                break;
            case LOG_LEVEL_BUG:
                 zlog_fatal(c, "BUG: %s", message);
                 break;
            default:
                zlog_error(c, "Invalid log level %d for message: %s", level, message);
                break;
        }
    }
}

void log_message_f(log_level level, const char *category, const char *format, ...) {
    zlog_category_t *c = zlog_get_category(category);
    if (c) {
        va_list args;
        va_start(args, format);
        switch (level) {
            case LOG_LEVEL_INFO:
                vzlog_info(c, format, args);
                break;
            case LOG_LEVEL_WARN:
                vzlog_warn(c, format, args);
                break;
            case LOG_LEVEL_ERROR:
                vzlog_error(c, format, args);
                break;
            case LOG_LEVEL_DEBUG:
                vzlog_debug(c, format, args);
                break;
            case LOG_LEVEL_CRITICAL:
                vzlog_fatal(c, format, args);
                break;
            case LOG_LEVEL_BUG:
                vzlog_fatal(c, format, args);
                break;
            default:
                // Need to format the message to include it in the log
                // zlog doesn't have a function that takes a va_list and a format string
                // for an arbitrary log level, so we have to format it ourselves.
                {
                    char buf[8192];
                    vsnprintf(buf, sizeof(buf), format, args);
                    zlog_error(c, "Invalid log level %d for message: %s", level, buf);
                }
                break;
        }
        va_end(args);
    }
}

