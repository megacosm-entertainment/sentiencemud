#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>

#include "niblang.h"

#define BASE_BUF 	1024
/* valid states */
#define BUFFER_SAFE	0
#define BUFFER_OVERFLOW	1
#define BUFFER_FREED 	2

static const int mem_buf_size[] =
{
    16,32,64,128,256,1024,2048,4096,8192,16384,32768,65536,131072,262144,524288,1048576,-1
};


/* local procedure for finding the next acceptable size */
/* -1 indicates out-of-boundary error */
static int mem_get_size( int val , int target)
{
	int i;

	for (i = 0; mem_buf_size[i] > 0; i++)
		if (mem_buf_size[i] >= val)
		{
			return mem_buf_size[i];
		}

	return -1;
}

NIB_BUFFER *new_mem_buffer_size(int size)
{
	NIB_BUFFER *buffer = calloc(1,sizeof(*buffer));

	buffer->state	= BUFFER_SAFE;
	buffer->size	= mem_get_size(size,0);
	buffer->len		= 0;
	buffer->buffer	= malloc(buffer->size);

	return buffer;
}

NIB_BUFFER *new_mem_buffer()
{
	return new_mem_buffer_size(BASE_BUF);
}

void free_mem_buffer(NIB_BUFFER *buffer)
{
	if (buffer)
	{
		free(buffer->buffer);
		free(buffer);
	}
}

bool mem_buffer_append_byte(NIB_BUFFER *buffer, char ch)
{
    char tmp[2];
    tmp[0] = ch;
    tmp[1] = '\0';
    return mem_buffer_append(buffer, tmp, 1);
}

bool mem_buffer_append_short(NIB_BUFFER *buffer, short data)
{
	char tmp[sizeof(data)];

	for(int i = 0; i < sizeof(data); i++, data >>= 8)
		tmp[i] = (char)(data & 0xFF);
	return mem_buffer_append(buffer, tmp, sizeof(data));
}

bool mem_buffer_append_int(NIB_BUFFER *buffer, int data)
{
	char tmp[sizeof(data)];

	for(int i = 0; i < sizeof(data); i++, data >>= 8)
		tmp[i] = (char)(data & 0xFF);
	return mem_buffer_append(buffer, tmp, sizeof(data));
}

bool mem_buffer_append_long(NIB_BUFFER *buffer, long data)
{
	char tmp[sizeof(data)];

	for(int i = 0; i < sizeof(data); i++, data >>= 8)
		tmp[i] = (char)(data & 0xFF);
	return mem_buffer_append(buffer, tmp, sizeof(data));
}

bool mem_buffer_append(NIB_BUFFER *buffer, char *data, int len)
{
    int size;
    char *oldbuffer;
    int oldsize;

    oldbuffer = buffer->buffer;
    oldsize = buffer->size;

    if (buffer->state == BUFFER_OVERFLOW) /* don't waste time on bad strings! */
		return false;

    size = buffer->len + len;

    while (size > buffer->size) /* increase the buffer size */
    {
		buffer->size 	= mem_get_size(buffer->size + len, size);
		if (buffer->size == -1) /* overflow */
		{
			buffer->size = oldsize;
			buffer->state = BUFFER_OVERFLOW;
//			bug("buffer overflow past size %d",buffer->size);
			return false;
		}
    }

    if (buffer->size != oldsize)
    {
		buffer->buffer = malloc(buffer->size);
		memcpy(buffer->buffer,oldbuffer,oldsize);
		free(oldbuffer);
    }

	memcpy(buffer->buffer + buffer->len, data, len);
	buffer->len += len;
    return true;
}


void mem_buffer_clear(NIB_BUFFER *buffer)
{
    buffer->len = 0;
    buffer->state = BUFFER_SAFE;
}


char *mem_buffer_get(NIB_BUFFER *buffer)
{
    return buffer->buffer;
}
