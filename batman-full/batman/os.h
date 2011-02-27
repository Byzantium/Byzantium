/*
 * Copyright (C) 2006-2009 B.A.T.M.A.N. contributors:
 *
 * Thomas Lopatic, Marek Lindner
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

#ifndef _BATMAN_OS_H
#define _BATMAN_OS_H

#include "batman.h"

#ifdef __GNUC_MINOR__
#define NO_RETURN  __attribute__ ((__noreturn__))
#else
#define NO_RETURN
#endif

uint32_t get_time_msec(void);
uint64_t get_time_msec64(void);
int32_t rand_num( int32_t limit );
void addr_to_string( uint32_t addr, char *str, int32_t len );




int8_t is_aborted(void);
void handler(int32_t sig);
void segmentation_fault(int32_t sig) NO_RETURN;
void restore_and_exit(uint8_t is_sigsegv) NO_RETURN;

/* route.c */
void add_del_route( uint32_t dest, uint8_t netmask, uint32_t router, uint32_t src_ip, int32_t ifi, char *dev, uint8_t rt_table, int8_t route_type, int8_t del );
void add_del_rule( uint32_t network, uint8_t netmask, int8_t rt_table, uint32_t prio, char *iif, int8_t dst_rule, int8_t del );
int add_del_interface_rules( int8_t del );
int flush_routes_rules( int8_t rt_table );

/* tun.c */
int probe_nat_tool(void);
void add_nat_rule(char *dev);
void del_nat_rule(char *dev);
void hna_local_update_nat(uint32_t hna_ip, uint8_t netmask, int8_t route_action);
int8_t probe_tun(uint8_t print_to_stderr);
int8_t del_dev_tun( int32_t fd );
int8_t add_dev_tun( struct batman_if *batman_if, uint32_t dest_addr, char *tun_dev, size_t tun_dev_size, int32_t *fd, int32_t *ifi );
int8_t set_tun_addr( int32_t fd, uint32_t tun_addr, char *tun_dev );

/* init.c */
void apply_init_args(int argc, char *argv[]);
void init_interface(struct batman_if *batman_if);
void deactivate_interface(struct batman_if *batman_if);
void check_inactive_interfaces(void);
void check_active_interfaces(void);
void init_interface_gw(void);
void interface_listen_sockets(void);

/* kernel.c */
void set_rp_filter( int32_t state, char* dev );
int32_t get_rp_filter( char *dev );
void set_send_redirects( int32_t state, char* dev );
int32_t get_send_redirects( char *dev );
void set_forwarding( int32_t state );
int32_t get_forwarding( void );
int8_t bind_to_iface( int32_t sock, char *dev );
int32_t use_gateway_module(void);
int is_wifi_interface(char *dev, int fd);

/* posix.c */
void print_animation( void );
void del_default_route(void);
void add_default_route(void);
int8_t receive_packet(unsigned char *packet_buff, int32_t packet_buff_len, int16_t *packet_len, uint32_t *neigh, uint32_t timeout, struct batman_if **if_incoming);
int8_t send_udp_packet(unsigned char *packet_buff, int packet_buff_len, struct sockaddr_in *broad, int send_sock, struct batman_if *batman_if);
void del_gw_interface(void);
void restore_defaults(void);
void cleanup(void);

/* tunnel.c */
void init_bh_ports(void);
void *gw_listen(void *arg);
void *client_to_gw_tun( void *arg );

/* unix_sokcet.c */
void *unix_listen( void *arg );
void internal_output(uint32_t sock);
void debug_output( int8_t debug_prio, char *format, ... );


#endif
