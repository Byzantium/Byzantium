/*
 * Copyright (C) 2006 B.A.T.M.A.N. contributors:
 * Thomas Lopatic, Corinna 'Elektra' Aichele, Axel Neumann, Marek Lindner
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
#include <syslog.h>

#include "bmx.h"


#define MAGIC_NUMBER 0x12345678


#ifndef NO_DEBUG_MALLOC


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



#ifdef MEMORY_USAGE

struct memoryUsage *memoryList = NULL;


struct memoryUsage
{
	struct memoryUsage *next;
	uint32_t length;
	uint32_t counter;
	int32_t tag;
};

void addMemory(uint32_t length, int32_t tag)
{

	struct memoryUsage *walker;

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

}

void removeMemory(int32_t tag, int32_t freetag)
{

	struct memoryUsage *walker;

	for ( walker = memoryList; walker != NULL; walker = walker->next ) {

		if ( walker->tag == tag ) {

			if ( walker->counter == 0 ) {

				dbg( DBGL_SYS, DBGT_ERR, 
				     "Freeing more memory than was allocated: malloc tag = %d, free tag = %d", 
				     tag, freetag );
				cleanup_all( -500069 );

			}

			walker->counter--;
			break;

		}

	}

	if ( walker == NULL ) {

		dbg( DBGL_SYS, DBGT_ERR, 
		     "Freeing memory that was never allocated: malloc tag = %d, free tag = %d",
		     tag, freetag );
		cleanup_all( -500070 );
	}
}

void debugMemory(struct ctrl_node *cn)
{
	
	struct memoryUsage *memoryWalker;

	dbg_printf( cn, "\nMemory usage information:\n" );

	for ( memoryWalker = memoryList; memoryWalker != NULL; memoryWalker = memoryWalker->next ) {

		if ( memoryWalker->counter != 0 )
			dbg_printf( cn, "   tag: %4i, num malloc: %4i, bytes per malloc: %4i, total: %6i\n", 
			         memoryWalker->tag, memoryWalker->counter, memoryWalker->length, 
			         memoryWalker->counter * memoryWalker->length );

	}
	dbg_printf( cn, "\n" );
	
}

#endif


void checkIntegrity(void)
{
	struct chunkHeader *walker;
	struct chunkTrailer *chunkTrailer;
	unsigned char *memory;

//        dbgf_all(DBGT_INFO, " ");

	for (walker = chunkList; walker != NULL; walker = walker->next)
	{
		if (walker->magicNumber != MAGIC_NUMBER)
		{
			dbgf( DBGL_SYS, DBGT_ERR, 
			     "invalid magic number in header: %08x, malloc tag = %d", 
			     walker->magicNumber, walker->tag );
			cleanup_all( -500073 );
		}

		memory = (unsigned char *)walker;

		chunkTrailer = (struct chunkTrailer *)(memory + sizeof(struct chunkHeader) + walker->length);

		if (chunkTrailer->magicNumber != MAGIC_NUMBER)
		{
			dbgf( DBGL_SYS, DBGT_ERR, 
			     "invalid magic number in trailer: %08x, malloc tag = %d", 
			     chunkTrailer->magicNumber, walker->tag );
			cleanup_all( -500075 );
		}
	}

}

void checkLeak(void)
{
	struct chunkHeader *walker;

        if (chunkList != NULL) {
		
                openlog( "bmx6", LOG_PID, LOG_DAEMON );

                for (walker = chunkList; walker != NULL; walker = walker->next) {
			syslog( LOG_ERR, "Memory leak detected, malloc tag = %d\n", walker->tag );
		
			fprintf( stderr, "Memory leak detected, malloc tag = %d \n", walker->tag );
			
		}
		
		closelog();
	}

}

void *_debugMalloc(uint32_t length, int32_t tag) {
	
	unsigned char *memory;
	struct chunkHeader *chunkHeader;
	struct chunkTrailer *chunkTrailer;
	unsigned char *chunk;

	memory = malloc(length + sizeof(struct chunkHeader) + sizeof(struct chunkTrailer));

	if (memory == NULL)
	{
		dbg( DBGL_SYS, DBGT_ERR, "Cannot allocate %u bytes, malloc tag = %d", 
		     (unsigned int)(length + sizeof(struct chunkHeader) + sizeof(struct chunkTrailer)), tag );
		cleanup_all( -500076 );
	}

	chunkHeader = (struct chunkHeader *)memory;
	chunk = memory + sizeof(struct chunkHeader);
	chunkTrailer = (struct chunkTrailer *)(memory + sizeof(struct chunkHeader) + length);

	chunkHeader->length = length;
	chunkHeader->tag = tag;
	chunkHeader->magicNumber = MAGIC_NUMBER;

	chunkTrailer->magicNumber = MAGIC_NUMBER;

	chunkHeader->next = chunkList;
	chunkList = chunkHeader;

#ifdef MEMORY_USAGE

	addMemory( length, tag );

#endif

	return chunk;
}

void *_debugRealloc(void *memoryParameter, uint32_t length, int32_t tag)
{
	
	unsigned char *memory;
	struct chunkHeader *chunkHeader=NULL;
	struct chunkTrailer *chunkTrailer;
	unsigned char *result;
	uint32_t copyLength;

	if (memoryParameter) { /* if memoryParameter==NULL, realloc() should work like malloc() !! */
		memory = memoryParameter;
		chunkHeader = (struct chunkHeader *)(memory - sizeof(struct chunkHeader));

		if (chunkHeader->magicNumber != MAGIC_NUMBER)
		{
			dbgf( DBGL_SYS, DBGT_ERR, 
			     "invalid magic number in header: %08x, malloc tag = %d", 
			     chunkHeader->magicNumber, chunkHeader->tag );
			cleanup_all( -500078 );
		}

		chunkTrailer = (struct chunkTrailer *)(memory + chunkHeader->length);

		if (chunkTrailer->magicNumber != MAGIC_NUMBER)
		{
			dbgf( DBGL_SYS, DBGT_ERR, 
			     "invalid magic number in trailer: %08x, malloc tag = %d", 
			     chunkTrailer->magicNumber, chunkHeader->tag );
			cleanup_all( -500079 );
		}
	}


	result = _debugMalloc(length, tag);
	if (memoryParameter) {
		copyLength = length;

		if (copyLength > chunkHeader->length)
			copyLength = chunkHeader->length;

		memcpy(result, memoryParameter, copyLength);
		debugFree(memoryParameter, -300280);
	}

	return result;
}

