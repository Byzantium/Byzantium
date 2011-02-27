/*
 * Copyright (C) 2006-2009 B.A.T.M.A.N. contributors:
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



#include <unistd.h>       /* close() */
#include <fcntl.h>        /* open(), O_RDWR */
#include <errno.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>    /* inet_ntop() */
#include <netinet/ip.h>   /* iph */
#include <linux/if_tun.h> /* TUNSETPERSIST, ... */
#include <linux/if.h>     /* ifr_if, ifr_tun */
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>       /* system() */
#include <sys/wait.h>     /* WEXITSTATUS */

#include "../os.h"
#include "../batman.h"

#define IPTABLES_ADD_MASQ "iptables -t nat -A POSTROUTING -o %s -j MASQUERADE"
#define IPTABLES_DEL_MASQ "iptables -t nat -D POSTROUTING -o %s -j MASQUERADE"

#define IPTABLES_ADD_MSS "iptables -t mangle -I POSTROUTING -p tcp --tcp-flags SYN,RST SYN -o %s -j TCPMSS --clamp-mss-to-pmtu"
#define IPTABLES_DEL_MSS "iptables -t mangle -D POSTROUTING -p tcp --tcp-flags SYN,RST SYN -o %s -j TCPMSS --clamp-mss-to-pmtu"

#define IPTABLES_ADD_ACC "iptables -t nat -I POSTROUTING -s %s/%i -j ACCEPT"
#define IPTABLES_DEL_ACC "iptables -t nat -D POSTROUTING -s %s/%i -j ACCEPT"


int run_cmd(char *cmd) {
	int error, pipes[2], stderr = -1, ret = 0;
	char error_log[256];

	if (pipe(pipes) < 0) {
		debug_output(3, "Warning - could not create a pipe to '%s': %s\n", cmd, strerror(errno));
		return -1;
	}

	memset(error_log, 0, 256);

	/* save stderr */
	stderr = dup(STDERR_FILENO);

	/* connect the commands output with the pipe for later logging */
	dup2(pipes[1], STDERR_FILENO);
	close(pipes[1]);

	error = system(cmd);

	/* copy stderr back */
	dup2(stderr, STDERR_FILENO);
	close(stderr);

	if ((error < 0) || (WEXITSTATUS(error) != 0)) {
		ret = read(pipes[0], error_log, sizeof(error_log));
		debug_output(3, "Warning - command '%s' returned an error\n", cmd);

		if (ret > 0)
			debug_output(3, "          %s\n", error_log);

		ret = -1;
	}

	close(pipes[0]);
	return ret;
}

/* Probe for iptables binary availability */
int probe_nat_tool(void) {
	return run_cmd("which iptables > /dev/null");
}

void exec_iptables_rule(char *cmd, int8_t route_action) {
	if (disable_client_nat)
		return;

	if (nat_tool_avail == -1) {
		debug_output(3, "Warning - could not %sactivate NAT: iptables binary not found!\n", (route_action == ROUTE_ADD ? "" : "de"));
		debug_output(3, "          You may need to run this command: %s\n", cmd);
		return;
	}

	run_cmd(cmd);
}

void add_nat_rule(char *dev) {
	char cmd[150];

	sprintf(cmd, IPTABLES_ADD_MASQ, dev);
	exec_iptables_rule(cmd, ROUTE_ADD);

	sprintf(cmd, IPTABLES_ADD_MSS, dev);
	exec_iptables_rule(cmd, ROUTE_ADD);
}

void del_nat_rule(char *dev) {
	char cmd[150];

	sprintf(cmd, IPTABLES_DEL_MASQ, dev);
	exec_iptables_rule(cmd, ROUTE_DEL);

	sprintf(cmd, IPTABLES_ADD_MSS, dev);
	exec_iptables_rule(cmd, ROUTE_DEL);
}

void hna_local_update_nat(uint32_t hna_ip, uint8_t netmask, int8_t route_action) {
	char cmd[100], ip_addr[16];

	inet_ntop(AF_INET, &hna_ip, ip_addr, sizeof(ip_addr));

	if (route_action == ROUTE_DEL)
		sprintf(cmd, IPTABLES_DEL_ACC, ip_addr, netmask);
	else
		sprintf(cmd, IPTABLES_ADD_ACC, ip_addr, netmask);

	exec_iptables_rule(cmd, route_action);
}

/* Probe for tun interface availability */
int8_t probe_tun(uint8_t print_to_stderr) {

	int32_t fd;

	if ( ( fd = open( "/dev/net/tun", O_RDWR ) ) < 0 ) {

		if (print_to_stderr)
			fprintf( stderr, "Error - could not open '/dev/net/tun' ! Is the tun kernel module loaded ?\n" );
		else
			debug_output( 0, "Error - could not open '/dev/net/tun' ! Is the tun kernel module loaded ?\n" );

		return 0;

	}

	close( fd );

	return 1;

}



int8_t del_dev_tun( int32_t fd ) {

	if ( ioctl( fd, TUNSETPERSIST, 0 ) < 0 ) {

		debug_output( 0, "Error - can't delete tun device: %s\n", strerror(errno) );
		return -1;

	}

	close( fd );

	return 1;

}



