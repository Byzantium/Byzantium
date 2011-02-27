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



enum {

	PROF_choose_gw,
	PROF_update_routes,
	PROF_update_gw_list,
	PROF_is_duplicate,
	PROF_get_orig_node,
	PROF_update_originator,
	PROF_purge_originator,
	PROF_schedule_forward_packet,
	PROF_send_outstanding_packets,
	PROF_COUNT

};


struct prof_container {

	clock_t start_time;
	clock_t total_time;
	char *name;
	uint64_t calls;

};


void prof_init(int32_t index, char *name);
void prof_start(int32_t index);
void prof_stop(int32_t index);
void prof_print(void);
