#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include "zlog.h"

#ifdef MUD_DEBUG
#include <backtrace.h>

// libbacktrace state (initialized lazily)
static struct backtrace_state *bt_state = NULL;

// Structure to accumulate stack trace output
typedef struct {
    char *buffer;
    size_t size;
    size_t used;
    int frame_num;
    int skip_frames;
} stacktrace_data;

static void bt_error_callback(void *data, const char *msg, int errnum) {
    (void)data;
    (void)errnum;
    fprintf(stderr, "libbacktrace error: %s\n", msg);
}

static int bt_full_callback(void *data, uintptr_t pc, const char *filename,
                            int lineno, const char *function) {
    stacktrace_data *sd = (stacktrace_data *)data;
    (void)pc;

    // Skip requested frames
    if (sd->skip_frames > 0) {
        sd->skip_frames--;
        return 0;
    }

    // Ensure we have space (grow if needed)
    size_t needed = 256; // Enough for one frame
    if (sd->used + needed > sd->size) {
        size_t new_size = sd->size * 2;
        if (new_size < sd->used + needed) {
            new_size = sd->used + needed + 1024;
        }
        char *new_buf = realloc(sd->buffer, new_size);
        if (!new_buf) {
            return 1; // Stop on allocation failure
        }
        sd->buffer = new_buf;
        sd->size = new_size;
    }

    // Format frame: #N function at file:line
    int written;
    if (filename && function) {
        written = sprintf(sd->buffer + sd->used, "  #%d %s at %s:%d\n",
                          sd->frame_num, function, filename, lineno);
    } else if (function) {
        written = sprintf(sd->buffer + sd->used, "  #%d %s\n",
                          sd->frame_num, function);
    } else if (filename) {
        written = sprintf(sd->buffer + sd->used, "  #%d <unknown> at %s:%d\n",
                          sd->frame_num, filename, lineno);
    } else {
        written = sprintf(sd->buffer + sd->used, "  #%d <unknown>\n",
                          sd->frame_num);
    }

    sd->used += written;
    sd->frame_num++;

    // Limit depth
    if (sd->frame_num >= LOG_STACKTRACE_MAX_DEPTH) {
        return 1;
    }

    return 0;
}

static struct backtrace_state *get_backtrace_state(void) {
    if (!bt_state) {
        bt_state = backtrace_create_state(NULL, 1, bt_error_callback, NULL);
    }
    return bt_state;
}
#endif // MUD_DEBUG

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

void _log_message(log_level level, const char *category, const char *message, const char *file, long line, const char *func) {
    zlog_category_t *c = zlog_get_category(category);
    if (c) {
        int zlevel;
        const char *fmt = "%s";
        switch (level) {
            case LOG_LEVEL_INFO: zlevel = ZLOG_LEVEL_INFO; break;
            case LOG_LEVEL_WARN: zlevel = ZLOG_LEVEL_WARN; break;
            case LOG_LEVEL_ERROR: zlevel = ZLOG_LEVEL_ERROR; break;
            case LOG_LEVEL_DEBUG: zlevel = ZLOG_LEVEL_DEBUG; break;
            case LOG_LEVEL_CRITICAL: zlevel = ZLOG_LEVEL_FATAL; break;
            case LOG_LEVEL_BUG: zlevel = ZLOG_LEVEL_FATAL; fmt = "BUG: %s"; break;
            default: zlevel = ZLOG_LEVEL_ERROR; break;
        }
        zlog(c, file, strlen(file), func, strlen(func), line, zlevel, fmt, message);
    }
}

void _log_message_f(log_level level, const char *category, const char *file, long line, const char *func, const char *format, ...) {
    zlog_category_t *c = zlog_get_category(category);
    if (c) {
        int zlevel;
        switch (level) {
            case LOG_LEVEL_INFO: zlevel = ZLOG_LEVEL_INFO; break;
            case LOG_LEVEL_WARN: zlevel = ZLOG_LEVEL_WARN; break;
            case LOG_LEVEL_ERROR: zlevel = ZLOG_LEVEL_ERROR; break;
            case LOG_LEVEL_DEBUG: zlevel = ZLOG_LEVEL_DEBUG; break;
            case LOG_LEVEL_CRITICAL: zlevel = ZLOG_LEVEL_FATAL; break;
            case LOG_LEVEL_BUG: zlevel = ZLOG_LEVEL_FATAL; break;
            default: zlevel = ZLOG_LEVEL_ERROR; break;
        }
        va_list args;
        va_start(args, format);
        vzlog(c, file, strlen(file), func, strlen(func), line, zlevel, format, args);
        va_end(args);
    }
}

