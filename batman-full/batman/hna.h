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


extern unsigned char *hna_buff_local;
extern uint8_t num_hna_local;

/* we print the announced hna over the unix socket */
extern struct list_head_first hna_list;
/* batman client needs to feed that into the unix socket */
extern struct list_head_first hna_chg_list;


struct hna_element
{
	uint32_t addr;
	uint8_t  netmask;
} __attribute__((packed));

struct hna_task
{
	struct list_head list;
	uint32_t addr;
	uint8_t netmask;
	uint8_t route_action;
};

struct hna_local_entry
{
	struct list_head list;
	uint32_t addr;
	uint8_t netmask;
};

struct hna_global_entry
{
	uint32_t addr;
	uint8_t netmask;
	struct orig_node *curr_orig_node ALIGN_POINTER;
	struct list_head_first orig_list ALIGN_POINTER;
} __attribute__((packed));

struct hna_orig_ptr
{
	struct list_head list;
	struct orig_node *orig_node;
};

void hna_init(void);
void hna_free(void);
void hna_local_task_add_ip(uint32_t ip_addr, uint16_t netmask, uint8_t route_action);
void hna_local_task_add_str(char *hna_string, uint8_t route_action, uint8_t runtime);
void hna_local_task_exec(void);

unsigned char *hna_local_update_vis_packet(unsigned char *vis_packet, uint16_t *vis_packet_size);
void hna_local_update_routes(struct hna_local_entry *hna_local_entry, int8_t route_action);

void hna_global_add(struct orig_node *orig_node, unsigned char *new_hna, int16_t new_hna_len);
void hna_global_update(struct orig_node *orig_node, unsigned char *new_hna,
				int16_t new_hna_len, struct neigh_node *old_router);
void hna_global_check_tq(struct orig_node *orig_node);
void hna_global_del(struct orig_node *orig_node);
