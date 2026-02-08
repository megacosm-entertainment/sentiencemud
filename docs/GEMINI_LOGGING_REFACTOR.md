# Gemini Logging Refactor Guide

This document outlines the process of refactoring the MUD's logging system from the legacy `log_string`, `log_stringf`, and `bug` functions to the new `zlog`-based functions `log_message` and `log_message_f`.

## 1. Understanding the Change

The goal is to replace all calls to the old logging functions with their modern, more structured `zlog` counterparts.

**Old Functions (to be removed):**

*   `void log_string(const char *str)`: Logs a simple string.
*   `void log_stringf(const char *fmt, ...)`: Logs a formatted string.
*   `void bug(const char *str, ...)`: Logs a "bug" message, which is a formatted string.

These are located in `src/db.c`.

**New Functions (to be used):**

*   `void log_message(int level, const char *file, const char *func, const char *message)`
*   `void log_message_f(int level, const char *file, const char *func, const char *format, ...)`

These new functions require a log level, and the file and function name where the log is being called from.

**Log Levels:**

*   `LOG_INFO`: General informational messages.
*   `LOG_WARNING`: Warnings about potential problems.
*   `LOG_ERROR`: Errors that affect operation but may not crash the MUD.
*   `LOG_BUG`: Critical errors and bugs.
*   `LOG_DEBUG`: Debug messages.

## 2. The Refactoring Process

The refactoring will be done on a file-by-file basis. For each file, you will:

1.  Open the file.
2.  Search for occurrences of `log_string`, `log_stringf`, and `bug`.
3.  Replace each call with the appropriate new function.
4.  Commit the changes for that file.

This incremental approach makes it easier to track progress and manage changes.

## 3. Replacement Patterns

Here are the patterns to use for replacing the old functions.

### `log_string`

**Pattern:** `log_string(message);`

**Replacement:** `log_message(LOG_LEVEL, __FILE__, __func__, message);`

*   Replace `LOG_LEVEL` with the appropriate level (`LOG_INFO`, `LOG_WARNING`, `LOG_ERROR`).
*   `__FILE__` and `__func__` are preprocessor macros that automatically provide the current file and function name.

**Example:**

In `act_wiz.c`, the line:

```c
log_string("Loading configuration settings from gconfig.rc...");
```

Becomes:

```c
log_message(LOG_INFO, __FILE__, __func__, "Loading configuration settings from gconfig.rc...");
```

### `log_stringf`

**Pattern:** `log_stringf(format, ...);`

**Replacement:** `log_message_f(LOG_LEVEL, __FILE__, __func__, format, ...);`

**Example:**

In `comm.c`, the line:

```c
log_stringf("Closing stalled %s handshake connection", d->host);
```

Becomes:

```c
log_message_f(LOG_INFO, __FILE__, __func__, "Closing stalled %s handshake connection", d->host);
```

### `bug`

**Pattern:** `bug(format, ...);`

**Replacement:** `log_message_f(LOG_BUG, __FILE__, __func__, format, ...);`

**Example:**

If you were to find a line like `bug("Something went wrong with %s", name);`, it would become:

```c
log_message_f(LOG_BUG, __FILE__, __func__, "Something went wrong with %s", name);
```

## 4. Final Step: Remove Old Functions

After all calls to `log_string`, `log_stringf`, and `bug` have been replaced throughout the entire `src` directory, you must delete their original definitions from `src/db.c`.

This is the final step to complete the refactor.

---

This document should serve as a clear guide for you to perform the refactoring.
