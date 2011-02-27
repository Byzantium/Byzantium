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

/* This file contains functions that deal with BSD kernel interfaces,
 * such as sysctls. */

#warning BSD support is known broken - if you compile this on BSD you are expected to fix it :-P

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>

#include "../os.h"
#include "../batman.h"

void set_forwarding(int32_t state)
{
	int mib[4];

	/* FreeBSD allows us to set a boolean sysctl to anything.
	 * Check the value for sanity. */
	if (state < 0 || state > 1) {
		errno = EINVAL;
		err(1, "set_forwarding: %i", state);
	}

	/* "net.inet.ip.forwarding" */
	mib[0] = CTL_NET;
	mib[1] = PF_INET;
	mib[2] = IPPROTO_IP;
	mib[3] = IPCTL_FORWARDING;

	if (sysctl(mib, 4, NULL, 0, (void*)&state, sizeof(state)) == -1)
		err(1, "Cannot change net.inet.ip.forwarding");
}

int32_t get_forwarding(void)
{
	int state;
	size_t len;
	int mib[4];

	/* "net.inet.ip.forwarding" */
	mib[0] = CTL_NET;
	mib[1] = PF_INET;
	mib[2] = IPPROTO_IP;
	mib[3] = IPCTL_FORWARDING;

	len = sizeof(state);

	if (sysctl(mib, 4, &state, &len, NULL, 0) == -1)
		err(1, "Cannot tell if packet forwarding is enabled");

	return state;
}

void set_send_redirects(int32_t state, char* BATMANUNUSED(dev))
{
	int mib[4];

	/* FreeBSD allows us to set a boolean sysctl to anything.
	 * Check the value for sanity. */
	if (state < 0 || state > 1) {
		errno = EINVAL;
		err(1, "set_send_redirects: %i", state);
	}

	/* "net.inet.ip.redirect" */
	mib[0] = CTL_NET;
	mib[1] = PF_INET;
	mib[2] = IPPROTO_IP;
	mib[3] = IPCTL_SENDREDIRECTS;

	if (sysctl(mib, 4, NULL, 0, (void*)&state, sizeof(state)) == -1)
		err(1, "Cannot change net.inet.ip.redirect");
}

int32_t get_send_redirects(char *BATMANUNUSED(dev))
{
	int state;
	size_t len;
	int mib[4];

	/* "net.inet.ip.redirect" */
	mib[0] = CTL_NET;
	mib[1] = PF_INET;
	mib[2] = IPPROTO_IP;
	mib[3] = IPCTL_SENDREDIRECTS;

	len = sizeof(state);

	if (sysctl(mib, 4, &state, &len, NULL, 0) == -1)
		err(1, "Cannot tell if redirects are enabled");

	return state;
}

void set_rp_filter( int32_t BATMANUNUSED(state), char* BATMANUNUSED(dev) )
{
	/* On BSD, reverse path filtering should be disabled in the firewall. */
	return;
}

int32_t get_rp_filter( char *BATMANUNUSED(dev) )
{
	/* On BSD, reverse path filtering should be disabled in the firewall. */
	return 0;
}


int8_t bind_to_iface( int32_t BATMANUNUSED(udp_recv_sock), char *BATMANUNUSED(dev) )
{
	/* XXX: Is binding a socket to a specific
	 * interface possible in *BSD?
	 * Possibly via bpf... */
	return 1;
}

int32_t use_gateway_module(void)
{
	return -1;
}

