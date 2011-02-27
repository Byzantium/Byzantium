/*
 * Copyright (C) 2006-2009 B.A.T.M.A.N. contributors:
 *
 * Thomas Lopatic, Corinna 'Elektra' Aichele, Axel Neumann, Marek Lindner, Andreas Langer
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


#define LINUX

#define DRIVER_AUTHOR "Andreas Langer <an.langer@gmx.de>, Marek Lindner <lindner_marek@yahoo.de>"
#define DRIVER_DESC   "batman gateway module"
#define DRIVER_DEVICE "batgat"

#include <linux/version.h>	/* KERNEL_VERSION ... */
#include <linux/fs.h>		/* fops ...*/
#include <linux/devfs_fs_kernel.h>
#include <linux/module.h>
#include <linux/in.h>		/* sockaddr_in */
#include <linux/net.h>		/* socket */
#include <linux/string.h>	/* strlen, strstr, strncmp ... */
#include <linux/ip.h>		/* iphdr */
#include <linux/if_arp.h>	/* ARPHRD_NONE */
#include <net/sock.h>		/* sock */
#include <net/pkt_sched.h>	/* class_create, class_destroy, class_device_create */
#include <linux/list.h>		/* list handling */
#include <linux/if.h>		/*IFNAMSIZ*/
#include <linux/netdevice.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
// #include <linux/smp_lock.h>	/* lock_kernel */

/* io controls */
#define IOCSETDEV 1
#define IOCREMDEV 2

#define TRANSPORT_PACKET_SIZE 29
#define VIP_BUFFER_SIZE 5
#define BATMAN_PORT 4306

#define TUNNEL_DATA 0x01
#define TUNNEL_IP_REQUEST 0x02
#define TUNNEL_IP_INVALID 0x03
#define TUNNEL_KEEPALIVE_REQUEST 0x04
#define TUNNEL_KEEPALIVE_REPLY 0x05

#define LEASE_TIME 1500

#define DBG(msg,args...) do { printk(KERN_DEBUG "batgat: [%s:%u] " msg "\n", __func__ ,__LINE__, ##args); } while(0)
#define ip_hdr(skb) (struct iphdr*)skb->nh.iph


#ifndef list_for_each_entry_safe
#define list_for_each_entry_safe(pos, n, head, member) \
	for (pos = list_entry((head)->next, typeof(*pos), member), \
	n = list_entry(pos->member.next, typeof(*pos), member); \
	&pos->member != (head); \
	pos = n, n = list_entry(n->member.next, typeof(*n), member))
#endif

#define PROC_ROOT_DIR "batgat"
#define PROC_FILE_CLIENTS "clients"

#ifndef REVISION_VERSION
#define REVISION_VERSION "1"
#endif

struct gw_client {
	uint32_t wip_addr;
	uint32_t vip_addr;
	uint16_t client_port;
	uint32_t last_keep_alive;
};

struct batgat_ioc_args {
	char dev_name[IFNAMSIZ];
	unsigned char exists;
	uint32_t universal;
	uint32_t ifindex;
};

struct gate_priv {
	struct socket *tun_socket;
};

struct free_client_data {
	struct list_head list;
	struct gw_client *gw_client;
};