char *log_get_stacktrace(int skip_frames) {
#ifndef MUD_DEBUG
    // Stack traces only available in debug builds
    (void)skip_frames;
    return NULL;
#else
    struct backtrace_state *state = get_backtrace_state();
    if (!state) {
        return NULL;
    }

    // Initialize accumulator
    stacktrace_data sd = {
        .buffer = malloc(1024),
        .size = 1024,
        .used = 0,
        .frame_num = 0,
        .skip_frames = skip_frames + 1 // +1 to skip this function
    };

    if (!sd.buffer) {
        return NULL;
    }

    // Write header (we'll update frame count later)
    sd.used = sprintf(sd.buffer, "Stack trace:\n");

    // Capture stack trace
    backtrace_full(state, 0, bt_full_callback, bt_error_callback, &sd);

    if (sd.frame_num == 0) {
        free(sd.buffer);
        return NULL;
    }

    return sd.buffer;
#endif
}

void _log_stacktrace(log_level level, const char *category, const char *message, const char *file, long line, const char *func) {
    char *trace = log_get_stacktrace(1); // Skip this function

    if (trace) {
        // Log the message with stack trace appended
        size_t msg_len = strlen(message);
        size_t trace_len = strlen(trace);
        char *combined = malloc(msg_len + trace_len + 2);

        if (combined) {
            sprintf(combined, "%s\n%s", message, trace);
            _log_message(level, category, combined, file, line, func);
            free(combined);
        } else {
            // Fallback: log message and trace separately
            _log_message(level, category, message, file, line, func);
            _log_message(level, category, trace, file, line, func);
        }
        free(trace);
    } else {
        // No stack trace available, just log the message
        _log_message(level, category, message, file, line, func);
    }
}

void _log_stacktrace_f(log_level level, const char *category, const char *file, long line, const char *func, const char *format, ...) {
    // Format the message first
    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);
    int msg_size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (msg_size < 0) {
        va_end(args);
        return;
    }

    char *message = malloc(msg_size + 1);
    if (!message) {
        va_end(args);
        return;
    }

    vsnprintf(message, msg_size + 1, format, args);
    va_end(args);

    // Now log with stack trace
    _log_stacktrace(level, category, message, file, line, func);
    free(message);
}

// Crash handler implementation
static char crash_core_dir[512] = {0};
static volatile sig_atomic_t crash_handler_active = 0;

static const char *signal_name(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV (Segmentation fault)";
        case SIGABRT: return "SIGABRT (Aborted)";
        case SIGBUS:  return "SIGBUS (Bus error)";
        case SIGFPE:  return "SIGFPE (Floating point exception)";
        case SIGILL:  return "SIGILL (Illegal instruction)";
        default:      return "Unknown signal";
    }
}

#ifdef MUD_DEBUG
// Signal-safe callback for crash handler - writes directly to fd
static int crash_bt_callback(void *data, uintptr_t pc, const char *filename,
                             int lineno, const char *function) {
    int fd = *(int *)data;
    char buf[512];
    int len;
    (void)pc;

    if (filename && function) {
        len = snprintf(buf, sizeof(buf), "  %s at %s:%d\n", function, filename, lineno);
    } else if (function) {
        len = snprintf(buf, sizeof(buf), "  %s\n", function);
    } else if (filename) {
        len = snprintf(buf, sizeof(buf), "  <unknown> at %s:%d\n", filename, lineno);
    } else {
        len = snprintf(buf, sizeof(buf), "  <unknown>\n");
    }

    if (len > 0) {
        (void)write(fd, buf, (size_t)len);
    }

    return 0;
}

static void crash_bt_error(void *data, const char *msg, int errnum) {
    int fd = *(int *)data;
    (void)errnum;
    const char *prefix = "  [backtrace error: ";
    (void)write(fd, prefix, strlen(prefix));
    (void)write(fd, msg, strlen(msg));
    (void)write(fd, "]\n", 2);
}

// Check if process is being traced (running under debugger)
// Async-signal-safe: uses only open/read/close
static bool is_being_traced(void) {
    int fd = open("/proc/self/status", O_RDONLY);
    if (fd < 0) return false;

    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) return false;
    buf[n] = '\0';

    // Look for "TracerPid:\t<pid>" - non-zero means being traced
    const char *tracer = strstr(buf, "TracerPid:");
    if (!tracer) return false;

    tracer += 10;  // Skip "TracerPid:"
    while (*tracer == ' ' || *tracer == '\t') tracer++;

    // If TracerPid is non-zero, we're being traced
    return (*tracer != '0' || (tracer[1] >= '0' && tracer[1] <= '9'));
}
#endif

