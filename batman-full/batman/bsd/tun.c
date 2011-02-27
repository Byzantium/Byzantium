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

/* This file contains functions interfacing tun devices on BSD. */

#warning BSD support is known broken - if you compile this on BSD you are expected to fix it :-P

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_tun.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "../os.h"
#include "../batman.h"


/*
 * open_tun_any() opens an available tun device.
 * It returns the file descriptor as return value,
 * or -1 on failure.
 *
 * The human readable name of the device (e.g. "/dev/tun0") is
 * copied into the dev_name parameter. The buffer to hold
 * this string is assumed to be dev_name_size bytes large.
 */
#if defined(__OpenBSD__) || defined(__Darwin__)
static int open_tun_any(char *dev_name, size_t dev_name_size)
{
	int i;
	int fd;
	char tun_dev_name[12]; /* 12 = length("/dev/tunxxx\0") */

	for (i = 0; i < sizeof(tun_dev_name); i++)
		tun_dev_name[i] = '\0';

	/* Try opening tun device /dev/tun[0..255] */
	for (i = 0; i < 256; i++) {
		snprintf(tun_dev_name, sizeof(tun_dev_name), "/dev/tun%i", i);
		if ((fd = open(tun_dev_name, O_RDWR)) != -1) {
			if (dev_name != NULL)
				strlcpy(dev_name, tun_dev_name, dev_name_size);
			return fd;
		}
	}
	return -1;
}
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
static int open_tun_any(char *dev_name, size_t dev_name_size)
{
	int fd;
	struct stat buf;

	/* Open lowest unused tun device */
	if ((fd = open("/dev/tun", O_RDWR)) != -1) {
		fstat(fd, &buf);
		printf("Using %s\n", devname(buf.st_rdev, S_IFCHR));
		if (dev_name != NULL)
			strlcpy(dev_name, devname(buf.st_rdev, S_IFCHR), dev_name_size);
		return fd;
	}
	return -1;
}
#endif

/* Probe for nat tool availability */
int probe_nat_tool(void) {
	fprintf(stderr, "probe_nat_tool: not implemented\n");
	return -1;
}

void add_nat_rule(char *BATMANUNUSED(dev)) {
	fprintf(stderr, "add_nat_rule: not implemented\n");
}

void del_nat_rule(char *BATMANUNUSED(dev)) {
	fprintf(stderr, "del_nat_rule: not implemented\n");
}

void own_hna_rules(uint32_t hna_ip, uint8_t netmask, int8_t route_action) {
	fprintf(stderr, "own_hna_rules: not implemented\n");
}

/* Probe for tun interface availability */
int8_t probe_tun(uint8_t BATMANUNUSED(print_to_stderr))
{
	int fd;
	fd = open_tun_any(NULL, 0);
	if (fd == -1)
		return 0;
	close(fd);
	return 1;
}

int8_t del_dev_tun(int32_t fd)
{
	return close(fd);
}

int8_t set_tun_addr(int32_t BATMANUNUSED(fd), uint32_t tun_addr, char *tun_ifname)
{
	int so;
	struct ifreq ifr_tun;
	struct sockaddr_in *addr;

	memset(&ifr_tun, 0, sizeof(ifr_tun));
	strlcpy(ifr_tun.ifr_name, tun_ifname, IFNAMSIZ);

	so = socket(AF_INET, SOCK_DGRAM, 0);

	/* Get interface flags */
	if (ioctl(so, SIOCGIFFLAGS, &ifr_tun) < 0) {
		perror("SIOCGIFFLAGS");
		return -1;
	}

	/* Set address */
	addr = (struct sockaddr_in*)&ifr_tun.ifr_addr;
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = tun_addr;
	if (ioctl(so, SIOCAIFADDR, &ifr_tun) < 0) {
		perror("SIOCAIFADDR");
		return -1;
	}
	close(so);

	return 0;
}

int8_t add_dev_tun(struct batman_if *batman_if, uint32_t tun_addr,
		char *tun_dev, size_t tun_dev_size, int32_t *fd, int32_t *BATMANUNUSED(ifi))
{
	int so;
	struct ifreq ifr_tun, ifr_if;
	struct tuninfo ti;
	char *tun_ifname;

	memset(&ifr_tun, 0, sizeof(ifr_tun));
	memset(&ifr_if, 0, sizeof(ifr_if));
	memset(&ti, 0, sizeof(ti));

	if ((*fd = open_tun_any(tun_dev, tun_dev_size)) < 0) {
		perror("Could not open tun device");
		return -1;
	}

	printf("Using %s\n", tun_dev);

	/* Initialise tuninfo to defaults. */
	if (ioctl(*fd, TUNGIFINFO, &ti) < 0) {
		perror("TUNGIFINFO");
		del_dev_tun(*fd);
		return -1;
	}

	/* Set name of interface to configure ("tunX") */
	tun_ifname = strstr(tun_dev, "tun");
	if (tun_ifname == NULL) {
		warn("Cannot determine tun interface name!");
		return -1;
	}
	strlcpy(ifr_tun.ifr_name, tun_ifname, IFNAMSIZ);

	/* Open temporary socket to configure tun interface. */
	so = socket(AF_INET, SOCK_DGRAM, 0);

	/* Get interface flags for tun device */
	if (ioctl(so, SIOCGIFFLAGS, &ifr_tun) < 0) {
		perror("SIOCGIFFLAGS");
		del_dev_tun(*fd);
		return -1;
	}

	/* Set up and running interface flags on tun device. */
	ifr_tun.ifr_flags |= IFF_UP;
	ifr_tun.ifr_flags |= IFF_RUNNING;
	if (ioctl(so, SIOCSIFFLAGS, &ifr_tun) < 0) {
		perror("SIOCSIFFLAGS");
		del_dev_tun(*fd);
		return -1;
	}

	/* Set IP of this end point of tunnel */
	if (set_tun_addr(*fd, tun_addr, tun_ifname) < 0) {
		perror("set_tun_addr");
		del_dev_tun(*fd);
		return -1;
	}

	/* get MTU from real interface */
	strlcpy(ifr_if.ifr_name, batman_if->dev, IFNAMSIZ);
	if (ioctl(so, SIOCGIFMTU, &ifr_if) < 0) {
		perror("SIOCGIFMTU");
		del_dev_tun(*fd);
		return -1;
	}
	/* set MTU of tun interface: real MTU - 28 */
	if (ifr_if.ifr_mtu < 100) {
		fprintf(stderr, "Warning: MTU smaller than 100 - cannot reduce MTU anymore\n" );
	} else {
		ti.mtu = ifr_if.ifr_mtu - 28;
		if (ioctl(*fd, TUNSIFINFO, &ti) < 0) {
			perror("TUNSIFINFO");
			del_dev_tun(*fd);
			return -1;
		}
	}

	strlcpy(tun_dev, ifr_tun.ifr_name, tun_dev_size);
	close(so);
	return 1;
}

