/*
 * Copyright (C) 2006-2009 B.A.T.M.A.N. contributors:
 *
 * Simon Wunderlich, Axel Neumann, Marek Lindner
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



#include "batman.h"
#define WORD_BIT_SIZE ( sizeof(TYPE_OF_WORD) * 8 )


void bit_init( TYPE_OF_WORD *seq_bits );
uint8_t get_bit_status( TYPE_OF_WORD *seq_bits, uint16_t last_seqno, uint16_t curr_seqno );
char *bit_print( TYPE_OF_WORD *seq_bits );
void bit_mark( TYPE_OF_WORD *seq_bits, int32_t n );
void bit_shift( TYPE_OF_WORD *seq_bits, int32_t n );
char bit_get_packet( TYPE_OF_WORD *seq_bits, int16_t seq_num_diff, int8_t set_mark );
int  bit_packet_count( TYPE_OF_WORD *seq_bits );
uint8_t bit_count( int32_t to_count );

