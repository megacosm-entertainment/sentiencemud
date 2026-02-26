#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "buffer.h"

static BUFFER *buf_free;
static char null_buffer_string[1] = "";

static const int buf_size[MAX_BUF_LIST] = {
    16, 32, 64, 128, 256, 1024, 2048, 4096,
    8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576
};

_Static_assert((sizeof(buf_size) / sizeof(buf_size[0])) == MAX_BUF_LIST,
               "buf_size table length must match MAX_BUF_LIST");
_Static_assert(1048576 == MAX_BUF_TOTAL,
               "last buf_size tier must match MAX_BUF_TOTAL");

static int get_size(int required)
{
    int i;

    if (required > MAX_BUF_TOTAL)
        return -1;

    for (i = 0; i < MAX_BUF_LIST; i++) {
        if (buf_size[i] >= required)
            return buf_size[i];
    }

    return -1;
}

static bool grow_buf(BUFFER *buffer, size_t required)
{
    int new_size;
    char *new_str;

    if (required <= (size_t)buffer->size)
        return true;

    if (required > (size_t)INT_MAX) {
        buffer->state = BUFFER_OVERFLOW;
        pbugf(LOG_ERROR, "buffer required size exceeds INT_MAX");
        return false;
    }

    new_size = get_size((int)required);
    if (new_size == -1) {
        buffer->state = BUFFER_OVERFLOW;
        pbugf(LOG_ERROR, "buffer overflow past size %d", buffer->size);
        return false;
    }

    new_str = realloc(buffer->string, (size_t)new_size);
    if (new_str == NULL) {
        buffer->state = BUFFER_OVERFLOW;
        pbugf(LOG_ERROR, "buffer realloc failed to size %d", new_size);
        return false;
    }

    buffer->string = new_str;
    buffer->size = new_size;
    return true;
}

BUFFER *new_buf(void)
{
    return new_buf_size(BASE_BUF);
}

BUFFER *new_buf_size(int size)
{
    BUFFER *buffer;
    int requested = size < BASE_BUF ? BASE_BUF : size;

    if (buf_free == NULL)
        buffer = alloc_perm(sizeof(*buffer));
    else {
        buffer = buf_free;
        buf_free = buf_free->next;
    }

    buffer->next = NULL;
    buffer->state = BUFFER_SAFE;
    buffer->size = get_size(requested);
    if (buffer->size == -1) {
        pbugf(LOG_ERROR, "new_buf: buffer size %d too large.", size);
        exit(1);
    }

    buffer->string = malloc((size_t)buffer->size);
    if (buffer->string == NULL) {
        pbugf(LOG_ERROR, "new_buf_size: malloc failed for size %d", buffer->size);
        exit(1);
    }

    buffer->len = 0;
    buffer->string[0] = '\0';
    VALIDATE(buffer);

    return buffer;
}

void free_buf(BUFFER *buffer)
{
    if (!IS_VALID(buffer))
        return;

    free(buffer->string);
    buffer->string = NULL;
    buffer->size = 0;
    buffer->len = 0;
    buffer->state = BUFFER_FREED;
    INVALIDATE(buffer);

    buffer->next = buf_free;
    buf_free = buffer;
}

bool add_buf_char(BUFFER *buffer, char ch)
{
    if (!IS_VALID(buffer) || buffer->string == NULL || buffer->size <= 0) {
        pbugf(LOG_ERROR, "add_buf_char: invalid buffer");
        return false;
    }

    if (buffer->state == BUFFER_OVERFLOW)
        return false;

    if (buffer->len > SIZE_MAX - 2) {
        buffer->state = BUFFER_OVERFLOW;
        pbugf(LOG_ERROR, "add_buf_char: size_t overflow");
        return false;
    }

    if (!grow_buf(buffer, buffer->len + 2))
        return false;

    buffer->string[buffer->len++] = ch;
    buffer->string[buffer->len] = '\0';
    return true;
}

bool add_buf(BUFFER *buffer, const char *string)
{
    size_t add_len;
    size_t required;

    if (!IS_VALID(buffer) || buffer->string == NULL || buffer->size <= 0) {
        pbugf(LOG_ERROR, "add_buf: invalid buffer");
        return false;
    }

    if (buffer->state == BUFFER_OVERFLOW)
        return false;

    if (string == NULL)
        return true;

    add_len = strlen(string);
    if (add_len == 0)
        return true;

    if (add_len > SIZE_MAX - buffer->len - 1) {
        buffer->state = BUFFER_OVERFLOW;
        pbugf(LOG_ERROR, "add_buf: size_t overflow");
        return false;
    }

    required = buffer->len + add_len + 1;

    if (!grow_buf(buffer, required))
        return false;

    memcpy(buffer->string + buffer->len, string, add_len + 1);
    buffer->len += add_len;
    return true;
}

bool bprintf(BUFFER *buffer, const char *fmt, ...)
{
    va_list args;
    va_list args_copy;
    int needed;
    int written;

    if (!IS_VALID(buffer) || buffer->string == NULL || buffer->size <= 0) {
        pbugf(LOG_ERROR, "bprintf: invalid buffer");
        return false;
    }

    if (fmt == NULL) {
        pbugf(LOG_ERROR, "bprintf: NULL format string");
        return false;
    }

    if (buffer->state == BUFFER_OVERFLOW)
        return false;

    va_start(args, fmt);
    va_copy(args_copy, args);
    needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return false;
    }

    if ((size_t)needed > SIZE_MAX - buffer->len - 1) {
        va_end(args);
        buffer->state = BUFFER_OVERFLOW;
        pbugf(LOG_ERROR, "bprintf: size_t overflow");
        return false;
    }

    if (!grow_buf(buffer, buffer->len + (size_t)needed + 1)) {
        va_end(args);
        return false;
    }

    written = vsnprintf(buffer->string + buffer->len, (size_t)needed + 1, fmt, args);
    va_end(args);

    if (written < 0)
        return false;

    buffer->len += (size_t)written;
    return true;
}

void clear_buf(BUFFER *buffer)
{
    if (!IS_VALID(buffer) || buffer->string == NULL || buffer->size <= 0)
        return;

    buffer->len = 0;
    buffer->string[0] = '\0';
    buffer->state = BUFFER_SAFE;
}

char *buf_string(BUFFER *buffer)
{
    if (!IS_VALID(buffer) || buffer->string == NULL)
        return null_buffer_string;

    return buffer->string;
}

size_t buf_len(BUFFER *buffer)
{
    if (!IS_VALID(buffer))
        return 0;

    return buffer->len;
}

size_t buf_capacity(BUFFER *buffer)
{
    if (!IS_VALID(buffer) || buffer->size <= 0)
        return 0;

    return (size_t)buffer->size;
}

size_t buf_remaining(BUFFER *buffer)
{
    if (!IS_VALID(buffer) || buffer->size <= 0)
        return 0;

    if ((size_t)buffer->size <= buffer->len)
        return 0;

    return (size_t)buffer->size - buffer->len - 1;
}