/*
 * Copyright (C) 2008-2009 B.A.T.M.A.N. contributors:
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
 *
 * This file contains macros for maintaining compatibility with older versions
 * of the Linux kernel.
 */

#include <linux/version.h>	/* LINUX_VERSION_CODE */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22)

#define skb_network_header(_skb) \
	((_skb)->nh.raw)

static inline struct iphdr *ip_hdr(const struct sk_buff *skb)
{
	return (struct iphdr *)skb_network_header(skb);
}

#endif /* KERNEL_VERSION(2, 6, 22) */


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)

static inline int kernel_bind(struct socket *sock, struct sockaddr *addr, int addrlen)
{
	return sock->ops->bind(sock, addr, addrlen);
}

#endif /* KERNEL_VERSION(2, 6, 19) */


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)

#define device_create \
	class_device_create

#define device_destroy \
	class_device_destroy
#else

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)

#define device_create \
	device_create_drvdata

#endif /* KERNEL_VERSION(2,6,27) */

#endif /* KERNEL_VERSION(2,6,26) */
