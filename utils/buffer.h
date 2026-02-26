#ifndef __UTILS_BUFFER_H__
#define __UTILS_BUFFER_H__

#include "../merc.h"

#define MAX_BUF         16384
#define MAX_BUF_LIST    16
#define BASE_BUF        1024
#define MAX_BUF_TOTAL   (1024 * 1024)

#define BUFFER_SAFE     0
#define BUFFER_OVERFLOW 1
#define BUFFER_FREED    2

BUFFER *new_buf(void);
BUFFER *new_buf_size(int size);
void free_buf(BUFFER *buffer);
bool add_buf_char(BUFFER *buffer, char ch);
bool add_buf(BUFFER *buffer, const char *string);
bool bprintf(BUFFER *buffer, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void clear_buf(BUFFER *buffer);
char *buf_string(BUFFER *buffer);
size_t buf_len(BUFFER *buffer);
size_t buf_capacity(BUFFER *buffer);
size_t buf_remaining(BUFFER *buffer);

#endif
