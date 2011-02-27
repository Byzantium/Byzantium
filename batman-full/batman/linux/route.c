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



#define _GNU_SOURCE
#include <sys/ioctl.h>
#include <arpa/inet.h>    /* inet_ntop() */
#include <errno.h>
#include <unistd.h>       /* close() */
#include <linux/if.h>     /* ifr_if, ifr_tun */
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>

#include "../os.h"
#include "../batman.h"


static const char *route_type_to_string[] = {
	[ROUTE_TYPE_UNICAST]      = "route",
	[ROUTE_TYPE_UNREACHABLE]  = "unreachable route",
	[ROUTE_TYPE_THROW]        = "throw route",
	[ROUTE_TYPE_UNKNOWN]      = "unknown route type",
};

static const char *route_type_to_string_script[] = {
	[ROUTE_TYPE_UNICAST]     = "UNICAST",
	[ROUTE_TYPE_UNREACHABLE] = "UNREACH",
	[ROUTE_TYPE_THROW]       = "THROW",
	[ROUTE_TYPE_UNKNOWN]     = "UNKNOWN",
};

#ifndef NO_POLICY_ROUTING
static const char *rule_type_to_string[] = {
	[RULE_TYPE_SRC] = "from",
	[RULE_TYPE_DST] = "to",
	[RULE_TYPE_IIF] = "iif",
};

static const char *rule_type_to_string_script[] = {
	[RULE_TYPE_SRC] = "SRC",
	[RULE_TYPE_DST] = "DST",
	[RULE_TYPE_IIF] = "IIF",
};
#endif


/***
 *
 * route types: 0 = UNICAST, 1 = THROW, 2 = UNREACHABLE
 *
 ***/

#if NO_POLICY_ROUTING

#include <net/route.h>

void add_del_route(uint32_t dest, uint8_t netmask, uint32_t router, uint32_t src_ip, int32_t ifi, char *dev, uint8_t rt_table, int8_t route_type, int8_t route_action)
{
	struct rtentry route;
	struct sockaddr_in *addr;
	char str1[16], str2[16], str3[16];
	int sock;

	inet_ntop(AF_INET, &dest, str1, sizeof (str1));
	inet_ntop(AF_INET, &router, str2, sizeof (str2));
	inet_ntop(AF_INET, &src_ip, str3, sizeof(str3));

	if (policy_routing_script != NULL) {
		dprintf(policy_routing_pipe, "ROUTE %s %s %s %i %s %s %i %s %i\n", (route_action == ROUTE_DEL ? "del" : "add"), route_type_to_string_script[route_type], str1, netmask, str2, str3, ifi, dev, rt_table);
		return;
	}

	/* ignore non-unicast routes as we can't handle them */
	if (route_type != ROUTE_TYPE_UNICAST)
		return;

	memset(&route, 0, sizeof (struct rtentry));
	addr = (struct sockaddr_in *)&route.rt_dst;

	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = dest;

	addr = (struct sockaddr_in *)&route.rt_genmask;

	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = (netmask == 32 ? 0xffffffff : htonl(~(0xffffffff >> netmask)));

	route.rt_flags = (netmask == 32 ? (RTF_HOST | RTF_UP) : RTF_UP);
	route.rt_metric = 1;

	/* adding default route */
	if ((router == dest) && (dest == 0)) {

		addr = (struct sockaddr_in *)&route.rt_gateway;

		addr->sin_family = AF_INET;
		addr->sin_addr.s_addr = router;

		if (route_type != ROUTE_TYPE_UNREACHABLE) {
			debug_output(3, "%s default route via %s\n", (route_action == ROUTE_DEL ? "Deleting" : "Adding"), dev);
			debug_output(4, "%s default route via %s\n", (route_action == ROUTE_DEL ? "Deleting" : "Adding"), dev);
		}

	/* single hop neigbor */
	} else if ((router == dest) && (dest != 0)) {

		debug_output(3, "%s route to %s via 0.0.0.0 (%s)\n", (route_action == ROUTE_DEL ? "Deleting" : "Adding"), str1, dev);
		debug_output(4, "%s route to %s via 0.0.0.0 (%s)\n", (route_action == ROUTE_DEL ? "Deleting" : "Adding"), str1, dev);

	/* multihop neighbor */
	} else {

		addr = (struct sockaddr_in *)&route.rt_gateway;

		addr->sin_family = AF_INET;
		addr->sin_addr.s_addr = router;

		debug_output(3, "%s %s to %s/%i via %s (%s)\n", (route_action == ROUTE_DEL ? "Deleting" : "Adding"), route_type_to_string[route_type], str1, netmask, str2, dev);
		debug_output(4, "%s %s to %s/%i via %s (%s)\n", (route_action == ROUTE_DEL ? "Deleting" : "Adding"), route_type_to_string[route_type], str1, netmask, str2, dev);

	}

	route.rt_dev = dev;

	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		debug_output(0, "Error - can't create socket for routing table manipulation: %s\n", strerror(errno));
		return;
	}

	if (ioctl(sock, (route_action == ROUTE_DEL ? SIOCDELRT : SIOCADDRT), &route) < 0)
		debug_output(0, "Error - can't %s route to %s/%i via %s: %s\n", (route_action == ROUTE_DEL ? "delete" : "add"), str1, netmask, str2, strerror(errno));

	close(sock);
}

