/*
 * Copyright (C) 2006-2009 B.A.T.M.A.N. contributors:
 *
 * Thomas Lopatic, Corinna 'Elektra' Aichele, Axel Neumann, Marek Lindner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os.h"
#include "allocate.h"


#define MAGIC_NUMBER 0x12345678

#if defined DEBUG_MALLOC

static pthread_mutex_t chunk_mutex = PTHREAD_MUTEX_INITIALIZER;

struct chunkHeader *chunkList = NULL;

struct chunkHeader
{
	struct chunkHeader *next;
	uint32_t length;
	int32_t tag;
	uint32_t magicNumber;
};

struct chunkTrailer
{
	uint32_t magicNumber;
};



#if defined MEMORY_USAGE

struct memoryUsage *memoryList = NULL;
static pthread_mutex_t memory_mutex = PTHREAD_MUTEX_INITIALIZER;


struct memoryUsage
{
	struct memoryUsage *next;
	uint32_t length;
	uint32_t counter;
	int32_t tag;
};


static size_t getHeaderPad() {
	size_t alignwith, pad;

	if (sizeof(TYPE_OF_WORD) > sizeof(void*))
		alignwith = sizeof(TYPE_OF_WORD);
	else
		alignwith = sizeof(void*);

	pad = alignwith - (sizeof(struct chunkHeader) % alignwith);

	if (pad == alignwith)
		return 0;
	else
		return pad;
}

static size_t getTrailerPad(size_t length) {
	size_t alignwith, pad;

	if (sizeof(TYPE_OF_WORD) > sizeof(void*))
		alignwith = sizeof(TYPE_OF_WORD);
	else
		alignwith = sizeof(void*);

	pad = alignwith - (length % alignwith);

	if (pad == alignwith)
		return 0;
	else
		return pad;
}

static void fillPadding(unsigned char* padding, size_t length) {
	unsigned char c = 0x00;
	size_t i;

	for (i = 0; i < length; i++) {
		c += 0xA7;
		padding[i] = c;
	}
}

static int checkPadding(unsigned char* padding, size_t length) {
	unsigned char c = 0x00;
	size_t i;

	for (i = 0; i < length; i++) {
		c += 0xA7;
		if (padding[i] != c)
			return 0;
	}
	return 1;
}

static void addMemory( uint32_t length, int32_t tag ) {

	struct memoryUsage *walker;

	pthread_mutex_lock(&memory_mutex);
	for ( walker = memoryList; walker != NULL; walker = walker->next ) {

		if ( walker->tag == tag ) {

			walker->counter++;
			break;

		}

	}

	if ( walker == NULL ) {

		walker = malloc( sizeof(struct memoryUsage) );

		walker->length = length;
		walker->tag = tag;
		walker->counter = 1;

		walker->next = memoryList;
		memoryList = walker;

	}

	pthread_mutex_unlock(&memory_mutex);
}


static void removeMemory( int32_t tag, int32_t freetag ) {

	struct memoryUsage *walker;

	pthread_mutex_lock(&memory_mutex);
	for ( walker = memoryList; walker != NULL; walker = walker->next ) {

		if ( walker->tag == tag ) {

			if ( walker->counter == 0 ) {

				debug_output( 0, "Freeing more memory than was allocated: malloc tag = %d, free tag = %d\n", tag, freetag );
				pthread_mutex_unlock(&memory_mutex);
				restore_and_exit(0);

			}

			walker->counter--;
			break;

		}

	}

	if ( walker == NULL ) {

		debug_output( 0, "Freeing memory that was never allocated: malloc tag = %d, free tag = %d\n", tag, freetag );
		pthread_mutex_unlock(&memory_mutex);
		restore_and_exit(0);

	}

	pthread_mutex_unlock(&memory_mutex);
}

#endif



void checkIntegrity(void)
{
	struct chunkHeader *walker;
	struct chunkTrailer *chunkTrailer;
	unsigned char *memory;


#if defined MEMORY_USAGE

	struct memoryUsage *memoryWalker;

	debug_output( 5, " \nMemory usage information:\n" );

	pthread_mutex_lock(&memory_mutex);
	for ( memoryWalker = memoryList; memoryWalker != NULL; memoryWalker = memoryWalker->next ) {

		if ( memoryWalker->counter != 0 )
			debug_output( 5, "   tag: %''4i, num malloc: %4i, bytes per malloc: %''4i, total: %6i\n", memoryWalker->tag, memoryWalker->counter, memoryWalker->length, memoryWalker->counter * memoryWalker->length );

	}
	pthread_mutex_unlock(&memory_mutex);

#endif

	pthread_mutex_lock(&chunk_mutex);

	for (walker = chunkList; walker != NULL; walker = walker->next)
	{
		if (walker->magicNumber != MAGIC_NUMBER)
		{
			debug_output( 0, "checkIntegrity - invalid magic number in header: %08x, malloc tag = %d\n", walker->magicNumber, walker->tag );
			pthread_mutex_unlock(&chunk_mutex);
			restore_and_exit(0);
		}

		memory = (unsigned char *)walker;

		chunkTrailer = (struct chunkTrailer *)(memory + sizeof(struct chunkHeader) + getHeaderPad() + walker->length + getTrailerPad(walker->length));

		if (chunkTrailer->magicNumber != MAGIC_NUMBER)
		{
			debug_output( 0, "checkIntegrity - invalid magic number in trailer: %08x, malloc tag = %d\n", chunkTrailer->magicNumber, walker->tag );
			pthread_mutex_unlock(&chunk_mutex);
			restore_and_exit(0);
		}
	}

	pthread_mutex_unlock(&chunk_mutex);
}

void checkLeak(void)
{
	struct chunkHeader *walker;

	pthread_mutex_lock(&chunk_mutex);
	for (walker = chunkList; walker != NULL; walker = walker->next)
		debug_output( 0, "Memory leak detected, malloc tag = %d\n", walker->tag );

	pthread_mutex_unlock(&chunk_mutex);
}

void *debugMalloc(uint32_t length, int32_t tag)
{
	unsigned char *memory;
	struct chunkHeader *chunkHeader;
	struct chunkTrailer *chunkTrailer;
	unsigned char *chunk;

/* 	printf("sizeof(struct chunkHeader) = %u, sizeof (struct chunkTrailer) = %u\n", sizeof (struct chunkHeader), sizeof (struct chunkTrailer)); */

	memory = malloc(length + sizeof(struct chunkHeader) + sizeof(struct chunkTrailer) + getHeaderPad() + getTrailerPad(length));

	if (memory == NULL)
	{
		debug_output( 0, "Cannot allocate %u bytes, malloc tag = %d\n", (unsigned int)(length + sizeof(struct chunkHeader) + sizeof(struct chunkTrailer)), tag );
		restore_and_exit(0);
	}

	chunkHeader = (struct chunkHeader *)memory;
	chunk = memory + sizeof(struct chunkHeader) + getHeaderPad();
	chunkTrailer = (struct chunkTrailer *)(memory + sizeof(struct chunkHeader) + length + getHeaderPad() + getTrailerPad(length));

	fillPadding((unsigned char*)chunkHeader + sizeof(struct chunkHeader), getHeaderPad());
	fillPadding(chunk + length, getTrailerPad(length));

	chunkHeader->length = length;
	chunkHeader->tag = tag;
	chunkHeader->magicNumber = MAGIC_NUMBER;

	chunkTrailer->magicNumber = MAGIC_NUMBER;

	pthread_mutex_lock(&chunk_mutex);
	chunkHeader->next = chunkList;
	chunkList = chunkHeader;
	pthread_mutex_unlock(&chunk_mutex);

