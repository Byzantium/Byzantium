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



#ifndef _BATMAN_BATMAN_H
#define _BATMAN_BATMAN_H

#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/un.h>
#include <stdint.h>
#include <stdio.h>

#define TYPE_OF_WORD uintmax_t /* you should choose something big, if you don't want to waste cpu */

#include "list-batman.h"
#include "bitarray.h"
#include "hash.h"
#include "allocate.h"
#include "profile.h"
#include "ring_buffer.h"

#define SOURCE_VERSION "0.4-alpha" /* put exactly one distinct word inside the string like "0.3-pre-alpha" or "0.3-rc1" or "0.3" */
#define ADDR_STR_LEN 16
#define TQ_MAX_VALUE 255

#define UNIX_PATH "/var/run/batmand.socket"



/***
 *
 * Things you should enable via your make file:
 *
 * DEBUG_MALLOC   enables malloc() / free() wrapper functions to detect memory leaks / buffer overflows / etc
 * MEMORY_USAGE   allows you to monitor the internal memory usage (needs DEBUG_MALLOC to work)
 * PROFILE_DATA   allows you to monitor the cpu usage for each function
 *
 ***/


#ifndef REVISION_VERSION
#define REVISION_VERSION "0"
#endif



/*
 * No configuration files or fancy command line switches yet
 * To experiment with B.A.T.M.A.N. settings change them here
 * and recompile the code
 * Here is the stuff you may want to play with:
 */

#define JITTER 100
#define TTL 50                /* Time To Live of broadcast messages */
#define PURGE_TIMEOUT 200000u  /* purge originators after time in ms if no valid packet comes in -> TODO: check influence on TQ_LOCAL_WINDOW_SIZE */
#define TQ_LOCAL_WINDOW_SIZE 64     /* sliding packet range of received originator messages in squence numbers (should be a multiple of our word size) */
#define TQ_GLOBAL_WINDOW_SIZE 5
#define TQ_LOCAL_BIDRECT_SEND_MINIMUM 1
#define TQ_LOCAL_BIDRECT_RECV_MINIMUM 1
#define TQ_TOTAL_BIDRECT_LIMIT 1

/**
 * hop penalty is applied "twice"
 * when the packet comes in and if rebroadcasted via the same interface
 */
#define TQ_HOP_PENALTY 10
#define DEFAULT_ROUTING_CLASS 30


#define MAX_AGGREGATION_BYTES 512 /* should not be bigger than 512 bytes or change the size of forw_node->direct_link_flags */
#define MAX_AGGREGATION_MS 100

#define ROUTE_TYPE_UNICAST          0
#define ROUTE_TYPE_THROW            1
#define ROUTE_TYPE_UNREACHABLE      2
#define ROUTE_TYPE_UNKNOWN          3
#define ROUTE_ADD                   0
#define ROUTE_DEL                   1

#define RULE_TYPE_SRC              0
#define RULE_TYPE_DST              1
#define RULE_TYPE_IIF              2
#define RULE_ADD                   0
#define RULE_DEL                   1


/***
 *
 * Things you should leave as is unless your know what you are doing !
 *
 * BATMAN_RT_TABLE_NETWORKS	routing table for announced networks
 * BATMAN_RT_TABLE_HOSTS	routing table for routes towards originators
 * BATMAN_RT_TABLE_UNREACH	routing table for unreachable routing entry
 * BATMAN_RT_TABLE_TUNNEL	routing table for the tunnel towards the internet gateway
 * BATMAN_RT_PRIO_DEFAULT	standard priority for routing rules
 * BATMAN_RT_PRIO_UNREACH	standard priority for unreachable rules
 * BATMAN_RT_PRIO_TUNNEL	standard priority for tunnel routing rules
 *
 ***/


#define BATMAN_RT_TABLE_NETWORKS 65
#define BATMAN_RT_TABLE_HOSTS 66
#define BATMAN_RT_TABLE_UNREACH 67
#define BATMAN_RT_TABLE_TUNNEL 68

#define BATMAN_RT_PRIO_DEFAULT 6600
#define BATMAN_RT_PRIO_UNREACH BATMAN_RT_PRIO_DEFAULT + 100
#define BATMAN_RT_PRIO_TUNNEL BATMAN_RT_PRIO_UNREACH + 100



/***
 *
 * ports which are to ignored by the blackhole check
 *
 ***/

#define BH_UDP_PORTS {4307, 162, 137, 138, 139, 5353} /* vis, SNMP-TRAP, netbios, mdns */


#define BATMANUNUSED(x) (x)__attribute__((unused))
#define ALIGN_WORD __attribute__ ((aligned(sizeof(TYPE_OF_WORD))))
#define ALIGN_POINTER __attribute__ ((aligned(sizeof(void*))))




extern char *prog_name;
extern uint8_t debug_level;
extern uint8_t debug_level_max;
extern uint8_t gateway_class;
extern uint8_t routing_class;
extern int16_t originator_interval;
extern uint32_t pref_gateway;
extern char *policy_routing_script;
extern int policy_routing_pipe;
extern pid_t policy_routing_script_pid;

extern int8_t stop;
extern int nat_tool_avail;
extern int8_t disable_client_nat;

extern struct gw_node *curr_gateway;
extern pthread_t curr_gateway_thread_id;

extern uint8_t found_ifs;
extern uint8_t active_ifs;
extern int32_t receive_max_sock;
extern fd_set receive_wait_set;

extern uint8_t unix_client;
extern uint8_t log_facility_active;

extern struct hashtable_t *orig_hash;

extern struct list_head_first if_list;
extern struct list_head_first gw_list;
extern struct list_head_first forw_list;
extern struct vis_if vis_if;
extern struct unix_if unix_if;
extern struct debug_clients debug_clients;

extern uint8_t tunnel_running;
extern uint64_t batman_clock_ticks;

extern uint8_t hop_penalty;
extern uint32_t purge_timeout;
extern uint8_t minimum_send;
extern uint8_t minimum_recv;
extern uint8_t global_win_size;
extern uint8_t local_win_size;
extern uint8_t num_words;
extern uint8_t aggregation_enabled;

#include "types.h" // can be removed as soon as these function have been cleaned up
int8_t batman(void);
void usage(void);
void verbose_usage(void);
int is_batman_if(char *dev, struct batman_if **batman_if);
void update_routes(struct orig_node *orig_node, struct neigh_node *neigh_node, unsigned char *hna_recv_buff, int16_t hna_buff_len);
void update_gw_list(struct orig_node *orig_node, uint8_t new_gwflags, uint16_t gw_port);
void get_gw_speeds(unsigned char gw_class, int *down, int *up);
unsigned char get_gw_class(int down, int up);
void choose_gw(void);

#endif