int8_t add_dev_tun( struct batman_if *batman_if, uint32_t tun_addr, char *tun_dev, size_t tun_dev_size, int32_t *fd, int32_t *ifi ) {

	int32_t tmp_fd, sock_opts;
	struct ifreq ifr_tun, ifr_if;
	struct sockaddr_in addr;

	/* set up tunnel device */
	memset( &ifr_tun, 0, sizeof(ifr_tun) );
	memset( &ifr_if, 0, sizeof(ifr_if) );

	ifr_tun.ifr_flags = IFF_TUN | IFF_NO_PI;
	strncpy( ifr_tun.ifr_name, "gate%d", IFNAMSIZ );

	if ( ( *fd = open( "/dev/net/tun", O_RDWR ) ) < 0 ) {

		debug_output( 0, "Error - can't create tun device (/dev/net/tun): %s\n", strerror(errno) );
		return -1;

	}

	if ( ( ioctl( *fd, TUNSETIFF, (void *)&ifr_tun ) ) < 0 ) {

		debug_output( 0, "Error - can't create tun device (TUNSETIFF): %s\n", strerror(errno) );
		close(*fd);
		return -1;

	}

	if ( ioctl( *fd, TUNSETPERSIST, 1 ) < 0 ) {

		debug_output( 0, "Error - can't create tun device (TUNSETPERSIST): %s\n", strerror(errno) );
		close(*fd);
		return -1;

	}


	tmp_fd = socket( AF_INET, SOCK_DGRAM, 0 );

	if ( tmp_fd < 0 ) {
		debug_output( 0, "Error - can't create tun device (udp socket): %s\n", strerror(errno) );
		del_dev_tun( *fd );
		return -1;
	}


	/* set ip of this end point of tunnel */
	memset( &addr, 0, sizeof(addr) );
	addr.sin_addr.s_addr = tun_addr;
	addr.sin_family = AF_INET;
	memcpy( &ifr_tun.ifr_addr, &addr, sizeof(struct sockaddr) );


	if ( ioctl( tmp_fd, SIOCSIFADDR, &ifr_tun) < 0 ) {

		debug_output( 0, "Error - can't create tun device (SIOCSIFADDR): %s\n", strerror(errno) );
		del_dev_tun( *fd );
		close( tmp_fd );
		return -1;

	}


	if ( ioctl( tmp_fd, SIOCGIFINDEX, &ifr_tun ) < 0 ) {

		debug_output( 0, "Error - can't create tun device (SIOCGIFINDEX): %s\n", strerror(errno) );
		del_dev_tun( *fd );
		close( tmp_fd );
		return -1;

	}

	*ifi = ifr_tun.ifr_ifindex;

	if ( ioctl( tmp_fd, SIOCGIFFLAGS, &ifr_tun) < 0 ) {

		debug_output( 0, "Error - can't create tun device (SIOCGIFFLAGS): %s\n", strerror(errno) );
		del_dev_tun( *fd );
		close( tmp_fd );
		return -1;

	}

	ifr_tun.ifr_flags |= IFF_UP;
	ifr_tun.ifr_flags |= IFF_RUNNING;

	if ( ioctl( tmp_fd, SIOCSIFFLAGS, &ifr_tun) < 0 ) {

		debug_output( 0, "Error - can't create tun device (SIOCSIFFLAGS): %s\n", strerror(errno) );
		del_dev_tun( *fd );
		close( tmp_fd );
		return -1;

	}

	/* get MTU from real interface */
	strncpy( ifr_if.ifr_name, batman_if->dev, IFNAMSIZ - 1 );

	if ( ioctl( tmp_fd, SIOCGIFMTU, &ifr_if ) < 0 ) {

		debug_output( 0, "Error - can't create tun device (SIOCGIFMTU): %s\n", strerror(errno) );
		del_dev_tun( *fd );
		close( tmp_fd );
		return -1;

	}

	/* set MTU of tun interface: real MTU - 29 */
	if ( ifr_if.ifr_mtu < 100 ) {

		debug_output( 0, "Warning - MTU smaller than 100 -> can't reduce MTU anymore\n" );

	} else {

		ifr_tun.ifr_mtu = ifr_if.ifr_mtu - 29;

		if ( ioctl( tmp_fd, SIOCSIFMTU, &ifr_tun ) < 0 ) {

			debug_output( 0, "Error - can't create tun device (SIOCSIFMTU): %s\n", strerror(errno) );
			del_dev_tun( *fd );
			close( tmp_fd );
			return -1;

		}

	}


	/* make tun socket non blocking */
	sock_opts = fcntl( *fd, F_GETFL, 0 );
	fcntl( *fd, F_SETFL, sock_opts | O_NONBLOCK );


	strncpy( tun_dev, ifr_tun.ifr_name, tun_dev_size - 1 );
	close( tmp_fd );

	return 1;

}


int8_t set_tun_addr( int32_t fd, uint32_t tun_addr, char *tun_dev ) {

	struct sockaddr_in addr;
	struct ifreq ifr_tun;


	memset( &ifr_tun, 0, sizeof(ifr_tun) );
	memset( &addr, 0, sizeof(addr) );

	addr.sin_addr.s_addr = tun_addr;
	addr.sin_family = AF_INET;
	memcpy( &ifr_tun.ifr_addr, &addr, sizeof(struct sockaddr) );

	strncpy( ifr_tun.ifr_name, tun_dev, IFNAMSIZ - 1 );

	if ( ioctl( fd, SIOCSIFADDR, &ifr_tun) < 0 ) {

		debug_output( 0, "Error - can't set tun address (SIOCSIFADDR): %s\n", strerror(errno) );
		return -1;

	}

	return 1;

}