#if defined MEMORY_USAGE

	addMemory( length, tag );

#endif

	return chunk;
}

void *debugRealloc(void *memoryParameter, uint32_t length, int32_t tag)
{
	unsigned char *memory;
	struct chunkHeader *chunkHeader=NULL;
	struct chunkTrailer *chunkTrailer;
	unsigned char *result;
	uint32_t copyLength;

	if (memoryParameter) { /* if memoryParameter==NULL, realloc() should work like malloc() !! */
		memory = memoryParameter;
		chunkHeader = (struct chunkHeader *)(memory - sizeof(struct chunkHeader) - getHeaderPad());

		if (chunkHeader->magicNumber != MAGIC_NUMBER)
		{
			debug_output( 0, "debugRealloc - invalid magic number in header: %08x, malloc tag = %d\n", chunkHeader->magicNumber, chunkHeader->tag );
			restore_and_exit(0);
		}

		if (checkPadding(memory - getHeaderPad(), getHeaderPad()) == 0) {
			debug_output( 0, "debugRealloc - invalid magic padding in header, malloc tag = %d\n", chunkHeader->tag );
			restore_and_exit(0);
		}

		chunkTrailer = (struct chunkTrailer *)(memory + chunkHeader->length + getTrailerPad(chunkHeader->length));

		if (chunkTrailer->magicNumber != MAGIC_NUMBER)
		{
			debug_output( 0, "debugRealloc - invalid magic number in trailer: %08x, malloc tag = %d\n", chunkTrailer->magicNumber, chunkHeader->tag );
			restore_and_exit(0);
		}

		if (checkPadding(memory + chunkHeader->length, getTrailerPad(chunkHeader->length)) == 0) {
			debug_output( 0, "debugRealloc - invalid magic padding in trailer, malloc tag = %d\n", chunkHeader->tag );
			restore_and_exit(0);
		}
	}


	result = debugMalloc(length, tag);
	if (memoryParameter) {
		copyLength = length;

		if (copyLength > chunkHeader->length)
			copyLength = chunkHeader->length;

		memcpy(result, memoryParameter, copyLength);
		debugFree(memoryParameter, 9999);
	}

	return result;
}