void _debugFree(void *memoryParameter, int tag)
{
	
	unsigned char *memory;
	struct chunkHeader *chunkHeader;
	struct chunkTrailer *chunkTrailer;
	struct chunkHeader *walker;
	struct chunkHeader *previous;

	memory = memoryParameter;
	chunkHeader = (struct chunkHeader *)(memory - sizeof(struct chunkHeader));

	if (chunkHeader->magicNumber != MAGIC_NUMBER)
	{
		dbgf( DBGL_SYS, DBGT_ERR, 
		     "invalid magic number in header: %08x, malloc tag = %d, free tag = %d", 
		     chunkHeader->magicNumber, chunkHeader->tag, tag );
		cleanup_all( -500080 );
	}

	previous = NULL;

	for (walker = chunkList; walker != NULL; walker = walker->next)
	{
		if (walker == chunkHeader)
			break;

		previous = walker;
	}

	if (walker == NULL)
	{
		dbg( DBGL_SYS, DBGT_ERR, "Double free detected, malloc tag = %d, free tag = %d", 
		     chunkHeader->tag, tag );
		cleanup_all( -500081 );
	}

	if (previous == NULL)
		chunkList = walker->next;

	else
		previous->next = walker->next;


	chunkTrailer = (struct chunkTrailer *)(memory + chunkHeader->length);

	if (chunkTrailer->magicNumber != MAGIC_NUMBER)
	{
		dbgf( DBGL_SYS, DBGT_ERR, 
		     "invalid magic number in trailer: %08x, malloc tag = %d, free tag = %d",
		     chunkTrailer->magicNumber, chunkHeader->tag, tag );
		cleanup_all( -500082 );
	}

#ifdef MEMORY_USAGE

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

void debugMemory( struct ctrl_node *cn )
{
}

void *_debugMalloc(uint32_t length, int32_t tag)
{
	void *result;

	result = malloc(length);

	if (result == NULL)
	{
		dbg( DBGL_SYS, DBGT_ERR, "Cannot allocate %u bytes, malloc tag = %d", length, tag );
		cleanup_all( -500072 );
	}

	return result;
}

void *_debugRealloc(void *memory, uint32_t length, int32_t tag)
{
	void *result;

	result = realloc(memory, length);

	if (result == NULL) {
		dbg( DBGL_SYS, DBGT_ERR, "Cannot re-allocate %u bytes, malloc tag = %d", length, tag );
		cleanup_all( -500071 );
	}

	return result;
}

void _debugFree(void *memory, int32_t tag)
{
	free(memory);
}

#endif
