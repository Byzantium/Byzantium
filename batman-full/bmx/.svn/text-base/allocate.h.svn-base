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


#ifndef _ALLOCATE_H
#define _ALLOCATE_H 1

#include <stdint.h>


// currently used memory tags: -300000, -300001 .. -300317
#define debugMalloc( length,tag )  _debugMalloc( (length), (tag) )
#define debugRealloc( mem,length,tag ) _debugRealloc( (mem), (length), (tag) )
#define debugFree( mem,tag ) _debugFree( (mem), (tag) )
void *_debugMalloc(uint32_t length, int32_t tag);
void *_debugRealloc(void *memory, uint32_t length, int32_t tag);
void _debugFree(void *memoryParameter, int32_t tag);

void checkIntegrity(void);
void checkLeak(void);
void debugMemory( struct ctrl_node *cn );


#endif