static void crash_signal_handler(int sig, siginfo_t *info, void *context) {
    (void)info;
    (void)context;

    // Prevent recursive crashes
    if (crash_handler_active) {
        _exit(128 + sig);
    }
    crash_handler_active = 1;

    // Write crash header to stderr (async-signal-safe)
    const char *header = "\n=== CRASH DETECTED ===\nSignal: ";
    (void)write(STDERR_FILENO, header, strlen(header));
    const char *signame = signal_name(sig);
    (void)write(STDERR_FILENO, signame, strlen(signame));
    (void)write(STDERR_FILENO, "\n", 1);

#ifdef MUD_DEBUG
    // Try to write stack trace to stderr
    struct backtrace_state *state = get_backtrace_state();
    if (state) {
        const char *trace_header = "Stack trace:\n";
        (void)write(STDERR_FILENO, trace_header, strlen(trace_header));
        int fd = STDERR_FILENO;
        backtrace_full(state, 0, crash_bt_callback, crash_bt_error, &fd);
    }

    // Write crash dump file (fallback when core dumps don't work)
    if (state) {
        char dump_path[600];
        char core_path[600];
        char timestamp[32];
        const char *dump_dir = ".";  // Default to current directory
        pid_t pid = getpid();
        int dump_fd = -1;

        // Generate human-readable timestamp: YYYY-MM-DD_HH-MM-SS
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", tm_info);

        // Try configured directory first, fall back to current directory
        if (crash_core_dir[0] != '\0') {
            snprintf(dump_path, sizeof(dump_path), "%s/crash_%s.txt", crash_core_dir, timestamp);
            dump_fd = open(dump_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (dump_fd >= 0) {
                dump_dir = crash_core_dir;
            } else {
                const char *fallback_msg = "Warning: Cannot write to configured dump dir, using current directory\n";
                (void)write(STDERR_FILENO, fallback_msg, strlen(fallback_msg));
            }
        }

        // Fall back to current directory if configured dir failed or wasn't set
        if (dump_fd < 0) {
            snprintf(dump_path, sizeof(dump_path), "./crash_%s.txt", timestamp);
            dump_fd = open(dump_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            dump_dir = ".";
        }

        // Write text stack trace
        if (dump_fd >= 0) {
            char dump_header[256];
            snprintf(dump_header, sizeof(dump_header),
                "=== CRASH DUMP ===\nTime: %s\nPID: %d\nSignal: ", timestamp, (int)pid);
            (void)write(dump_fd, dump_header, strlen(dump_header));
            (void)write(dump_fd, signame, strlen(signame));
            (void)write(dump_fd, "\n\nStack trace:\n", 15);
            backtrace_full(state, 0, crash_bt_callback, crash_bt_error, &dump_fd);
            close(dump_fd);

            const char *dump_msg = "Crash dump written to: ";
            (void)write(STDERR_FILENO, dump_msg, strlen(dump_msg));
            (void)write(STDERR_FILENO, dump_path, strlen(dump_path));
            (void)write(STDERR_FILENO, "\n", 1);
        }

        // Try to generate a GDB-loadable core file using gcore
        // Skip if running under debugger (gcore can't attach while being traced)
        if (is_being_traced()) {
            const char *debug_msg = "Skipping gcore (running under debugger)\n";
            (void)write(STDERR_FILENO, debug_msg, strlen(debug_msg));
        } else {
            // gcore -o specifies output prefix, it appends .PID automatically
            snprintf(core_path, sizeof(core_path), "%s/core_%s.%d", dump_dir, timestamp, (int)pid);
            char gcore_cmd[700];
            snprintf(gcore_cmd, sizeof(gcore_cmd), "gcore -o %s/core_%s %d 2>&1", dump_dir, timestamp, (int)pid);

            // fork+exec is safer in signal handler than system()
            pid_t child = fork();
            if (child == 0) {
                // Child process - run gcore
                execl("/bin/sh", "sh", "-c", gcore_cmd, (char *)NULL);
                _exit(1);
            } else if (child > 0) {
                // Parent - wait for gcore to complete
                int status;
                waitpid(child, &status, 0);
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    const char *core_msg = "GDB core file written to: ";
                    (void)write(STDERR_FILENO, core_msg, strlen(core_msg));
                    (void)write(STDERR_FILENO, core_path, strlen(core_path));
                    (void)write(STDERR_FILENO, "\n", 1);
                } else {
                    const char *core_err = "Warning: gcore failed (may not be installed or lacks permissions)\n";
                    (void)write(STDERR_FILENO, core_err, strlen(core_err));
                }
            }
        }
    }
#else
    const char *no_trace = "(Stack trace not available in release build)\n";
    (void)write(STDERR_FILENO, no_trace, strlen(no_trace));
#endif

    // Also try to log to file (may not be async-signal-safe, but worth trying)
    char crash_msg[256];
    snprintf(crash_msg, sizeof(crash_msg), "FATAL CRASH: %s", signal_name(sig));
    log_stacktrace(LOG_LEVEL_CRITICAL, LOG_CRITICAL, crash_msg);

    // Flush log buffers
    log_shutdown();

    // Change to core dump directory if specified
    if (crash_core_dir[0] != '\0') {
        if (chdir(crash_core_dir) != 0) {
            const char *chdir_err = "Warning: Could not change to core dump directory\n";
            (void)write(STDERR_FILENO, chdir_err, strlen(chdir_err));
        }
    }

    const char *footer = "=== Generating core dump ===\n";
    (void)write(STDERR_FILENO, footer, strlen(footer));

    // Reset signal handler to default and re-raise to generate core dump
    signal(sig, SIG_DFL);
    raise(sig);
}

// Check if core dumps will work (returns 1 if likely to work, 0 if not)
static int check_core_pattern(void) {
    FILE *f = fopen("/proc/sys/kernel/core_pattern", "r");
    if (!f) {
        return 0; // Can't check, assume it might work
    }

    char pattern[256];
    if (fgets(pattern, sizeof(pattern), f) == NULL) {
        fclose(f);
        return 0;
    }
    fclose(f);

    // If pattern starts with |, it pipes to a program
    // In containers, this often points to host systemd which won't work
    if (pattern[0] == '|') {
        return 0; // Piped to external program, may not work in container
    }

    return 1; // Pattern looks like a file path, should work
}

void log_install_crash_handler(const char *core_dir) {
    // Store core directory
    if (core_dir && core_dir[0] != '\0') {
        strncpy(crash_core_dir, core_dir, sizeof(crash_core_dir) - 1);
        crash_core_dir[sizeof(crash_core_dir) - 1] = '\0';
    }

    // Enable core dumps
    struct rlimit rl;
    rl.rlim_cur = RLIM_INFINITY;
    rl.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &rl) != 0) {
        perror("Warning: Could not set core dump size limit");
    }

    // Ensure process is dumpable (important if setuid or similar)
    if (prctl(PR_SET_DUMPABLE, 1, 0, 0, 0) != 0) {
        perror("Warning: Could not set process dumpable");
    }

    // Check if core dumps will actually work
    int core_dumps_work = check_core_pattern();

    // Install signal handlers
    struct sigaction sa;
    sa.sa_sigaction = crash_signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND; // SA_RESETHAND prevents recursive handler
    sigemptyset(&sa.sa_mask);

    // Add all crash signals to mask (block during handler)
    sigaddset(&sa.sa_mask, SIGSEGV);
    sigaddset(&sa.sa_mask, SIGABRT);
    sigaddset(&sa.sa_mask, SIGBUS);
    sigaddset(&sa.sa_mask, SIGFPE);
    sigaddset(&sa.sa_mask, SIGILL);

    // Install handlers
    if (sigaction(SIGSEGV, &sa, NULL) != 0) perror("sigaction SIGSEGV");
    if (sigaction(SIGABRT, &sa, NULL) != 0) perror("sigaction SIGABRT");
    if (sigaction(SIGBUS, &sa, NULL) != 0) perror("sigaction SIGBUS");
    if (sigaction(SIGFPE, &sa, NULL) != 0) perror("sigaction SIGFPE");
    if (sigaction(SIGILL, &sa, NULL) != 0) perror("sigaction SIGILL");

    // Log that crash handler is installed
    if (core_dir && core_dir[0] != '\0') {
        log_message_f(LOG_LEVEL_INFO, LOG_INIT,
            "Crash handler installed (core dumps to: %s)", core_dir);
    } else {
        log_message(LOG_LEVEL_INFO, LOG_INIT,
            "Crash handler installed (core dumps to current directory)");
    }

    if (!core_dumps_work) {
        log_message(LOG_LEVEL_WARN, LOG_INIT,
            "Core dumps may not work: /proc/sys/kernel/core_pattern pipes to external program. "
            "Stack traces will still be logged on crash.");
    }
}

