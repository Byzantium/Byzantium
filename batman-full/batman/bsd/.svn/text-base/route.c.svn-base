/*
 * Copyright (C) 2006, 2007 BATMAN contributors:
 * Stefan Sperling <stsp@stsp.name>
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

/* This file contains interface functions for the routing table on BSD. */

#warning BSD support is known broken - if you compile this on BSD you are expected to fix it :-P

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <err.h>

#include "../os.h"
#include "../batman.h"

/* Message structure used to interface the kernel routing table.
 * See route(4) for details on the message passing interface for
 * manipulating the kernel routing table.
 */
struct rt_msg
{
	struct rt_msghdr hdr;
	struct sockaddr_in dest;
	struct sockaddr_in gateway;
	struct sockaddr_in netmask;
};

static inline int32_t n_bits(uint8_t n)
{
	int32_t i, result;

	result = 0;

	if (n > 32)
		n = 32;
	for (i = 0; i < n; i++)
		result |= (0x80000000 >> i);

	return result;
}

/* Send routing message msg to the kernel.
 * The kernel's reply is returned in msg. */
static int rt_message(struct rt_msg *msg)
{
	int rt_sock;
	static unsigned int seq = 0;
	ssize_t len;
	pid_t pid;

	rt_sock = socket(PF_ROUTE, SOCK_RAW, AF_INET);
	if (rt_sock < 0)
		err(1, "Could not open socket to routing table");

	pid = getpid();
	len = 0;
	seq++;

	/* Send message */
	do {
		msg->hdr.rtm_seq = seq;
		len = write(rt_sock, msg, msg->hdr.rtm_msglen);
		if (len < 0) {
			warn("Error sending routing message to kernel");
			return -1;
		}
	} while (len < msg->hdr.rtm_msglen);

	/* Get reply */
	do {
		len = read(rt_sock, msg, sizeof(struct rt_msg));
		if (len < 0)
			err(1, "Error reading from routing socket");
	} while (len > 0 && (msg->hdr.rtm_seq != seq
				|| msg->hdr.rtm_pid != pid));

	if (msg->hdr.rtm_version != RTM_VERSION)
		warn("RTM_VERSION mismatch: compiled with version %i, "
		    "but running kernel uses version %i", RTM_VERSION,
		    msg->hdr.rtm_version);

	/* Check reply for errors. */
	if (msg->hdr.rtm_errno) {
		errno = msg->hdr.rtm_errno;
		return -1;
	}

	return 0;
}

/* Get IP address of a network device (e.g. "tun0"). */
static uint32_t get_dev_addr(char *dev)
{
	int so;
	struct ifreq ifr;
	struct sockaddr_in *addr;

	memset(&ifr, 0, sizeof(ifr));

	strlcpy(ifr.ifr_name, dev, IFNAMSIZ);

	so = socket(AF_INET, SOCK_DGRAM, 0);
	if (ioctl(so, SIOCGIFADDR, &ifr, sizeof(ifr)) < 0) {
		perror("SIOCGIFADDR");
		return -1;
	}

	if (ifr.ifr_addr.sa_family != AF_INET) {
		warn("get_dev_addr: got a non-IPv4 interface");
		return -1;
	}

	addr = (struct sockaddr_in*)&ifr.ifr_addr;
	return addr->sin_addr.s_addr;
}

void add_del_route(uint32_t dest, uint8_t netmask, uint32_t router, uint32_t BATMANUNUSED(src_ip),
		int32_t BATMANUNUSED(ifi), char *dev, uint8_t BATMANUNUSED(rt_table), int8_t BATMANUNUSED(route_type), int8_t del)
{
	char dest_str[16], router_str[16];
	struct rt_msg msg;

	memset(&msg, 0, sizeof(struct rt_msg));

	inet_ntop(AF_INET, &dest, dest_str, sizeof (dest_str));
	inet_ntop(AF_INET, &router, router_str, sizeof (router_str));

	/* Message header */
	msg.hdr.rtm_type = del ? RTM_DELETE : RTM_ADD;
	msg.hdr.rtm_version = RTM_VERSION;
	msg.hdr.rtm_flags = RTF_STATIC | RTF_UP;
	if (netmask == 32)
		msg.hdr.rtm_flags |= RTF_HOST;
	msg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
	msg.hdr.rtm_msglen = sizeof(struct rt_msg);

	/* Destination and gateway sockaddrs */
	msg.dest.sin_family = AF_INET;
	msg.dest.sin_len = sizeof(struct sockaddr_in);
	msg.gateway.sin_family = AF_INET;
	msg.gateway.sin_len = sizeof(struct sockaddr_in);
	msg.hdr.rtm_flags = RTF_GATEWAY;
	if (dest == router) {
		if (dest == 0) {
			/* Add default route via dev */
			fprintf(stderr, "%s default route via %s\n",
				del ? "Deleting" : "Adding", dev);
			msg.gateway.sin_addr.s_addr = get_dev_addr(dev);
		} else {
			/* Route to dest via default route.
			 * This is a nop. */
			return;
		}
	} else {
		if (router != 0) {
			/* Add route to dest via router */
			msg.dest.sin_addr.s_addr = dest;
			msg.gateway.sin_addr.s_addr = router;
			fprintf(stderr, "%s route to %s/%i via %s\n", del ? "Deleting" : "Adding",
					dest_str, netmask, router_str);
		} else {
			/* Route to dest via default route.
			 * This is a nop. */
			return;
		}
	}

	/* Netmask sockaddr */
	msg.netmask.sin_family = AF_INET;
	msg.netmask.sin_len = sizeof(struct sockaddr_in);
	/* Netmask is passed as decimal value (e.g. 28 for a /28).
	 * So we need to convert it into a bit pattern with n_bits(). */
	msg.netmask.sin_addr.s_addr = htonl(n_bits(netmask));

	if (rt_message(&msg) < 0)
		err(1, "Cannot %s route to %s/%i",
			del ? "delete" : "add", dest_str, netmask);
}


void add_del_rule(uint32_t BATMANUNUSED(network), uint8_t BATMANUNUSED(netmask), int8_t BATMANUNUSED(rt_table),
		uint32_t BATMANUNUSED(prio), char *BATMANUNUSED(iif), int8_t BATMANUNUSED(dst_rule), int8_t BATMANUNUSED(del) )
{
	fprintf(stderr, "add_del_rule: not implemented\n");
	return;
}

int add_del_interface_rules( int8_t BATMANUNUSED(del) )
{
	fprintf(stderr, "add_del_interface_rules: not implemented\n");
	return 0;
}

int flush_routes_rules( int8_t BATMANUNUSED(rt_table) )
{
	fprintf(stderr, "flush_routes_rules: not implemented\n");
	return 0;
}

