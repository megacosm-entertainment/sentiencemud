/*-
 * Copyright 2005 Colin Percival
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libmd/sha256.h,v 1.1 2005/03/09 19:23:04 cperciva Exp $
 */

/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#ifndef _sent_SHA256_H_
#define _sent_SHA256_H_

#include <sys/types.h>


typedef struct SHA256Context
{
   int state[8];
   int count[2];
   unsigned char buf[64];
} sent_SHA256_CTX;

void sent_SHA256_Init( sent_SHA256_CTX * );
void sent_SHA256_Update( sent_SHA256_CTX *, const unsigned char *, size_t );
void sent_SHA256_Final( unsigned char [32], sent_SHA256_CTX * );
char *sent_SHA256_End( sent_SHA256_CTX *, char * );
char *sent_SHA256_File( const char *, char * );
char *sent_SHA256_FileChunk( const char *, char *, off_t, off_t );
char *sent_SHA256_Data( const unsigned char *, unsigned int, char * );
char *sent_SHA256_crypt( const char *pwd );

#endif /* !_sent_SHA256_H_ */
