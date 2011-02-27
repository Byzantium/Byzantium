/*
 * Copyright (C) 2009 B.A.T.M.A.N. contributors:
 *
 * Marek Lindner
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

#ifndef TYPES_H
#define TYPES_H

#include "packet.h"

struct orig_node {                /* structure for orig_list maintaining nodes of mesh */
	uint32_t orig;
	struct neigh_node *router;
	struct batman_if *batman_if;
	TYPE_OF_WORD *bcast_own;
	uint8_t *bcast_own_sum;
	uint8_t tq_own;
	int tq_asym_penalty;
	uint32_t last_valid;        /* when last packet from this node was received */
	uint8_t  gwflags;      /* flags related to gateway functions: gateway class */
	unsigned char *hna_buff;
	int16_t  hna_buff_len;
	uint16_t last_real_seqno;   /* last and best known squence number */
	uint8_t last_ttl;         /* ttl of last received packet */
	struct list_head_first neigh_list;
};

struct neigh_node {
	struct list_head list;
	uint32_t addr;
	uint8_t real_packet_count;
	uint8_t *tq_recv;
	uint8_t tq_index;
	uint8_t tq_avg;
	uint8_t last_ttl;
	uint32_t last_valid;            /* when last packet via this neighbour was received */
	TYPE_OF_WORD *real_bits;
	struct orig_node *orig_node;
	struct batman_if *if_incoming;
};

struct forw_node {                /* structure for forw_list maintaining packets to be send/forwarded */
	struct list_head list;
	uint32_t send_time;
	uint8_t  own;
	unsigned char *pack_buff;
	uint16_t  pack_buff_len;
	uint32_t direct_link_flags;
	uint8_t num_packets;
	struct batman_if *if_incoming;
};

struct gw_node {
	struct list_head list;
	struct orig_node *orig_node;
	uint16_t gw_port;
	uint16_t gw_failure;
	uint32_t last_failure;
	uint32_t deleted;
};

struct batman_if {
	struct list_head list;
	char *dev;
	int32_t udp_send_sock;
	int32_t udp_recv_sock;
	int32_t udp_tunnel_sock;
	uint8_t if_num;
	uint8_t if_active;
	int32_t if_index;
	int8_t if_rp_filter_old;
	int8_t if_send_redirects_old;
	pthread_t listen_thread_id;
	struct sockaddr_in addr;
	struct sockaddr_in broad;
	uint32_t netaddr;
	uint8_t netmask;
	uint8_t wifi_if;
	struct bat_packet out;
};

struct gw_client {
	uint32_t wip_addr;
	uint32_t vip_addr;
	uint16_t client_port;
	uint32_t last_keep_alive;
	uint8_t nat_warn;
};

struct free_ip {
	struct list_head list;
	uint32_t addr;
};

struct vis_if {
	int32_t sock;
	struct sockaddr_in addr;
};

struct unix_if {
	int32_t unix_sock;
	pthread_t listen_thread_id;
	struct sockaddr_un addr;
	struct list_head_first client_list;
};

struct unix_client {
	struct list_head list;
	int32_t sock;
	uint8_t debug_level;
};

struct debug_clients {
	void **fd_list;
	int16_t *clients_num;
	pthread_mutex_t **mutex;
};

struct debug_level_info {
	struct list_head list;
	int32_t fd;
};

struct curr_gw_data {
	unsigned int orig;
	struct gw_node *gw_node;
	struct batman_if *batman_if;
};

struct batgat_ioc_args {
	char dev_name[16];
	unsigned char exists;
	uint32_t universal;
	uint32_t ifindex;
};

#endif
