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
	NIB_BUFFER *buffer = nib_calloc(1,sizeof(*buffer));

	buffer->state	= BUFFER_SAFE;
	buffer->size	= mem_get_size(size,0);
	buffer->len		= 0;
	buffer->buffer	= (nib_bytecode_p)malloc(buffer->size);

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
		nib_free(buffer->buffer);
		nib_free(buffer);
	}
}

bool mem_buffer_append_byte(NIB_BUFFER *buffer, nib_bytecode_t ch)
{
	return mem_buffer_append(buffer, (nib_bytecode_p)&ch, sizeof(ch));
}

bool mem_buffer_append_short(NIB_BUFFER *buffer, short data)
{
	return mem_buffer_append(buffer, (nib_bytecode_p)&data, sizeof(data));
}

bool mem_buffer_append_int(NIB_BUFFER *buffer, int data)
{
	return mem_buffer_append(buffer, (nib_bytecode_p)&data, sizeof(data));
}

bool mem_buffer_append_long(NIB_BUFFER *buffer, long data)
{
	return mem_buffer_append(buffer, (nib_bytecode_p)&data, sizeof(data));
}

bool mem_buffer_append_float(NIB_BUFFER *buffer, double data)
{
	return mem_buffer_append(buffer, (nib_bytecode_p)&data, sizeof(data));
}

bool mem_buffer_append_pointer(NIB_BUFFER *buffer, void *data)
{
	return mem_buffer_append(buffer, (nib_bytecode_p)&data, sizeof(data));
}

bool mem_buffer_update_short(NIB_BUFFER *buffer, int offset, short data)
{
	if (offset + sizeof(data) > buffer->len) return false;

	memcpy(buffer->buffer + offset, (nib_bytecode_p)&data, sizeof(data));

	return true;
}

bool mem_buffer_update_int(NIB_BUFFER *buffer, int offset, int data)
{
	if (offset + sizeof(data) > buffer->len) return false;

	memcpy(buffer->buffer + offset, (nib_bytecode_p)&data, sizeof(data));

	return true;
}

bool mem_buffer_update_long(NIB_BUFFER *buffer, int offset, long data)
{
	if (offset + sizeof(data) > buffer->len) return false;

	memcpy(buffer->buffer + offset, (nib_bytecode_p)&data, sizeof(data));

	return true;
}

bool mem_buffer_update_float(NIB_BUFFER *buffer, int offset, double data)
{
	if (offset + sizeof(data) > buffer->len) return false;

	memcpy(buffer->buffer + offset, (nib_bytecode_p)&data, sizeof(data));

	return true;
}

bool mem_buffer_update_pointer(NIB_BUFFER *buffer, int offset, void *data)
{
	if (offset + sizeof(data) > buffer->len) return false;

	memcpy(buffer->buffer + offset, (nib_bytecode_p)&data, sizeof(data));

	return true;
}

bool mem_buffer_resize(NIB_BUFFER *buffer, int len, int new_size)
{
    nib_bytecode_p oldbuffer;
    int oldsize;

    oldbuffer = buffer->buffer;
    oldsize = buffer->size;

    while (new_size > buffer->size) /* increase the buffer size */
    {
		buffer->size 	= mem_get_size(buffer->size + len, new_size);
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
		buffer->buffer = (nib_bytecode_p)malloc(buffer->size);
		memcpy(buffer->buffer,oldbuffer,oldsize);
		nib_free(oldbuffer);
    }

	return true;
}

bool mem_buffer_append(NIB_BUFFER *buffer, nib_bytecode_p data, int len)
{
    if (buffer->state == BUFFER_OVERFLOW) /* don't waste time on bad strings! */
		return false;

	if (!mem_buffer_resize(buffer, len, buffer->len + len))
		return false;

	memcpy(buffer->buffer + buffer->len, data, len);
	buffer->len += len;
    return true;
}

bool mem_buffer_extend(NIB_BUFFER *buffer, int offset, int len)
{
	int oldsize = buffer->len;
    if (buffer->state == BUFFER_OVERFLOW) /* don't waste time on bad strings! */
		return false;

	if (!mem_buffer_resize(buffer, len, buffer->len + len))
		return false;

	memmove(buffer->buffer + offset, buffer->buffer + offset + len, oldsize - offset);
	for(int i = 0; i < len; i++)
		buffer->buffer[offset + i] = '\0';	// Clear it out

	buffer->len += len;
	return true;
}

bool mem_buffer_prune(NIB_BUFFER *buffer, int offset, int len)
{
	int oldsize = buffer->len;
    if (buffer->state == BUFFER_OVERFLOW) /* don't waste time on bad strings! */
		return false;

	memmove(buffer->buffer + offset + len, buffer->buffer + offset, oldsize - (offset + len));
	buffer->len -= len;
	return true;
}

void mem_buffer_clear(NIB_BUFFER *buffer)
{
    buffer->len = 0;
    buffer->state = BUFFER_SAFE;
}


nib_bytecode_p mem_buffer_get(NIB_BUFFER *buffer)
{
    return buffer->buffer;
}