void add_del_rule(uint32_t BATMANUNUSED(network), uint8_t BATMANUNUSED(netmask), int8_t BATMANUNUSED(rt_table), uint32_t BATMANUNUSED(prio), char *BATMANUNUSED(iif), int8_t BATMANUNUSED(rule_type), int8_t BATMANUNUSED(rule_action))
{
	return;
}

int add_del_interface_rules(int8_t BATMANUNUSED(rule_action))
{
	return 1;
}

int flush_routes_rules(int8_t BATMANUNUSED(is_rule))
{
	return 1;
}

#else

void add_del_route(uint32_t dest, uint8_t netmask, uint32_t router, uint32_t src_ip, int32_t ifi, char *dev, uint8_t rt_table, int8_t route_type, int8_t route_action)
{
	int netlink_sock;
	size_t len;
	uint32_t my_router;
	char buf[4096], str1[16], str2[16], str3[16];
	struct rtattr *rta;
	struct sockaddr_nl nladdr;
	struct iovec iov;
	struct msghdr msg;
	struct nlmsghdr *nh;
	struct req_s {
		struct rtmsg rtm;
		char buff[4 * (sizeof(struct rtattr) + 4)];
	} *req;
	char req_buf[NLMSG_LENGTH(sizeof(struct req_s))] ALIGN_WORD;

	iov.iov_base = buf;
	iov.iov_len  = sizeof(buf);

	inet_ntop(AF_INET, &dest, str1, sizeof(str1));
	inet_ntop(AF_INET, &router, str2, sizeof(str2));
	inet_ntop(AF_INET, &src_ip, str3, sizeof(str3));

	if (policy_routing_script != NULL) {
		dprintf(policy_routing_pipe, "ROUTE %s %s %s %i %s %s %i %s %i\n", (route_action == ROUTE_DEL ? "del" : "add"), route_type_to_string_script[route_type], str1, netmask, str2, str3, ifi, dev, rt_table);
		return;
	}

	/* adding default route */
	if ((router == dest) && (dest == 0)) {

		if (route_type != ROUTE_TYPE_UNREACHABLE) {
			debug_output(3, "%s default route via %s (table %i)\n", (route_action == ROUTE_DEL ? "Deleting" : "Adding"), dev, rt_table);
			debug_output(4, "%s default route via %s (table %i)\n", (route_action == ROUTE_DEL ? "Deleting" : "Adding"), dev, rt_table);
		}

		my_router = router;

	/* single hop neigbor */
	} else if ((router == dest) && (dest != 0)) {

		debug_output(3, "%s route to %s via 0.0.0.0 (table %i - %s)\n", (route_action == ROUTE_DEL ? "Deleting" : "Adding"), str1, rt_table, dev);
		debug_output(4, "%s route to %s via 0.0.0.0 (table %i - %s)\n", (route_action == ROUTE_DEL ? "Deleting" : "Adding"), str1, rt_table, dev);
		my_router = 0;

	/* multihop neighbor */
	} else {

		debug_output(3, "%s %s to %s/%i via %s (table %i - %s)\n", (route_action == ROUTE_DEL ? "Deleting" : "Adding"), route_type_to_string[route_type], str1, netmask, str2, rt_table, dev);
		debug_output(4, "%s %s to %s/%i via %s (table %i - %s)\n", (route_action == ROUTE_DEL ? "Deleting" : "Adding"), route_type_to_string[route_type], str1, netmask, str2, rt_table, dev);
		my_router = router;

	}


	nh = (struct nlmsghdr *)req_buf;
	req = (struct req_s*)NLMSG_DATA(req_buf);
	memset(&nladdr, 0, sizeof(struct sockaddr_nl));
	memset(req_buf, 0, NLMSG_LENGTH(sizeof(struct req_s)));
	memset(&msg, 0, sizeof(struct msghdr));

	nladdr.nl_family = AF_NETLINK;

	len = sizeof(struct rtmsg) + sizeof(struct rtattr) + 4;

	/* additional information for the outgoing interface and the gateway */
	if (route_type == ROUTE_TYPE_UNICAST)
		len += 2 * (sizeof(struct rtattr) + 4);

	if (src_ip != 0)
		len += sizeof(struct rtattr) + 4;

	nh->nlmsg_len = NLMSG_LENGTH(len);
	nh->nlmsg_pid = getpid();
	req->rtm.rtm_family = AF_INET;
	req->rtm.rtm_table = rt_table;
	req->rtm.rtm_dst_len = netmask;

	if (route_action == ROUTE_DEL) {

		nh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
		nh->nlmsg_type = RTM_DELROUTE;
		req->rtm.rtm_scope = RT_SCOPE_NOWHERE;

	} else {

		nh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_APPEND;
		nh->nlmsg_type = RTM_NEWROUTE;

		if (route_type == ROUTE_TYPE_UNICAST && my_router == 0 && src_ip != 0)
			req->rtm.rtm_scope = RT_SCOPE_LINK;
		else
			req->rtm.rtm_scope = RT_SCOPE_UNIVERSE;

		req->rtm.rtm_protocol = RTPROT_STATIC; /* may be changed to some batman specific value - see <linux/rtnetlink.h> */

		switch(route_type) {
		case ROUTE_TYPE_UNICAST:
			req->rtm.rtm_type = RTN_UNICAST;
			break;
		case ROUTE_TYPE_THROW:
			req->rtm.rtm_type = RTN_THROW;
			break;
		case ROUTE_TYPE_UNREACHABLE:
			req->rtm.rtm_type = RTN_UNREACHABLE;
			break;
		default:
			debug_output(0, "Error - unknown route type (add_del_route): %i\n", route_type);
			return;
		}
	}

	rta = (struct rtattr *)req->buff;
	rta->rta_type = RTA_DST;
	rta->rta_len = sizeof(struct rtattr) + 4;
	memcpy(((char *)req->buff) + sizeof(struct rtattr), (char *)&dest, 4);

	if (route_type == ROUTE_TYPE_UNICAST) {

		rta = (struct rtattr *)(req->buff + sizeof(struct rtattr) + 4);
		rta->rta_type = RTA_GATEWAY;
		rta->rta_len = sizeof(struct rtattr) + 4;
		memcpy(((char *)req->buff) + 2 * sizeof(struct rtattr) + 4, (char *)&my_router, 4);

		rta = (struct rtattr *)(req->buff + 2 * sizeof(struct rtattr) + 8);
		rta->rta_type = RTA_OIF;
		rta->rta_len = sizeof(struct rtattr) + 4;
		memcpy(((char *)req->buff) + 3 * sizeof(struct rtattr) + 8, (char *)&ifi, 4);

		if (src_ip != 0) {
			rta = (struct rtattr *)(req->buff + 3 * sizeof(struct rtattr) + 12);
			rta->rta_type = RTA_PREFSRC;
			rta->rta_len = sizeof(struct rtattr) + 4;
			memcpy(((char *)req->buff) + 4 * sizeof(struct rtattr) + 12, (char *)&src_ip, 4);
		}

	}

	if ((netlink_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0) {

		debug_output(0, "Error - can't create netlink socket for routing table manipulation: %s\n", strerror(errno));
		return;

	}

	if (sendto(netlink_sock, req_buf, nh->nlmsg_len, 0, (struct sockaddr *)&nladdr, sizeof(struct sockaddr_nl)) < 0) {

		debug_output(0, "Error - can't send message to kernel via netlink socket for routing table manipulation: %s\n", strerror(errno));
		close(netlink_sock);
		return;

	}


	msg.msg_name = (void *)&nladdr;
	msg.msg_namelen = sizeof(nladdr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;

	len = recvmsg(netlink_sock, &msg, 0);
	nh = (struct nlmsghdr *)buf;

	while (NLMSG_OK(nh, len)) {

		if (nh->nlmsg_type == NLMSG_DONE)
			break;

		if (( nh->nlmsg_type == NLMSG_ERROR) && (((struct nlmsgerr*)NLMSG_DATA(nh))->error != 0))
			debug_output(0, "Error - can't %s %s to %s/%i via %s (table %i): %s\n", (route_action == ROUTE_DEL ? "delete" : "add"), route_type_to_string[route_type], str1, netmask, str2, rt_table, strerror(-((struct nlmsgerr*)NLMSG_DATA(nh))->error));

		nh = NLMSG_NEXT(nh, len);

	}

	close(netlink_sock);
}

/***
 *
 * rule types: 0 = RTA_SRC, 1 = RTA_DST, 2 = RTA_IIF
 *
 ***/

void add_del_rule(uint32_t network, uint8_t netmask, int8_t rt_table, uint32_t prio, char *iif, int8_t rule_type, int8_t rule_action)
{
	int netlink_sock;
	size_t len;
	char buf[4096], str1[16];
	struct rtattr *rta;
	struct sockaddr_nl nladdr;
	struct iovec iov;
	struct msghdr msg;
	struct nlmsghdr *nh;
	struct req_s {
		struct rtmsg rtm;
		char buff[2 * (sizeof(struct rtattr) + 4)];
	} *req;
	char req_buf[NLMSG_LENGTH(sizeof(struct req_s))] ALIGN_WORD;

	iov.iov_base = buf;
	iov.iov_len  = sizeof(buf);


	inet_ntop(AF_INET, &network, str1, sizeof (str1));

	if (policy_routing_script != NULL) {
		dprintf(policy_routing_pipe, "RULE %s %s %s %i %s %s %u %s %i\n", (rule_action == RULE_DEL ? "del" : "add"), rule_type_to_string[rule_type], str1, netmask, "unused", "unused", prio, iif, rt_table);
		return;
	}


	nh = (struct nlmsghdr *)req_buf;
	req = (struct req_s*)NLMSG_DATA(req_buf);
	memset(&nladdr, 0, sizeof(struct sockaddr_nl));
	memset(req_buf, 0, NLMSG_LENGTH(sizeof(struct req_s)));
	memset(&msg, 0, sizeof(struct msghdr));

	nladdr.nl_family = AF_NETLINK;

	len = sizeof(struct rtmsg) + sizeof(struct rtattr) + 4;

	if (prio != 0)
		len += sizeof(struct rtattr) + 4;

	nh->nlmsg_len = NLMSG_LENGTH(len);
	nh->nlmsg_pid = getpid();
	req->rtm.rtm_family = AF_INET;
	req->rtm.rtm_table = rt_table;


	if (rule_action == RULE_DEL) {

		nh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
		nh->nlmsg_type = RTM_DELRULE;
		req->rtm.rtm_scope = RT_SCOPE_NOWHERE;

	} else {

		nh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_EXCL;
		nh->nlmsg_type = RTM_NEWRULE;
		req->rtm.rtm_scope = RT_SCOPE_UNIVERSE;
		req->rtm.rtm_protocol = RTPROT_STATIC;
		req->rtm.rtm_type = RTN_UNICAST;

	}

	switch(rule_type) {
	case RULE_TYPE_SRC:
		rta = (struct rtattr *)req->buff;
		rta->rta_type = RTA_SRC;
		req->rtm.rtm_src_len = netmask;
		rta->rta_len = sizeof(struct rtattr) + 4;
		memcpy(((char *)req->buff) + sizeof(struct rtattr), (char *)&network, 4);
		break;
	case RULE_TYPE_DST:
		rta = (struct rtattr *)req->buff;
		rta->rta_type = RTA_DST;
		req->rtm.rtm_dst_len = netmask;
		rta->rta_len = sizeof(struct rtattr) + 4;
		memcpy(((char *)req->buff) + sizeof(struct rtattr), (char *)&network, 4);
		break;
	case RULE_TYPE_IIF:
		rta = (struct rtattr *)req->buff;
		rta->rta_len = sizeof(struct rtattr) + 4;

		if (rule_action == RULE_DEL) {
			rta->rta_type = RTA_SRC;
			memcpy(((char *)req->buff) + sizeof(struct rtattr), (char *)&network, 4);
		} else {
			rta->rta_type = RTA_IIF;
			memcpy(((char *)req->buff) + sizeof(struct rtattr), iif, 4);
		}
		break;
	default:
		debug_output(0, "Error - unknown rule type (add_del_rule): %i\n", rule_type);
		return;
	}

	if (prio != 0) {
		rta = (struct rtattr *)(req->buff + sizeof(struct rtattr) + 4);
		rta->rta_type = RTA_PRIORITY;
		rta->rta_len = sizeof(struct rtattr) + 4;
		memcpy(((char *)req->buff) + 2 * sizeof(struct rtattr) + 4, (char *)&prio, 4);
	}


	if ((netlink_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0) {

		debug_output(0, "Error - can't create netlink socket for routing rule manipulation: %s\n", strerror(errno));
		return;

	}


	if (sendto(netlink_sock, req_buf, nh->nlmsg_len, 0, (struct sockaddr *)&nladdr, sizeof(struct sockaddr_nl)) < 0)  {

		debug_output( 0, "Error - can't send message to kernel via netlink socket for routing rule manipulation: %s\n", strerror(errno));
		close(netlink_sock);
		return;

	}


	msg.msg_name = (void *)&nladdr;
	msg.msg_namelen = sizeof(nladdr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;

	len = recvmsg(netlink_sock, &msg, 0);
	nh = (struct nlmsghdr *)buf;

	while (NLMSG_OK(nh, len)) {

		if (nh->nlmsg_type == NLMSG_DONE)
			break;

		if ((nh->nlmsg_type == NLMSG_ERROR) && (((struct nlmsgerr*)NLMSG_DATA(nh))->error != 0)) {

			inet_ntop(AF_INET, &network, str1, sizeof (str1));

			debug_output(0, "Error - can't %s rule %s %s/%i: %s\n", (rule_action == RULE_DEL ? "delete" : "add"), rule_type_to_string_script[rule_type], str1, netmask, strerror(-((struct nlmsgerr*)NLMSG_DATA(nh))->error));

		}

		nh = NLMSG_NEXT(nh, len);

	}

	close(netlink_sock);
}

int add_del_interface_rules(int8_t rule_action)
{
	int32_t tmp_fd;
	uint32_t addr, netaddr;
	int len;
	uint8_t netmask, if_count = 1;
	char *buf, *buf_ptr;
	struct ifconf ifc;
	struct ifreq *ifr, ifr_tmp;
	struct batman_if *batman_if;


	tmp_fd = socket(AF_INET, SOCK_DGRAM, 0);

	if (tmp_fd < 0) {
		debug_output(0, "Error - can't %s interface rules (udp socket): %s\n", (rule_action == RULE_DEL ? "delete" : "add"), strerror(errno));
		return -1;
	}


	len = 10 * sizeof(struct ifreq); /* initial buffer size guess (10 interfaces) */

	while (1) {

		buf = debugMalloc(len, 601);
		ifc.ifc_len = len;
		ifc.ifc_buf = buf;

		if (ioctl(tmp_fd, SIOCGIFCONF, &ifc) < 0) {

			debug_output(0, "Error - can't %s interface rules (SIOCGIFCONF): %s\n", (rule_action == RULE_DEL ? "delete" : "add"), strerror(errno));
			close(tmp_fd);
			debugFree(buf, 1606);
			return -1;

		} else {

			if (ifc.ifc_len < len)
				break;

		}

		len += 10 * sizeof(struct ifreq);
		debugFree(buf, 1601);

	}

	for (buf_ptr = buf; buf_ptr < buf + ifc.ifc_len;) {

		ifr = (struct ifreq *)buf_ptr;
		buf_ptr += (ifr->ifr_addr.sa_family == AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr)) + sizeof(ifr->ifr_name);

		/* ignore if not IPv4 interface */
		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;

		/* the tunnel thread will remove the default route by itself */
		if (strncmp(ifr->ifr_name, "gate", 4) == 0)
			continue;

		memset(&ifr_tmp, 0, sizeof (struct ifreq));
		strncpy(ifr_tmp.ifr_name, ifr->ifr_name, IFNAMSIZ - 1);

		if (ioctl(tmp_fd, SIOCGIFFLAGS, &ifr_tmp ) < 0) {

			debug_output(0, "Error - can't get flags of interface %s (interface rules): %s\n", ifr->ifr_name, strerror(errno));
			continue;

		}

		/* ignore if not up and running */
		if (!( ifr_tmp.ifr_flags & IFF_UP) || !(ifr_tmp.ifr_flags & IFF_RUNNING))
			continue;


		if (ioctl(tmp_fd, SIOCGIFADDR, &ifr_tmp) < 0) {

			debug_output(0, "Error - can't get IP address of interface %s (interface rules): %s\n", ifr->ifr_name, strerror(errno));
			continue;

		}

		addr = ((struct sockaddr_in *)&ifr_tmp.ifr_addr)->sin_addr.s_addr;

		if (ioctl(tmp_fd, SIOCGIFNETMASK, &ifr_tmp) < 0) {

			debug_output(0, "Error - can't get netmask address of interface %s (interface rules): %s\n", ifr->ifr_name, strerror(errno));
			continue;

		}

		netaddr = (((struct sockaddr_in *)&ifr_tmp.ifr_addr)->sin_addr.s_addr & addr);
		netmask = bit_count(((struct sockaddr_in *)&ifr_tmp.ifr_addr)->sin_addr.s_addr);

		add_del_route(netaddr, netmask, 0, 0, 0, ifr->ifr_name, BATMAN_RT_TABLE_TUNNEL, ROUTE_TYPE_THROW, rule_action);

		if (is_batman_if(ifr->ifr_name, &batman_if))
			continue;

		add_del_rule(netaddr, netmask, BATMAN_RT_TABLE_TUNNEL, (rule_action == RULE_DEL ? 0 : BATMAN_RT_PRIO_TUNNEL + if_count), 0, RULE_TYPE_SRC, rule_action);

		if (strncmp( ifr->ifr_name, "lo", IFNAMSIZ - 1) == 0)
			add_del_rule(0, 0, BATMAN_RT_TABLE_TUNNEL, BATMAN_RT_PRIO_TUNNEL, "lo\0 ", RULE_TYPE_IIF, rule_action);

		if_count++;

	}

	close(tmp_fd);
	debugFree(buf, 1605);

	return 1;
}

int flush_routes_rules(int8_t is_rule)
{
	int netlink_sock, rtl;
	size_t len;
	int32_t dest = 0, router = 0, ifi = 0;
	uint32_t prio = 0;
	int8_t rule_type = 0;
	char buf[8192], *dev = NULL;
	struct sockaddr_nl nladdr;
	struct iovec iov;
	struct msghdr msg;
	struct nlmsghdr *nh;
	struct rtmsg *rtm;
	struct req_s {
		struct rtmsg rtm;
	} *req;
	char req_buf[NLMSG_LENGTH(sizeof(struct req_s))] ALIGN_WORD;

	struct rtattr *rtap;

	iov.iov_base = buf;
	iov.iov_len  = sizeof(buf);

	nh = (struct nlmsghdr *)req_buf;
	req = (struct req_s*)NLMSG_DATA(req_buf);
	memset(&nladdr, 0, sizeof(struct sockaddr_nl));
	memset(req_buf, 0, NLMSG_LENGTH(sizeof(struct req_s)));
	memset(&msg, 0, sizeof(struct msghdr));

	nladdr.nl_family = AF_NETLINK;

	nh->nlmsg_len = NLMSG_LENGTH(sizeof(struct req_s));
	nh->nlmsg_pid = getpid();
	req->rtm.rtm_family = AF_INET;

	nh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nh->nlmsg_type = (is_rule ? RTM_GETRULE : RTM_GETROUTE);
	req->rtm.rtm_scope = RTN_UNICAST;

	if ((netlink_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0) {

		debug_output(0, "Error - can't create netlink socket for flushing the routing table: %s\n", strerror(errno));
		return -1;

	}


	if (sendto(netlink_sock, req_buf, nh->nlmsg_len, 0, (struct sockaddr *)&nladdr, sizeof(struct sockaddr_nl)) < 0) {

		debug_output(0, "Error - can't send message to kernel via netlink socket for flushing the routing table: %s\n", strerror(errno));
		close(netlink_sock);
		return -1;

	}

	msg.msg_name = (void *)&nladdr;
	msg.msg_namelen = sizeof(nladdr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;

	len = recvmsg(netlink_sock, &msg, 0);
	nh = (struct nlmsghdr *)buf;

	while (NLMSG_OK(nh, len)) {

		if (nh->nlmsg_type == NLMSG_DONE)
			break;

		if ((nh->nlmsg_type == NLMSG_ERROR) && (((struct nlmsgerr*)NLMSG_DATA(nh))->error != 0)) {

			debug_output(0, "Error - can't flush %s: %s \n", (is_rule ? "routing rules" : "routing table"), strerror(-((struct nlmsgerr*)NLMSG_DATA(nh))->error));
			close(netlink_sock);
			return -1;

		}

		rtm = (struct rtmsg *)NLMSG_DATA(nh);
		rtap = (struct rtattr *)RTM_RTA(rtm);
		rtl = RTM_PAYLOAD(nh);

		nh = NLMSG_NEXT(nh, len);

		if ((rtm->rtm_table != BATMAN_RT_TABLE_UNREACH) && (rtm->rtm_table != BATMAN_RT_TABLE_NETWORKS) &&
			(rtm->rtm_table != BATMAN_RT_TABLE_HOSTS) && (rtm->rtm_table != BATMAN_RT_TABLE_TUNNEL))
			continue;

		while (RTA_OK(rtap, rtl)) {

			switch(rtap->rta_type) {

				case RTA_DST:
					dest = *((int32_t *)RTA_DATA(rtap));
					rule_type = ROUTE_TYPE_UNREACHABLE;
					break;

				case RTA_SRC:
					dest = *((int32_t *)RTA_DATA(rtap));
					rule_type = ROUTE_TYPE_UNICAST;
					break;

				case RTA_GATEWAY:
					router = *((int32_t *)RTA_DATA(rtap));
					break;

				case RTA_OIF:
					ifi = *((int32_t *)RTA_DATA(rtap));
					break;

				case RTA_PRIORITY:
					prio = *((uint32_t *)RTA_DATA(rtap));
					break;

				case RTA_IIF:
					dev = ((char *)RTA_DATA(rtap));
					rule_type = ROUTE_TYPE_THROW;
					break;

				case 15:  /* FIXME: RTA_TABLE is not always available - not needed but avoid warning */
					break;

				case RTA_PREFSRC:  /* rta_type 7 - not needed but avoid warning */
					break;

				default:
					debug_output(0, "Error - unknown rta type: %i \n", rtap->rta_type);
					break;

			}

			rtap = RTA_NEXT(rtap,rtl);

		}

		if (is_rule) {
			switch (rule_type) {
			case ROUTE_TYPE_UNICAST:
				add_del_rule(dest, rtm->rtm_src_len, rtm->rtm_table, prio, 0 , rule_type, RULE_DEL);
				break;
			case ROUTE_TYPE_THROW:
				add_del_rule(dest, rtm->rtm_dst_len, rtm->rtm_table, prio, 0 , rule_type, RULE_DEL);
				break;
			case ROUTE_TYPE_UNREACHABLE:
				add_del_rule(0 , 0, rtm->rtm_table, prio, dev , rule_type, RULE_DEL);
				break;
			}
		} else {
			switch (rtm->rtm_type) {
			case RTN_UNICAST:
				rule_type = ROUTE_TYPE_UNICAST;
				break;
			case RTN_THROW:
				rule_type = ROUTE_TYPE_THROW;
				break;
			case RTN_UNREACHABLE:
				rule_type = ROUTE_TYPE_UNREACHABLE;
				break;
			default:
				rule_type = ROUTE_TYPE_UNKNOWN;
				break;
			}

			/* sometimes dest and router are not reset */
			if (rule_type == ROUTE_TYPE_UNREACHABLE)
				dest = router = 0;

			add_del_route(dest, rtm->rtm_dst_len, router, 0, ifi, "unknown", rtm->rtm_table, rule_type, ROUTE_DEL);
		}

	}

	close(netlink_sock);
	return 1;
}

#endif