void debugFree(void *memoryParameter, int tag)
{
	unsigned char *memory;
	struct chunkHeader *chunkHeader;
	struct chunkTrailer *chunkTrailer;
	struct chunkHeader *walker;
	struct chunkHeader *previous;

	memory = memoryParameter;
	chunkHeader = (struct chunkHeader *)(memory - sizeof(struct chunkHeader) - getHeaderPad());

	if (chunkHeader->magicNumber != MAGIC_NUMBER)
	{
		debug_output( 0, "debugFree - invalid magic number in header: %08x, malloc tag = %d, free tag = %d\n", chunkHeader->magicNumber, chunkHeader->tag, tag );
		restore_and_exit(0);
	}

	if (checkPadding(memory - getHeaderPad(), getHeaderPad()) == 0) {
		debug_output( 0, "debugFree - invalid magic padding in header, malloc tag = %d\n", chunkHeader->tag );
		restore_and_exit(0);
	}

	previous = NULL;

	pthread_mutex_lock(&chunk_mutex);
	for (walker = chunkList; walker != NULL; walker = walker->next)
	{
		if (walker == chunkHeader)
			break;

		previous = walker;
	}

	if (walker == NULL)
	{
		debug_output( 0, "Double free detected, malloc tag = %d, free tag = %d\n", chunkHeader->tag, tag );
		pthread_mutex_unlock(&chunk_mutex);
		restore_and_exit(0);
	}

	if (previous == NULL)
		chunkList = walker->next;

	else
		previous->next = walker->next;

	pthread_mutex_unlock(&chunk_mutex);

	chunkTrailer = (struct chunkTrailer *)(memory + chunkHeader->length + getTrailerPad(chunkHeader->length));

	if (chunkTrailer->magicNumber != MAGIC_NUMBER)
	{
		debug_output( 0, "debugFree - invalid magic number in trailer: %08x, malloc tag = %d, free tag = %d\n", chunkTrailer->magicNumber, chunkHeader->tag, tag );
		restore_and_exit(0);
	}

	if (checkPadding(memory + chunkHeader->length, getTrailerPad(chunkHeader->length)) == 0) {
		debug_output( 0, "debugFree - invalid magic padding in trailer, malloc tag = %d\n", chunkHeader->tag );
		restore_and_exit(0);
	}

#if defined MEMORY_USAGE

	removeMemory( chunkHeader->tag, tag );

#endif

	free(chunkHeader);
}

#else

void checkIntegrity(void)
{
}

void checkLeak(void)
{
}

void *debugMalloc(uint32_t length, int32_t tag)
{
	void *result;

	result = malloc(length);

	if (result == NULL)
	{
		debug_output( 0, "Cannot allocate %u bytes, malloc tag = %d\n", length, tag );
		restore_and_exit(0);
	}

	return result;
}

void *debugRealloc(void *memory, uint32_t length, int32_t tag)
{
	void *result;

	result = realloc(memory, length);

	if (result == NULL)
	{
		debug_output( 0, "Cannot re-allocate %u bytes, malloc tag = %d\n", length, tag );
		restore_and_exit(0);
	}

	return result;
}

void debugFree(void *memory, int32_t tag)
{
	free(memory);
}

#endif
