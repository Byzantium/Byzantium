/*
 * Copyright (C) 2006-2009 B.A.T.M.A.N. contributors:
 *
 * Simon Wunderlich, Marek Lindner
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
#include "types.h"


struct neigh_node * create_neighbor(struct orig_node *orig_node, struct orig_node *orig_neigh_node, uint32_t neigh, struct batman_if *if_incoming);
int compare_orig( void *data1, void *data2 );
int choose_orig( void *data, int32_t size );
struct orig_node *get_orig_node( uint32_t addr );
void update_orig( struct orig_node *orig_node, struct bat_packet *in, uint32_t neigh, struct batman_if *if_incoming, unsigned char *hna_recv_buff, int16_t hna_buff_len, uint8_t is_duplicate, uint32_t curr_time );
void purge_orig( uint32_t curr_time );
void debug_orig(void);

