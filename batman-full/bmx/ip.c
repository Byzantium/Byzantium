/*
 * Copyright (c) 2010  Axel Neumann
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
 */


 #define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <fcntl.h>


#include <linux/rtnetlink.h>

#include "bmx.h"
#include "ip.h"
#include "msg.h"
#include "schedule.h"
#include "plugin.h"
#include "tools.h"
#include "metrics.h"

int32_t base_port = DEF_BASE_PORT;


int32_t Rt_prio = DEF_RT_PRIO;
int32_t Rt_table = DEF_RT_TABLE;



int32_t prio_rules = DEF_PRIO_RULES;

int32_t throw_rules = DEF_THROW_RULES;


static int32_t Lo_rule = DEF_LO_RULE;

//TODO: make this configurable
static IPX_T cfg_llocal_bmx_prefix = {{{0}}};
static uint8_t cfg_llocal_bmx_prefix_len = 0;
static IPX_T cfg_global_bmx_prefix = {{{0}}};
static uint8_t cfg_global_bmx_prefix_len = 0;

const IPX_T ZERO_IP = {{{0}}};
const MAC_T ZERO_MAC = {{0}};

//const struct link_key ZERO_LINK_KEY = {.link = NULL, .dev = NULL};

const IFNAME_T ZERO_IFNAME = {{0}};

//#define IN6ADDR_ANY_INIT { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } } }
//#define IN6ADDR_LOOPBACK_INIT { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } } }

const IP6_T IP6_LOOPBACK_ADDR = { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } } };


//const IP6_T   IP6_ALLROUTERS_MC_ADDR = {.s6_addr[0] = 0xFF, .s6_addr[1] = 0x02, .s6_addr[15] = 0x02};
const IP6_T   IP6_ALLROUTERS_MC_ADDR = {{{0xFF,0x02,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x02}}};

//const IP6_T   IP6_LINKLOCAL_UC_PREF = {.s6_addr[0] = 0xFE, .s6_addr[1] = 0x80};
const IP6_T   IP6_LINKLOCAL_UC_PREF = {{{0xFE,0x80,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}}};
const uint8_t IP6_LINKLOCAL_UC_PLEN = 10;

//const IP6_T   IP6_MC_PREF = {.s6_addr[0] = 0xFF};
const IP6_T   IP6_MC_PREF = {{{0xFF,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}}};
const uint8_t IP6_MC_PLEN = 8;


static int nlsock_default = -1;
static int nlsock_flush_all = -1;

const struct ort_data ort_dat[ORT_MAX + 1] = {
        {AF_INET, IP4_MAX_PREFIXLEN},
        {AF_INET6, IP6_MAX_PREFIXLEN}
};



static int rt_sock = 0;

struct rtnl_handle ip_rth = { .fd = -1 };

struct dev_node *primary_dev_cfg = NULL;
uint8_t af_cfg = 0;


AVL_TREE(if_link_tree, struct if_link_node, index);

AVL_TREE(dev_ip_tree, struct dev_node, llocal_ip_key);
AVL_TREE(dev_name_tree, struct dev_node, name_phy_cfg);

AVL_TREE(iptrack_tree, struct track_node, k);


static LIST_SIMPEL( throw4_list, struct throw_node, list, list );

IDM_T dev_soft_conf_changed = NO; // temporary enabled to trigger changed interface configuration



static Sha ip_sha;


#define ARG_PEDANTIC_CLEANUP "pedantic_cleanup"
#define DEF_PEDANT_CLNUP  NO
static int32_t Pedantic_cleanup = DEF_PEDANT_CLNUP;
static int32_t if6_forward_orig = -1;
static int32_t if4_forward_orig = -1;
static int32_t if4_rp_filter_all_orig = -1;
static int32_t if4_rp_filter_default_orig = -1;
static int32_t if4_send_redirects_all_orig = -1;
static int32_t if4_send_redirects_default_orig = -1;




STATIC_FUNC
int rtnl_open(struct rtnl_handle *rth)
{
        unsigned subscriptions = 0;
        int protocol = NETLINK_ROUTE;
	socklen_t addr_len;
	int sndbuf = 32768;
        int rcvbuf = 1024 * 1024;

	memset(rth, 0, sizeof(*rth));

	rth->fd = socket(AF_NETLINK, SOCK_RAW, protocol);
	if (rth->fd < 0) {
                dbgf(DBGL_SYS, DBGT_ERR, "Cannot open netlink socket");
		return FAILURE;
	}

	if (setsockopt(rth->fd,SOL_SOCKET,SO_SNDBUF,&sndbuf,sizeof(sndbuf)) < 0) {
		dbgf(DBGL_SYS, DBGT_ERR, "SO_SNDBUF");
		return FAILURE;
	}

	if (setsockopt(rth->fd,SOL_SOCKET,SO_RCVBUF,&rcvbuf,sizeof(rcvbuf)) < 0) {
		dbgf(DBGL_SYS, DBGT_ERR, "SO_RCVBUF");
		return FAILURE;
	}

	memset(&rth->local, 0, sizeof(rth->local));
	rth->local.nl_family = AF_NETLINK;
	rth->local.nl_groups = subscriptions;

	if (bind(rth->fd, (struct sockaddr*)&rth->local, sizeof(rth->local)) < 0) {
		dbgf(DBGL_SYS, DBGT_ERR, "Cannot bind netlink socket");
		return FAILURE;
	}
	addr_len = sizeof(rth->local);
	if (getsockname(rth->fd, (struct sockaddr*)&rth->local, &addr_len) < 0) {
                dbgf(DBGL_SYS, DBGT_ERR, "Cannot getsockname");
		return FAILURE;
	}
	if (addr_len != sizeof(rth->local)) {
                dbgf(DBGL_SYS, DBGT_ERR, "Wrong address length %d\n", addr_len);
		return FAILURE;
	}
	if (rth->local.nl_family != AF_NETLINK) {
                dbgf(DBGL_SYS, DBGT_ERR, "Wrong address family %d\n", rth->local.nl_family);
		return FAILURE;
	}
	rth->seq = time(NULL);
	return SUCCESS;
}



STATIC_FUNC
int open_netlink_socket( void ) {

        int sock = 0;
	if ( ( sock = socket( AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE ) ) < 0 ) {

		dbg( DBGL_SYS, DBGT_ERR, "can't create netlink socket for routing table manipulation: %s",
		     strerror(errno) );

		return -1;
	}


	if ( fcntl( sock, F_SETFL, O_NONBLOCK) < 0 ) {

		dbg( DBGL_SYS, DBGT_ERR, "can't set netlink socket nonblocking : (%s)",  strerror(errno));
		close(sock);
		return -1;
	}

	return sock;
}




STATIC_FUNC
void parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len)
{
	memset(tb, 0, sizeof(struct rtattr *) * (max + 1));

	while (RTA_OK(rta, len)) {
		if (rta->rta_type <= max)
			tb[rta->rta_type] = rta;
		rta = RTA_NEXT(rta,len);
	}

	if (len) {
                dbgf(DBGL_SYS, DBGT_ERR, "!!!Deficit %d, rta_len=%d\n", len, rta->rta_len);
        }

}

STATIC_FUNC
IDM_T get_if_req(IFNAME_T *dev_name, struct ifreq *if_req, int siocgi_req)
{

	memset( if_req, 0, sizeof (struct ifreq) );

        if (dev_name)
                strncpy(if_req->ifr_name, dev_name->str, IFNAMSIZ - 1);

        errno = 0;
        if ( ioctl( rt_sock, siocgi_req, if_req ) < 0 ) {

                if (siocgi_req != SIOCGIWNAME) {
                        dbg(DBGL_SYS, DBGT_ERR,
                                "can't get SIOCGI %d of interface %s: %s",
                                siocgi_req, dev_name->str, strerror(errno));
                }
                return FAILURE;
	}

	return SUCCESS;
}


STATIC_FUNC
int rt_macro_to_table( int rt_macro ) {

	dbgf_all(  DBGT_INFO, "rt_macro %d", rt_macro  );

        if ( rt_macro == RT_TABLE_HOSTS )
		return Rt_table + RT_TABLE_HOSTS_OFFS;

	else if ( rt_macro == RT_TABLE_NETS )
		return Rt_table + RT_TABLE_NETS_OFFS;

	else if ( rt_macro == RT_TABLE_TUNS )
		return Rt_table + RT_TABLE_TUNS_OFFS;

	else if ( rt_macro > MAX_RT_TABLE )
		cleanup_all( -500170 );

	else if ( rt_macro >= 0 )
		return rt_macro;

	cleanup_all( -500171 );

	return 0;

}
STATIC_FUNC
char *del2str(IDM_T del)
{
        return ( del ? "DEL" : "ADD");
}

#ifdef WITH_UNUSED
STATIC_FUNC
char *rtn2str(uint8_t rtn)
{
	if ( rtn == RTN_UNICAST )
		return "RTN_UNICAST";

	else if ( rtn == RTN_THROW )
		return "RTN_THROW  ";

	return "RTN_ILLEGAL";
}

STATIC_FUNC
char *rta2str(uint8_t rta)
{
	if ( rta == RTA_DST )
		return "RTA_DST";

	return "RTA_ILLEGAL";
}
#endif

STATIC_FUNC
char *trackt2str(uint8_t cmd)
{
	if ( cmd == IP_NOP )
		return "TRACK_NOP";

        else if ( cmd == IP_RULE_FLUSH )
		return "RULE_FLUSH";

        else if ( cmd == IP_RULE_DEFAULT )
		return "RULE_DEFAULT";

        else if (cmd == IP_ROUTE_FLUSH_ALL)
                return "ROUTE_FLUSH_ALL";

        else if (cmd == IP_ROUTE_FLUSH)
                return "ROUTE_FLUSH";

        else if ( cmd == IP_THROW_MY_HNA )
		return "THROW_MY_HNA";

	else if ( cmd == IP_THROW_MY_NET )
		return "THROW_MY_NET";

	else if ( cmd == IP_ROUTE_HOST )
		return "ROUTE_HOST";

	else if ( cmd == IP_ROUTE_HNA )
		return "ROUTE_HNA";

	return "TRACK_ILLEGAL";
}

char *family2Str(uint8_t family)
{
        static char b[B64_SIZE];

        switch (family) {
        case AF_INET:
                return "inet";
        case AF_INET6:
                return "inet6";
        default:
                sprintf( b, "%d ???", family);
                return b;
        }
}




void ip2Str(int family, const IPX_T *addr, char *str)
{
        assertion(-500583, (str));
        uint32_t *a;

        if (!addr)
                a = (uint32_t *)&(ZERO_IP.s6_addr32[0]);

        else if (family == AF_INET) {

                a = (uint32_t *)&(addr->s6_addr32[3]);

        } else if (family == AF_INET6) {
                
                a = (uint32_t *)&(addr->s6_addr32[0]);

        } else {
                strcpy(str, "ERROR");
                return;
        }

        inet_ntop(family, a, str, family == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN);
	return;
}

void ip42X(IPX_T *ipx, IP4_T ip4)
{
        *ipx = ZERO_IP;
	ipx->s6_addr32[3] = ip4;
}

char *ipXAsStr(int family, const IPX_T *addr)
{
	static uint8_t c=0;
        static char str[IP2S_ARRAY_LEN][INET6_ADDRSTRLEN];

	c = (c+1) % IP2S_ARRAY_LEN;

        ip2Str(family, addr, str[c]);

        return str[c];
}



char *ip4AsStr( IP4_T addr )
{

	static uint8_t c=0;
	static char str[IP2S_ARRAY_LEN][INET_ADDRSTRLEN];

	c = (c+1) % IP2S_ARRAY_LEN;

	inet_ntop( AF_INET, &addr, str[c], INET_ADDRSTRLEN );

	return str[c];
}


char* macAsStr(const MAC_T* mac)
{
        return memAsStr( mac, sizeof(MAC_T));
}

IDM_T is_mac_equal(const MAC_T *a, const MAC_T *b)
{
        return (a->u16[2] == b->u16[2] &&
                a->u16[1] == b->u16[1] &&
                a->u16[0] == b->u16[0]);

}


IDM_T is_ip_equal(const IPX_T *a, const IPX_T *b)
{
        return (a->s6_addr32[3] == b->s6_addr32[3] &&
                a->s6_addr32[2] == b->s6_addr32[2] &&
                a->s6_addr32[1] == b->s6_addr32[1] &&
                a->s6_addr32[0] == b->s6_addr32[0]);

}

IDM_T is_ip_net_equal(const IPX_T *netA, const IPX_T *netB, const uint8_t plen, const uint8_t family)
{

        IPX_T aprefix = *netA;
        IPX_T bprefix = *netB;

        ip_netmask_validate(&aprefix, plen, family, YES /*force*/);
        ip_netmask_validate(&bprefix, plen, family, YES /*force*/);

        return is_ip_equal(&aprefix, &bprefix);
}




IDM_T is_ip_set(const IPX_T *ip)
{
        return (ip && !is_ip_equal(ip, &ZERO_IP));
}

IDM_T is_ip_forbidden( const IPX_T *ip, const uint8_t family )
{
	TRACE_FUNCTION_CALL;

        if (family == AF_INET6 ) {

                if (!is_ip_equal(ip, &IP6_LOOPBACK_ADDR)) {

                        return NO;
                }

        } else if (family == AF_INET ) {

                if (ipXto4(*ip) != INADDR_LOOPBACK &&
                        ipXto4(*ip) != INADDR_NONE) {

                        return NO;
                }
        }

        return YES;
}


IDM_T ip_netmask_validate(IPX_T *ipX, uint8_t mask, uint8_t family, uint8_t force)
{
	TRACE_FUNCTION_CALL;
        uint8_t nmask = mask;
        int i;
        IP4_T ip32 = 0, m32 = 0;

        if (nmask > ort_dat[AFINET2BMX(family)].max_prefixlen)
                goto validate_netmask_error;

        if (family == AF_INET)
                nmask += (IP6_MAX_PREFIXLEN - IP4_MAX_PREFIXLEN);

        for (i = 3; i >= 0 && nmask > 0 && i >= ((nmask - 1) / 32); i--) {

                ip32 = ipX->s6_addr32[i];

                if ( force ) {

                        if (nmask <= (i * 32) && ip32)
                                ipX->s6_addr32[i] = 0;
                        else
                                ipX->s6_addr32[i] = (ip32 & (m32 = htonl(0xFFFFFFFF << (32 - (nmask - (i * 32))))));

                } else {

                        if (nmask <= (i * 32) && ip32)
                                goto validate_netmask_error;

                        else if (ip32 != (ip32 & (m32 = htonl(0xFFFFFFFF << (32 - (nmask - (i * 32)))))))
                                goto validate_netmask_error;
                }
        }


        return SUCCESS;
validate_netmask_error:

        dbgf(DBGL_SYS, DBGT_ERR, "inconsistent network prefix %s/%d (force=%d  nmask=%d, ip32=%s m32=%s)",
                ipXAsStr(family, ipX), mask, force, nmask, ip4AsStr(ip32), ip4AsStr(m32));

        return FAILURE;

}


STATIC_FUNC
struct dev_node * dev_get_by_name(char *name)
{
        IFNAME_T key = ZERO_IFNAME;

        strcpy(key.str, name);

        return avl_find_item(&dev_name_tree, &key);
}


STATIC_FUNC
IDM_T kernel_if_fix(IDM_T purge_all, uint16_t curr_sqn)
{
	TRACE_FUNCTION_CALL;
        uint16_t changed = 0;
        struct if_link_node *iln;
        int index = 0;

        while ((iln = avl_next_item(&if_link_tree, &index))) {

                index = iln->index;
                IPX_T addr = ZERO_IP;
                struct if_addr_node *ian;

                while ((ian = avl_next_item(&iln->if_addr_tree, &addr))) {

                        addr = ian->ip_addr;

                        if ( purge_all || curr_sqn != ian->update_sqn) {

                                dbgf(terminating || initializing ? DBGL_ALL : DBGL_SYS, DBGT_WARN,
                                        "addr index %d %s addr %s REMOVED",
                                        iln->index, ian->label.str, ipXAsStr(ian->ifa.ifa_family, &ian->ip_addr));

                                if (ian->dev) {
                                        ian->dev->hard_conf_changed = YES;
                                        ian->dev->if_llocal_addr = NULL;
                                        ian->dev->if_global_addr = NULL;
                                }

                                avl_remove(&iln->if_addr_tree, &addr, -300236);
                                debugFree(ian, -300237);
                                changed++;

                                continue;

                        } else {

                                changed += ian->changed;

                        }
                }

                if (purge_all || curr_sqn != iln->update_sqn) {

                        assertion(-500565, (!iln->if_addr_tree.items));

                        dbgf(terminating || initializing ? DBGL_ALL : DBGL_SYS, DBGT_WARN,
                                "link index %d %s addr %s REMOVED",
                                iln->index, iln->name.str, memAsStr(&iln->addr, iln->alen));

                        avl_remove(&if_link_tree, &iln->index, -300232);
                        avl_remove(&if_link_tree, &iln->name, -300234);
                        debugFree(iln, -300230);
                        changed++;


                } else if (iln->changed) {

                        struct dev_node *dev = dev_get_by_name(iln->name.str);

                        if (dev)
                                dev->hard_conf_changed = YES;

                        changed += iln->changed;

                }
        }

        if (changed) {

                dbgf(DBGL_SYS, DBGT_WARN, "network configuration CHANGED");
                return YES;
        
        } else {

                dbgf_all(DBGT_INFO, "network configuration UNCHANGED");
                return NO;
        }
}

STATIC_INLINE_FUNC
void kernel_if_addr_config(struct nlmsghdr *nlhdr, uint16_t index_sqn)
{

        int len = nlhdr->nlmsg_len;
        struct ifaddrmsg *if_addr = NLMSG_DATA(nlhdr);
        int index = if_addr->ifa_index;
        int family = if_addr->ifa_family;
        struct if_link_node *iln = avl_find_item(&if_link_tree, &index);

        if (!iln)
                return;

        if (family != AF_INET && family != AF_INET6)
                return;

        if (family != af_cfg)
                return;

        if (nlhdr->nlmsg_type != RTM_NEWADDR)
                return;

        if (len < (int) NLMSG_LENGTH(sizeof (if_addr)))
                return;


        len -= NLMSG_LENGTH(sizeof (*if_addr));
        if (len < 0) {
                dbgf(DBGL_SYS, DBGT_ERR, "BUG: wrong nlmsg len %d", len);
                return;
        }

        struct rtattr * rta_tb[IFA_MAX + 1];

        parse_rtattr(rta_tb, IFA_MAX, IFA_RTA(if_addr), nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof (*if_addr)));


        if (!rta_tb[IFA_LOCAL])
                rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];

        if (!rta_tb[IFA_LOCAL] || !if_addr)
                return;

        IPX_T ip_addr = ZERO_IP;

        uint32_t alen = MIN(sizeof (ip_addr), RTA_PAYLOAD(rta_tb[IFA_LOCAL]));

        memcpy(&ip_addr, RTA_DATA(rta_tb[IFA_LOCAL]), alen);

        if (family == AF_INET)
                ip42X(&ip_addr, *((IP4_T*) (&ip_addr)));

        if (is_ip_forbidden(&ip_addr, family)) // specially catch loopback ::1/128
                return;



        struct if_addr_node *new_ian = NULL;
        struct if_addr_node *old_ian = avl_find_item(&iln->if_addr_tree, &ip_addr);

        if (old_ian) {

                old_ian->changed = 0;

                if (old_ian->update_sqn == index_sqn) {
                        dbgf(DBGL_SYS, DBGT_ERR,
                                "ifi %d addr %s found several times!",
                                iln->index, ipXAsStr(old_ian->ifa.ifa_family, &ip_addr));
                }

                if (nlhdr->nlmsg_len > old_ian->nlmsghdr->nlmsg_len) {

                        if (old_ian->dev) {
                                old_ian->dev->hard_conf_changed = YES;
                                old_ian->dev->if_llocal_addr = NULL;
                                old_ian->dev->if_global_addr = NULL;
                        }

                        avl_remove(&iln->if_addr_tree, &ip_addr, -300239);
                        dbgf(DBGL_SYS, DBGT_ERR, "new size");

                } else if (memcmp(nlhdr, old_ian->nlmsghdr, nlhdr->nlmsg_len)) {

                        if (nlhdr->nlmsg_len != old_ian->nlmsghdr->nlmsg_len) {
                                dbgf(DBGL_SYS, DBGT_ERR,
                                        "different data and size %d != %d",
                                        nlhdr->nlmsg_len, old_ian->nlmsghdr->nlmsg_len);
                        }

                        memcpy(old_ian->nlmsghdr, nlhdr, nlhdr->nlmsg_len);
                        new_ian = old_ian;

                } else {
                        new_ian = old_ian;
                }

        }

        if (!new_ian) {
                new_ian = debugMalloc(sizeof (struct if_addr_node) +nlhdr->nlmsg_len, -300234);
                memset(new_ian, 0, sizeof (struct if_addr_node) +nlhdr->nlmsg_len);
                memcpy(new_ian->nlmsghdr, nlhdr, nlhdr->nlmsg_len);
                new_ian->ip_addr = ip_addr;
                new_ian->iln = iln;
                avl_insert(&iln->if_addr_tree, new_ian, -300238);
        }

        IFNAME_T label = {
                {0}};
        IPX_T ip_mcast = ZERO_IP;

        if (rta_tb[IFA_LABEL])
                strcpy(label.str, (char*) RTA_DATA(rta_tb[IFA_LABEL]));
        else
                label = iln->name;


        if (family == AF_INET && rta_tb[IFA_BROADCAST]) {

                memcpy(&ip_mcast, RTA_DATA(rta_tb[IFA_BROADCAST]), alen);
                ip42X(&ip_mcast, *((IP4_T*) (&ip_mcast)));

        } else if (family == AF_INET6) {

                ip_mcast = IP6_ALLROUTERS_MC_ADDR;
        }


        if (!old_ian ||
                old_ian->ifa.ifa_family != if_addr->ifa_family ||
                old_ian->ifa.ifa_flags != if_addr->ifa_flags ||
                old_ian->ifa.ifa_prefixlen != if_addr->ifa_prefixlen ||
                old_ian->ifa.ifa_scope != if_addr->ifa_scope ||
                old_ian->ifa.ifa_index != if_addr->ifa_index ||
                memcmp(&old_ian->label, &label, sizeof (label)) ||
                //                                                memcmp(&old_ian->ip_any, &ip_any, alen) ||
                memcmp(&old_ian->ip_mcast, &ip_mcast, alen)
                ) {

                dbgf(DBGL_CHANGES,
                        (initializing || terminating) ? DBGT_INFO : DBGT_WARN,
                        "%s addr %s CHANGED", label.str, ipXAsStr(family, &ip_addr));

                if (new_ian->dev) {
                        new_ian->dev->hard_conf_changed = YES;
                        new_ian->dev->if_llocal_addr = NULL;
                        new_ian->dev->if_global_addr = NULL;
                }


                new_ian->changed++;
        }

        new_ian->ifa.ifa_family = if_addr->ifa_family;
        new_ian->ifa.ifa_flags = if_addr->ifa_flags;
        new_ian->ifa.ifa_prefixlen = if_addr->ifa_prefixlen;
        new_ian->ifa.ifa_scope = if_addr->ifa_scope;
        new_ian->ifa.ifa_index = if_addr->ifa_index;

        new_ian->label = label;
        new_ian->ip_mcast = ip_mcast;

        new_ian->update_sqn = index_sqn;

        if (old_ian && old_ian != new_ian)
                debugFree(old_ian, -300240);

}


STATIC_FUNC
int kernel_if_link_config(struct nlmsghdr *nlhdr, uint16_t update_sqn)
{
	TRACE_FUNCTION_CALL;

	struct ifinfomsg *if_link_info = NLMSG_DATA(nlhdr);
	//struct idxmap *im, **imp;
	struct rtattr *tb[IFLA_MAX+1];

        uint16_t changed = 0;

	if (nlhdr->nlmsg_type != RTM_NEWLINK)
		return 0;

	if (nlhdr->nlmsg_len < NLMSG_LENGTH(sizeof(if_link_info)))
		return -1;

	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(if_link_info), IFLA_PAYLOAD(nlhdr));

        if (!tb[IFLA_IFNAME])
                return 0;

        int index = if_link_info->ifi_index;
        struct if_link_node *new_ilx = NULL, *old_ilx = avl_find_item(&if_link_tree, &index);

        if (old_ilx) {

                if (old_ilx->update_sqn == update_sqn) {
                        dbgf(DBGL_SYS, DBGT_ERR, "ifi %d found several times!", old_ilx->index);
                }

                assertion(-500902, (nlhdr->nlmsg_len >= sizeof (struct nlmsghdr)));

                if (nlhdr->nlmsg_len > old_ilx->nlmsghdr->nlmsg_len) {

                        avl_remove(&if_link_tree, &index, -300240);
                        dbgf(DBGL_SYS, DBGT_ERR, "newand  larger nlmsg_len");

                } else if (memcmp(nlhdr, old_ilx->nlmsghdr, nlhdr->nlmsg_len)) {

                        if (nlhdr->nlmsg_len != old_ilx->nlmsghdr->nlmsg_len) {
                                dbgf(DBGL_SYS, DBGT_ERR,
                                        "different data and size %d != %d",
                                        nlhdr->nlmsg_len, old_ilx->nlmsghdr->nlmsg_len);
                        }
                        memcpy(old_ilx->nlmsghdr, nlhdr, nlhdr->nlmsg_len);
                        new_ilx = old_ilx;
                
                } else {
                        new_ilx = old_ilx;
                }
        }

        if (!new_ilx) {
                new_ilx = debugMalloc(sizeof (struct if_link_node) + nlhdr->nlmsg_len, -300231);
                memset(new_ilx, 0, sizeof (struct if_link_node));
                new_ilx->index = if_link_info->ifi_index;
                AVL_INIT_TREE(new_ilx->if_addr_tree, struct if_addr_node, ip_addr);
                avl_insert(&if_link_tree, new_ilx, -300233);
                memcpy(new_ilx->nlmsghdr, nlhdr, nlhdr->nlmsg_len);
        }

        IFNAME_T devname = {{0}};
        assertion(-500570, (is_zero(&devname, sizeof(devname))));
        strcpy(devname.str, RTA_DATA(tb[IFLA_IFNAME]));

        int32_t alen = (tb[IFLA_ADDRESS]) ? RTA_PAYLOAD(tb[IFLA_ADDRESS]) : 0;
        ADDR_T addr = {{0}};
        memcpy(&addr, RTA_DATA(tb[IFLA_ADDRESS]), MIN(alen, (int)sizeof (addr)));

        if (!old_ilx ||
                old_ilx->type != if_link_info->ifi_type ||
                old_ilx->flags != if_link_info->ifi_flags ||
                old_ilx->alen != alen /*(int)RTA_PAYLOAD(tb[IFLA_ADDRESS])*/ ||
                memcmp(&old_ilx->addr, RTA_DATA(tb[IFLA_ADDRESS]), MIN(alen, (int)sizeof(old_ilx->addr))) ||
                memcmp(&old_ilx->name, &devname, sizeof (devname))) {

                dbgf(DBGL_SYS, (initializing || terminating) ? DBGT_INFO : DBGT_WARN,
                        "link %s addr %s CHANGED", devname.str, memAsStr(RTA_DATA(tb[IFLA_ADDRESS]), alen));

                changed++;
        }

        new_ilx->type = if_link_info->ifi_type;
        new_ilx->flags = if_link_info->ifi_flags;

        new_ilx->addr = addr;
        new_ilx->alen = alen;

        new_ilx->name = devname;

        new_ilx->update_sqn = update_sqn;
        new_ilx->changed = changed;

        if (old_ilx && old_ilx != new_ilx)
                debugFree(old_ilx, -300241);

	return 0;
}






IDM_T kernel_if_config(void)
{
	TRACE_FUNCTION_CALL;

        static uint16_t index_sqn = 0;
        int rtm_type[2] = {RTM_GETLINK, RTM_GETADDR};
        int msg_count;
        int info;

        index_sqn++;
        dbgf_all( DBGT_INFO, "%d", index_sqn);

        for (info = LINK_INFO; info <= ADDR_INFO; info++) {

                struct ip_req req;
                struct sockaddr_nl nla;
                struct iovec iov;
                struct msghdr msg = {.msg_name = &nla, .msg_namelen = sizeof (nla), .msg_iov = &iov, .msg_iovlen = 1};
                char buf[16384];

                iov.iov_base = buf;

                memset(&req, 0, sizeof (req));
                req.nlmsghdr.nlmsg_len = sizeof (req);
                req.nlmsghdr.nlmsg_type = rtm_type[info];
                req.nlmsghdr.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
                req.nlmsghdr.nlmsg_pid = 0;
                req.nlmsghdr.nlmsg_seq = ip_rth.dump = ++ip_rth.seq;
                req.rtgenmsg.rtgen_family = AF_UNSPEC;

                if (send(ip_rth.fd, (void*) & req, sizeof (req), 0) < 0) {
                        dbgf(DBGL_SYS, DBGT_ERR, "failed");
                        return FAILURE;
                }

                dbgf_all(DBGT_INFO, "send %s_INFO request", info == LINK_INFO ? "LINK" : "ADDR");

                while (1) {
                        int status;

                        iov.iov_len = sizeof (buf);

                        if ((status = recvmsg(ip_rth.fd, &msg, 0)) < 0) {

                                if (errno == EINTR || errno == EAGAIN)
                                        continue;

                                dbgf(DBGL_SYS, DBGT_ERR, "netlink receive error %s (%d)", strerror(errno), errno);
                                return FAILURE;

                        } else if (status == 0) {

                                dbgf(DBGL_SYS, DBGT_ERR, "EOF on netlink");
                                return FAILURE;
                        }

                        dbgf_all(DBGT_INFO, "rcvd %s_INFO status %d", info == LINK_INFO ? "LINK" : "ADDR", status);

                        struct nlmsghdr *nlhdr = (struct nlmsghdr*) buf;

                        msg_count = 0;

                        for (; NLMSG_OK(nlhdr, (unsigned) status); nlhdr = NLMSG_NEXT(nlhdr, status)) {

                                msg_count++;

                                if (nla.nl_pid || nlhdr->nlmsg_pid != ip_rth.local.nl_pid || nlhdr->nlmsg_seq != ip_rth.dump)
                                        continue;

                                if (nlhdr->nlmsg_type == NLMSG_DONE)
                                        break;

                                if (nlhdr->nlmsg_type == NLMSG_ERROR) {
                                        dbgf(DBGL_SYS, DBGT_ERR, "NLMSG_ERROR");
                                        return FAILURE;
                                }


                                if (info == LINK_INFO )
                                        kernel_if_link_config(nlhdr, index_sqn);
                                else
                                        kernel_if_addr_config(nlhdr, index_sqn);

                        }

                        dbgf_all(DBGT_INFO, "processed %d %s msgs", msg_count, info == LINK_INFO ? "LINK" : "ADDR");
                        
                        if (nlhdr->nlmsg_type == NLMSG_DONE)
                                        break;

                        if (msg.msg_flags & MSG_TRUNC) {
                                dbgf(DBGL_SYS, DBGT_ERR, "Message truncated");
                                continue;
                        }

                        if (status) {
                                dbgf(DBGL_SYS, DBGT_ERR, "Remnant of size %d", status);
                                return FAILURE;
                        }
                }
        }

        return kernel_if_fix(NO, index_sqn);

}


STATIC_FUNC
IDM_T iptrack(uint8_t family, uint8_t cmd, uint8_t quiet, int8_t del, IPX_T *net, uint8_t mask, uint16_t table, uint32_t prio, IFNAME_T *iif)
{
	TRACE_FUNCTION_CALL;
        assertion(-500628, (cmd != IP_NOP));
        assertion(-500629, (del || (cmd != IP_ROUTE_FLUSH && cmd != IP_RULE_FLUSH)));

        IDM_T flush = (cmd == IP_RULE_FLUSH || cmd == IP_ROUTE_FLUSH_ALL || cmd == IP_ROUTE_FLUSH);

        uint8_t cmd_t = (cmd > IP_ROUTES && cmd < IP_ROUTE_MAX) ? IP_ROUTES :
                ((cmd > IP_RULES && cmd < IP_RULE_MAX) ? IP_RULES : IP_NOP);

        struct track_key sk;
        memset(&sk, 0, sizeof (sk));
        sk.net = net ? *net : ZERO_IP;
        sk.iif = iif ? *iif : ZERO_IFNAME;
        sk.prio = prio;
        sk.table = table;
        sk.family = family;
        sk.mask = mask;
        sk.cmd_type = cmd_t;

        int found = 0, exact = 0;
        struct track_node *first_tn = NULL;
        struct avl_node *an = avl_find(&iptrack_tree, &sk);
        struct track_node *tn = an ? an->item : NULL;

        while (tn) {

                if (!first_tn && (tn->cmd == cmd || flush))
                        first_tn = tn;

                if (tn->cmd == cmd) {
                        assertion(-500887, (exact == 0));
                        exact += tn->items;
                }

                found += tn->items;

                tn = (tn = avl_iterate_item(&iptrack_tree, &an)) && !memcmp(&sk, &tn->k, sizeof (sk)) ? tn : NULL;
        }

        if (flush || (del && !first_tn) || (del && found != 1) || (!del && found > 0)) {

                dbgf(
                        quiet ? DBGL_ALL  : (flush || (del && !first_tn)) ? DBGL_SYS : DBGL_CHANGES,
                        quiet ? DBGT_INFO : (flush || (del && !first_tn)) ? DBGT_ERR : DBGT_INFO,
                        "   %s %s %s/%d  table %d  prio %d  dev %s exists %d tims with %d exact match",
                        del2str(del), trackt2str(cmd), ipXAsStr(family, net), mask, table, prio,
                        iif ? iif->str : NULL, found, exact);

                EXITERROR(-500700, (!(!quiet && (flush || (del && !first_tn)))));
        }

        if (flush)
                return YES;

        if (del) {

                if (!first_tn) {
                        
                        dbgf( DBGL_SYS, DBGT_ERR, " ");
                        assertion(-500883, (0));

                } else if (first_tn->items == 1) {

                        struct track_node *rem_tn = avl_remove(&iptrack_tree, &first_tn->k, -300250);
                        assertion(-500882, (rem_tn == first_tn));
                        debugFree(first_tn, -300072);

                } else if (first_tn->items > 1) {

                        first_tn->items--;
                }

                if ( found != 1 )
                        return NO;

	} else {

                if (exact) {

                        assertion(-500886, (first_tn));
                        assertion(-500884, (!memcmp(&sk, &first_tn->k, sizeof (sk))));
                        assertion(-500885, (first_tn->cmd == cmd));

                        first_tn->items++;

                } else {

                        struct track_node *tn = debugMalloc(sizeof ( struct track_node), -300030);
                        memset(tn, 0, sizeof ( struct track_node));
                        tn->k = sk;
                        tn->items = 1;
                        tn->cmd = cmd;

                        avl_insert(&iptrack_tree, tn, -300251);
                }

                if (found > 0)
                        return NO;

	}

        return YES;
}


STATIC_FUNC
void add_rtattr(struct rtmsg_req *req, int rta_type, char *data, uint16_t data_len, uint16_t family)
{
	TRACE_FUNCTION_CALL;
        IP4_T ip4;
        if (family == AF_INET) {
                ip4 = ipXto4((*((IPX_T*)data)));
                data_len = sizeof (ip4);
                data = (char*) & ip4;
        }

        struct rtattr *rta = (struct rtattr *)(((char *) req) + NLMSG_ALIGN(req->nlh.nlmsg_len));

        req->nlh.nlmsg_len = NLMSG_ALIGN( req->nlh.nlmsg_len ) + RTA_LENGTH(data_len);

        assertion(-50173, (NLMSG_ALIGN(req->nlh.nlmsg_len) < sizeof ( struct rtmsg_req)));
        // if this fails then double req buff size !!

        rta->rta_type = rta_type;
        rta->rta_len = RTA_LENGTH(data_len);
        memcpy( RTA_DATA(rta), data, data_len );
}


IDM_T ip(uint8_t family, uint8_t cmd, int8_t del, uint8_t quiet, const IPX_T *NET, uint8_t nmask,
        int32_t table_macro, uint32_t prio, IFNAME_T *iifname, int oif_idx, IPX_T *via, IPX_T *src)
{
	TRACE_FUNCTION_CALL;

        assertion(-500653, (family == af_cfg && (family == AF_INET || family == AF_INET6)));
        assertion(-500650, ((NET && !is_ip_forbidden(NET, family))));
        assertion(-500651, (!(via && is_ip_forbidden(via, family))));
        assertion(-500652, (!(src && is_ip_forbidden(src, family))));

	struct sockaddr_nl nladdr;
	struct nlmsghdr *nh;
        struct rtmsg_req req;

        uint16_t table = rt_macro_to_table(table_macro);

        IPX_T net_dummy = *NET;
        IPX_T *net = &net_dummy;

        uint8_t more_data;

        int nlsock = 0;

        if ( cmd == IP_ROUTE_FLUSH_ALL )
                nlsock = nlsock_flush_all;
        else
                nlsock = nlsock_default;
        
        assertion(-500672, (ip_netmask_validate(net, nmask, family, NO) == SUCCESS));


        IDM_T llocal = (via && is_ip_equal(via, net)) ? YES : NO;

        if (!is_ip_set(src))
                src = NULL;

        if (!throw_rules && (cmd == IP_THROW_MY_HNA || cmd == IP_THROW_MY_NET))
		return SUCCESS;;

        if (iptrack(family, cmd, quiet, del,net, nmask, table, prio, iifname) == NO)
                return SUCCESS;


#ifndef NO_DEBUG_ALL
        struct if_link_node *oif_iln = oif_idx ? avl_find_item(&if_link_tree, &oif_idx) : NULL;

        dbgf(DBGL_ALL, DBGT_INFO, "%s %s %s %s/%-2d  iif %s  table %d  prio %d  oif %s  via %s  src %s (using nl_sock %d)",
                family2Str(family), trackt2str(cmd), del2str(del), ipXAsStr(family, net), nmask,
                iifname ? iifname->str : NULL, table, prio, oif_iln ? oif_iln->name.str : "---",
                via ? ipXAsStr(family, via) : "---", src ? ipXAsStr(family, src) : "---", nlsock);
#endif

        memset(&nladdr, 0, sizeof (struct sockaddr_nl));
        memset(&req, 0, sizeof (req));

	nladdr.nl_family = AF_NETLINK;

        req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.nlh.nlmsg_pid = My_pid;

        req.rtm.rtm_family = family;
        req.rtm.rtm_table = table;

        req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;

        if (cmd > IP_ROUTES && cmd < IP_ROUTE_MAX) {

                if ( cmd == IP_ROUTE_FLUSH_ALL ) {

                        req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
                        req.nlh.nlmsg_type = RTM_GETROUTE;
                        req.rtm.rtm_scope = RTN_UNICAST;

                } else if (del) {
                        req.nlh.nlmsg_type = RTM_DELROUTE;
                        req.rtm.rtm_scope = RT_SCOPE_NOWHERE;
                } else {
                        req.nlh.nlmsg_flags = req.nlh.nlmsg_flags | NLM_F_CREATE | NLM_F_EXCL; //| NLM_F_REPLACE;
                        req.nlh.nlmsg_type = RTM_NEWROUTE;
                        req.rtm.rtm_scope = ((cmd == IP_ROUTE_HNA || cmd == IP_ROUTE_HOST) && llocal) ? RT_SCOPE_LINK : RT_SCOPE_UNIVERSE;
                        req.rtm.rtm_protocol = RTPROT_STATIC;
                        req.rtm.rtm_type = (cmd == IP_THROW_MY_HNA || cmd == IP_THROW_MY_NET) ? RTN_THROW : RTN_UNICAST;
                }

                if (is_ip_set(net)) {
                        req.rtm.rtm_dst_len = nmask;
                        add_rtattr(&req, RTA_DST, (char*) net, sizeof (IPX_T), family);
                }

                if (via && !llocal)
                        add_rtattr(&req, RTA_GATEWAY, (char*) via, sizeof (IPX_T), family);

                if (oif_idx)
                        add_rtattr(&req, RTA_OIF, (char*) & oif_idx, sizeof (oif_idx), 0);

                if (src)
                        add_rtattr(&req, RTA_PREFSRC, (char*) src, sizeof (IPX_T), family);



        } else if (cmd > IP_RULES && cmd < IP_RULE_MAX) {

                if (del) {
                        req.nlh.nlmsg_type = RTM_DELRULE;
                        req.rtm.rtm_scope = RT_SCOPE_NOWHERE;
                } else {
                        req.nlh.nlmsg_flags = req.nlh.nlmsg_flags | NLM_F_CREATE | NLM_F_EXCL;
                        req.nlh.nlmsg_type = RTM_NEWRULE;
                        req.rtm.rtm_scope = RT_SCOPE_UNIVERSE;
                        req.rtm.rtm_protocol = RTPROT_STATIC;
                        req.rtm.rtm_type = RTN_UNICAST;
                }

                if (is_ip_set(net)) {
                        req.rtm.rtm_src_len = nmask;
                        add_rtattr(&req, RTA_SRC, (char*) net, sizeof (IPX_T), family);
                }

                if (iifname)
                        add_rtattr(&req, RTA_IIF, iifname->str, strlen(iifname->str) + 1, 0);

        } else {

                cleanup_all(-500628);
        }


        if (prio)
                add_rtattr(&req, RTA_PRIORITY, (char*) & prio, sizeof (prio), 0);

        errno = 0;

        if (sendto(nlsock, &req, req.nlh.nlmsg_len, 0, (struct sockaddr *) & nladdr, sizeof (struct sockaddr_nl)) < 0) {

                dbg(DBGL_SYS, DBGT_ERR, "can't send netlink message to kernel: %s", strerror(errno));
		return FAILURE;
        }

        int max_retries = 10;

        //TODO: see ip/libnetlink.c rtnl_talk() for HOWTO
        while (1) {
                struct msghdr msg;
                char buf[4096]; // less causes lost messages !!??
                memset(&msg, 0, sizeof (struct msghdr));

                memset(&nladdr, 0, sizeof (struct sockaddr_nl));
                nladdr.nl_family = AF_NETLINK;

                memset(buf, 0, sizeof(buf));
                struct iovec iov = {.iov_base = buf, .iov_len = sizeof (buf)};

		msg.msg_name = (void *)&nladdr;
		msg.msg_namelen = sizeof(nladdr);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = NULL;

		errno=0;
		int status = recvmsg( nlsock, &msg, 0 );

                more_data = NO;

		if ( status < 0 ) {

			if ( errno == EINTR ) {

                                dbgf(DBGL_SYS, DBGT_WARN, "(EINTR) %s", strerror(errno));

                        } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
                                
                                dbgf(DBGL_SYS, DBGT_ERR, "(EWOULDBLOCK || EAGAIN) %s", strerror(errno));

                        } else {

                                dbgf(DBGL_SYS, DBGT_ERR, "%s", strerror(errno));
                        }

                        if ( max_retries-- > 0 ) {
                                usleep(500);
                                continue;
                        } else {
                                dbgf(DBGL_SYS, DBGT_ERR, "giving up!");
                                return FAILURE;
                        }

		} else if (status == 0) {
                        dbgf(DBGL_SYS, DBGT_ERR, "netlink EOF");
                        return FAILURE;
                }

                if (msg.msg_flags & MSG_TRUNC) {
                        dbgf(DBGL_CHANGES, DBGT_INFO, "MSG_TRUNC");
                        more_data = YES;
                }

		nh = (struct nlmsghdr *)buf;

		while ( NLMSG_OK(nh, (size_t)status) ) {

                        if (nh->nlmsg_flags & NLM_F_MULTI) {
                                dbgf(DBGL_ALL, DBGT_INFO, "NLM_F_MULTI");
                                more_data = YES;
                        }

                        if (nh->nlmsg_type == NLMSG_DONE) {
                                dbgf(DBGL_CHANGES, DBGT_INFO, "NLMSG_DONE");
                                more_data = NO;
                                break;

                        } else if ((nh->nlmsg_type == NLMSG_ERROR) && (((struct nlmsgerr*) NLMSG_DATA(nh))->error != 0)) {

                                dbg(quiet ? DBGL_ALL : DBGL_SYS, quiet ? DBGT_INFO : DBGT_ERR,
                                        "can't %s %s to %s/%i via %s table %i: %s",
                                        del2str(del), trackt2str(cmd), ipXAsStr(family, net), nmask, ipXAsStr(family, via),
                                        table, strerror(-((struct nlmsgerr*) NLMSG_DATA(nh))->error));

                                return FAILURE;
                        }

                        if (cmd == IP_ROUTE_FLUSH_ALL) {

                                struct rtmsg *rtm = (struct rtmsg *) NLMSG_DATA(nh);
                                struct rtattr *rtap = (struct rtattr *) RTM_RTA(rtm);
                                int rtl = RTM_PAYLOAD(nh);

                                while (rtm->rtm_table == table && RTA_OK(rtap, rtl)) {

                                        if (rtap->rta_type == RTA_DST) {


                                                IPX_T fip;

                                                if (family == AF_INET6)
                                                        fip = *((IPX_T *) RTA_DATA(rtap));
                                                else
                                                        ip42X(&fip, *((IP4_T *) RTA_DATA(rtap)));


                                                ip(family, IP_ROUTE_FLUSH, DEL, YES, &fip, rtm->rtm_dst_len, table, 0, NULL, 0, NULL, NULL);

                                        }

                                        rtap = RTA_NEXT(rtap, rtl);
                                }
                        }

                        nh = NLMSG_NEXT(nh, status);
		}

                if ( more_data ) {
                        dbgf(DBGL_CHANGES, DBGT_INFO, "more data via netlink socket %d...", nlsock);
                } else {
                        break;
                }
        }

        return SUCCESS;
}






STATIC_FUNC
void check_proc_sys_net(char *file, int32_t desired, int32_t *backup)
{
	TRACE_FUNCTION_CALL;
        FILE *f;
	int32_t state = 0;
	char filename[MAX_PATH_SIZE];
	int trash;


	sprintf( filename, "/proc/sys/net/%s", file );

	if((f = fopen(filename, "r" )) == NULL) {

		dbgf( DBGL_SYS, DBGT_ERR, "can't open %s for reading! retry later..", filename );

		return;
	}

	trash=fscanf(f, "%d", &state);
	fclose(f);

	if ( backup )
		*backup = state;

	// other routing protocols are probably not able to handle this therefore
	// it is probably better to leave the routing configuration operational as it is!
	if ( !backup  &&  !Pedantic_cleanup   &&  state != desired ) {

		dbg_mute( 50, DBGL_SYS, DBGT_INFO,
		          "NOT restoring %s to NOT mess up other routing protocols. "
		          "Use --%s=1 to enforce proper cleanup",
		          file, ARG_PEDANTIC_CLEANUP );

		return;
	}


	if ( state != desired ) {

		dbg( DBGL_SYS, DBGT_INFO, "changing %s from %d to %d", filename, state, desired );

		if((f = fopen(filename, "w" )) == NULL) {

			dbgf( DBGL_SYS, DBGT_ERR,
			      "can't open %s for writing! retry later...", filename );
			return;
		}

		fprintf(f, "%d", desired?1:0 );
		fclose(f);

	}
}

void sysctl_restore(struct dev_node *dev)
{
        TRACE_FUNCTION_CALL;

        if (dev && af_cfg == AF_INET) {

		char filename[100];

		if (dev->ip4_rp_filter_orig > -1) {
			sprintf( filename, "ipv4/conf/%s/rp_filter", dev->name_phy_cfg.str);
			check_proc_sys_net( filename, dev->ip4_rp_filter_orig, NULL );
		}

		dev->ip4_rp_filter_orig = -1;


		if (dev->ip4_send_redirects_orig > -1) {
			sprintf( filename, "ipv4/conf/%s/send_redirects", dev->name_phy_cfg.str);
			check_proc_sys_net( filename, dev->ip4_send_redirects_orig, NULL );
		}

		dev->ip4_send_redirects_orig = -1;

        } else if (dev && af_cfg == AF_INET6) {

                // nothing to restore
        }
        
        if (!dev) {

		if( if4_rp_filter_all_orig != -1 )
			check_proc_sys_net( "ipv4/conf/all/rp_filter", if4_rp_filter_all_orig, NULL );

		if4_rp_filter_all_orig = -1;

		if( if4_rp_filter_default_orig != -1 )
			check_proc_sys_net( "ipv4/conf/default/rp_filter", if4_rp_filter_default_orig,  NULL );

		if4_rp_filter_default_orig = -1;

		if( if4_send_redirects_all_orig != -1 )
			check_proc_sys_net( "ipv4/conf/all/send_redirects", if4_send_redirects_all_orig, NULL );

		if4_send_redirects_all_orig = -1;

		if( if4_send_redirects_default_orig != -1 )
			check_proc_sys_net( "ipv4/conf/default/send_redirects", if4_send_redirects_default_orig, NULL );

		if4_send_redirects_default_orig = -1;

		if( if4_forward_orig != -1 )
			check_proc_sys_net( "ipv4/ip_forward", if4_forward_orig, NULL );

		if4_forward_orig = -1;


                if (if6_forward_orig != -1)
                        check_proc_sys_net("ipv6/conf/all/forwarding", if6_forward_orig, NULL);

                if6_forward_orig = -1;

	}
}

// TODO: check for further traps: http://lwn.net/Articles/45386/
void sysctl_config( struct dev_node *dev )
{
        TRACE_FUNCTION_CALL;
        static TIME_T ipv4_timestamp = -1;
        static TIME_T ipv6_timestamp = -1;
        char filename[100];

        if (!(dev->active && dev->if_llocal_addr && dev->if_llocal_addr->iln->flags && IFF_UP))
                return;


        if (dev && af_cfg == AF_INET) {

		sprintf( filename, "ipv4/conf/%s/rp_filter", dev->name_phy_cfg.str);
		check_proc_sys_net( filename, 0, &dev->ip4_rp_filter_orig );

		sprintf( filename, "ipv4/conf/%s/send_redirects", dev->name_phy_cfg.str);
		check_proc_sys_net( filename, 0, &dev->ip4_send_redirects_orig );

                if (ipv4_timestamp != bmx_time) {

                        check_proc_sys_net("ipv4/conf/all/rp_filter", 0, &if4_rp_filter_all_orig);
                        check_proc_sys_net("ipv4/conf/default/rp_filter", 0, &if4_rp_filter_default_orig);
                        check_proc_sys_net("ipv4/conf/all/send_redirects", 0, &if4_send_redirects_all_orig);
                        check_proc_sys_net("ipv4/conf/default/send_redirects", 0, &if4_send_redirects_default_orig);
                        check_proc_sys_net("ipv4/ip_forward", 1, &if4_forward_orig);

                        ipv4_timestamp = bmx_time;
                }

        } else if (dev && af_cfg == AF_INET6) {


                if (ipv6_timestamp != bmx_time) {

                        check_proc_sys_net("ipv6/conf/all/forwarding", 1, &if6_forward_orig);

                        ipv6_timestamp = bmx_time;
                }
        }
}





STATIC_FUNC
int8_t dev_bind_sock(int32_t sock, IFNAME_T *name)
{
	errno=0;

        if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, name->str, strlen(name->str) + 1) < 0) {
                dbg(DBGL_SYS, DBGT_ERR, "Can not bind socket to device %s : %s", name->str, strerror(errno));
                return FAILURE;
        }

        return SUCCESS;
}


STATIC_FUNC
void dev_reconfigure_soft(struct dev_node *dev)
{
        TRACE_FUNCTION_CALL;

        assertion(-500611, (dev->active));
        assertion(-500612, (dev->if_llocal_addr));
        assertion(-500613, (dev->if_global_addr || !dev->announce));
        assertion(-500614, (dev->if_global_addr || dev != primary_dev_cfg));
        assertion(-500615, (dev->if_global_addr || dev->linklayer != VAL_DEV_LL_LO));
        
        if (!initializing) {
                dbg(DBGL_SYS, DBGT_INFO, "%s soft interface configuration changed", dev->label_cfg.str);
        }

        if (dev->umetric_max_conf != OPT_CHILD_UNDEFINED)
                dev->umetric_max = dev->umetric_max_configured;
        else if (dev->linklayer == VAL_DEV_LL_WLAN)
                dev->umetric_max = DEF_DEV_BANDWIDTH_WIFI;
        else if (dev->linklayer == VAL_DEV_LL_LAN)
                dev->umetric_max = DEF_DEV_BANDWIDTH_LAN;
        else if (dev->linklayer == VAL_DEV_LL_LO)
                dev->umetric_max = umetric(0, 0, DEF_FMETRIC_EXP_OFFSET);
        else
                cleanup_all(-500821);

        dbg(bmx_time ? DBGL_CHANGES : DBGL_SYS, DBGT_INFO,
                "enabled %sprimary %s %ju %s=%s MAC: %s link-local %s/%d global %s/%d brc %s",
                dev == primary_dev_cfg ? "" : "non-",
                dev->linklayer == VAL_DEV_LL_LO ? "loopback" : (
                dev->linklayer == VAL_DEV_LL_WLAN ? "wireless" : (
                dev->linklayer == VAL_DEV_LL_LAN ? "ethernet" : ("ILLEGAL"))),
                dev->umetric_max,
                dev->arg_cfg,
                dev->label_cfg.str, macAsStr(&dev->mac),
                dev->ip_llocal_str, dev->if_llocal_addr->ifa.ifa_prefixlen,
                dev->ip_global_str, dev->if_global_addr ? dev->if_global_addr->ifa.ifa_prefixlen : 0,
                dev->ip_brc_str);


        my_description_changed = YES;

	dev->soft_conf_changed = NO;

}


STATIC_FUNC
void dev_deactivate( struct dev_node *dev )
{

        TRACE_FUNCTION_CALL;
        dbgf(DBGL_SYS, DBGT_WARN, "deactivating %s=%s %s", dev->arg_cfg, dev->label_cfg.str, dev->ip_llocal_str);

        if (dev->active) {
                dev->active = NO;
                cb_plugin_hooks(PLUGIN_CB_DEV_EVENT, dev);
        }

        if (dev == primary_dev_cfg && !terminating) {
                dbg_mute(30, DBGL_SYS, DBGT_WARN,
                        "Using an IP on the loopback device as primary interface ensures reachability under your primary IP!");
        }


	if ( dev->linklayer != VAL_DEV_LL_LO ) {
/*
                if (dev == primary_dev_cfg && !terminating) {

                        purge_link_and_orig_nodes(NULL, NO);

                        dbg_mute(30, DBGL_SYS, DBGT_WARN,
                                "Using an IP on the loopback interface as primary IP ensures reachability under your primary IP!");
                } else {
                        purge_link_and_orig_nodes(dev, NO);
                }
*/
                purge_link_and_orig_nodes(dev, NO);

                purge_tx_task_list(dev->tx_task_lists, NULL, NULL);

                struct avl_node *an;
                struct link_dev_node *lndev;
                for (an = NULL; (lndev = avl_iterate_item(&link_dev_tree, &an));) {
                        purge_tx_task_list(lndev->tx_task_lists, NULL, dev);
                }


		if (dev->unicast_sock != 0)
			close(dev->unicast_sock);

		dev->unicast_sock = 0;

		if (dev->rx_mcast_sock != 0)
			close(dev->rx_mcast_sock);

		dev->rx_mcast_sock = 0;

		if (dev->rx_fullbrc_sock != 0)
			close(dev->rx_fullbrc_sock);

		dev->rx_fullbrc_sock = 0;
        }


        if (!is_ip_set(&dev->llocal_ip_key)) {
                dbgf(DBGL_SYS, DBGT_ERR, "no address given to remove in dev_ip_tree!");
        } else if (!avl_find(&dev_ip_tree, &dev->llocal_ip_key)) {
                dbgf(DBGL_SYS, DBGT_ERR, "%s not in dev_ip_tree!", ipXAsStr(af_cfg, &dev->llocal_ip_key));
        } else {
                assertion(-500842, (dev->link_id));
                //avl_remove(&dev_link_id_tree, &dev->link_id, -300311);
                dev->link_id = LINK_ID_INVALID;

                avl_remove(&dev_ip_tree, &dev->llocal_ip_key, -300192);
                dev->llocal_ip_key = ZERO_IP;

                if (dev_ip_tree.items == 0)
                        remove_task(tx_packets, NULL);

        }



	sysctl_restore ( dev );

	change_selects();

	dbgf_all( DBGT_WARN, "Interface %s deactivated", dev->label_cfg.str );

        my_description_changed = YES;

}


STATIC_FUNC
void set_sockaddr_storage(struct sockaddr_storage *ss, uint32_t family, IPX_T *ipx)
{
        TRACE_FUNCTION_CALL;
        memset(ss, 0, sizeof ( struct sockaddr_storage));

        ss->ss_family = family;

        if (family == AF_INET) {
                struct sockaddr_in *in = (struct sockaddr_in*) ss;
                in->sin_port = htons(base_port);
                in->sin_addr.s_addr = ipXto4(*ipx);
        } else {
                struct sockaddr_in6 *in6 = (struct sockaddr_in6*) ss;
                in6->sin6_port = htons(base_port);
                in6->sin6_addr = *ipx;
        }
}


STATIC_FUNC
IDM_T dev_init_sockets(struct dev_node *dev)
{

        TRACE_FUNCTION_CALL;
        assertion( -500618, (dev->linklayer != VAL_DEV_LL_LO));

        int set_on = 1;
        int sock_opts;
        int pf_domain = af_cfg == AF_INET ? PF_INET : PF_INET6;

        if ((dev->unicast_sock = socket(pf_domain, SOCK_DGRAM, 0)) < 0) {

                dbg(DBGL_SYS, DBGT_ERR, "can't create send socket: %s", strerror(errno));
                return FAILURE;
        }

        set_sockaddr_storage(&dev->llocal_unicast_addr, af_cfg, &dev->if_llocal_addr->ip_addr);

        if (af_cfg == AF_INET) {
                if (setsockopt(dev->unicast_sock, SOL_SOCKET, SO_BROADCAST, &set_on, sizeof (set_on)) < 0) {
                        dbg(DBGL_SYS, DBGT_ERR, "can't enable broadcasts on unicast socket: %s",
                                strerror(errno));
                        return FAILURE;
                }
        } else {
                struct ipv6_mreq mreq6;

                mreq6.ipv6mr_multiaddr = dev->if_llocal_addr->ip_mcast;

                mreq6.ipv6mr_interface = dev->if_llocal_addr->iln->index;//0 corresponds to any interface

/*
                if (setsockopt(dev->unicast_sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &set_on, sizeof (set_on)) < 0) {
                        dbg(DBGL_SYS, DBGT_ERR, "can't set IPV6_MULTICAST_LOOP:: on unicast socket: %s",
                                strerror(errno));
                        return FAILURE;
                }
*/
                if (setsockopt(dev->unicast_sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &set_on, sizeof (set_on)) < 0) {
                        dbg(DBGL_SYS, DBGT_ERR, "can't set IPV6_MULTICAST_HOPS on unicast socket: %s",
                                strerror(errno));
                        return FAILURE;
                }

                if (setsockopt(dev->unicast_sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6, sizeof (mreq6)) < 0) {
                        dbg(DBGL_SYS, DBGT_ERR, "can't set IPV6_ADD_MEMBERSHIP on unicast socket: %s",
                                strerror(errno));
                        return FAILURE;
                }
        }

        // bind send socket to interface name
        if (dev_bind_sock(dev->unicast_sock, &dev->name_phy_cfg) < 0)
                return FAILURE;

        // bind send socket to address
        if (bind(dev->unicast_sock, (struct sockaddr *) & dev->llocal_unicast_addr, sizeof (dev->llocal_unicast_addr)) < 0) {
                dbg(DBGL_SYS, DBGT_ERR, "can't bind unicast socket: %s", strerror(errno));
                return FAILURE;
        }

        // make udp send socket non blocking
        sock_opts = fcntl(dev->unicast_sock, F_GETFL, 0);
        fcntl(dev->unicast_sock, F_SETFL, sock_opts | O_NONBLOCK);

#ifdef SO_TIMESTAMP
        if (setsockopt(dev->unicast_sock, SOL_SOCKET, SO_TIMESTAMP, &set_on, sizeof(set_on)))
                dbg( DBGL_SYS, DBGT_WARN,
                     "No SO_TIMESTAMP support, despite being defined, falling back to SIOCGSTAMP");
#else
        dbg( DBGL_SYS, DBGT_WARN, "No SO_TIMESTAMP support, falling back to SIOCGSTAMP");
#endif


        set_sockaddr_storage(&dev->tx_netwbrc_addr, af_cfg, &dev->if_llocal_addr->ip_mcast);




        // get netwbrc recv socket
        if ((dev->rx_mcast_sock = socket(pf_domain, SOCK_DGRAM, 0)) < 0) {
                dbg(DBGL_CHANGES, DBGT_ERR, "can't create network-broadcast socket: %s", strerror(errno));
                return FAILURE;
        }

        // bind recv socket to interface name
        if (dev_bind_sock(dev->rx_mcast_sock, &dev->name_phy_cfg) < 0)
                return FAILURE;


        struct sockaddr_storage rx_netwbrc_addr;

        if (af_cfg == AF_INET && ipXto4(dev->if_llocal_addr->ip_mcast) == 0xFFFFFFFF) {
                // if the mcast address is the full-broadcast address
                // we'll listen on the network-broadcast address :
                IPX_T brc;
                ip42X(&brc, ipXto4(dev->if_llocal_addr->ip_addr) | htonl(~(0XFFFFFFFF << dev->if_llocal_addr->ifa.ifa_prefixlen)));
                set_sockaddr_storage(&rx_netwbrc_addr, af_cfg, &brc);
        } else {
                set_sockaddr_storage(&rx_netwbrc_addr, af_cfg, &dev->if_llocal_addr->ip_mcast);
        }


        if (bind(dev->rx_mcast_sock, (struct sockaddr *) & rx_netwbrc_addr, sizeof (rx_netwbrc_addr)) < 0) {
                char ip6[IP6_ADDR_LEN];

                if (af_cfg == AF_INET)
                        inet_ntop(af_cfg, &((struct sockaddr_in*) (&rx_netwbrc_addr))->sin_addr, ip6, sizeof (ip6));
                else
                        inet_ntop(af_cfg, &((struct sockaddr_in6*) (&rx_netwbrc_addr))->sin6_addr, ip6, sizeof (ip6));

                dbg(DBGL_CHANGES, DBGT_ERR, "can't bind network-broadcast socket to %s: %s",
                        ip6, strerror(errno));
                return FAILURE;
        }

        if (af_cfg == AF_INET) {
                // we'll always listen on the full-broadcast address
                struct sockaddr_storage rx_fullbrc_addr;
                IPX_T brc;
                ip42X(&brc, 0XFFFFFFFF);
                set_sockaddr_storage(&rx_fullbrc_addr, af_cfg, &brc);

                // get fullbrc recv socket
                if ((dev->rx_fullbrc_sock = socket(pf_domain, SOCK_DGRAM, 0)) < 0) {
                        dbg(DBGL_CHANGES, DBGT_ERR, "can't create full-broadcast socket: %s",
                                strerror(errno));
                        return FAILURE;
                }

                // bind recv socket to interface name
                if (dev_bind_sock(dev->rx_fullbrc_sock, &dev->name_phy_cfg) < 0)
                        return FAILURE;


                // bind recv socket to address
                if (bind(dev->rx_fullbrc_sock, (struct sockaddr *) & rx_fullbrc_addr, sizeof (rx_fullbrc_addr)) < 0) {
                        dbg(DBGL_CHANGES, DBGT_ERR, "can't bind full-broadcast socket: %s", strerror(errno));
                        return FAILURE;
                }

        }

        return SUCCESS;
}


STATIC_FUNC
void dev_activate( struct dev_node *dev )
{
        TRACE_FUNCTION_CALL;

        assertion(-500575, (dev && !dev->active && dev->if_llocal_addr && dev->if_llocal_addr->iln->flags & IFF_UP));
        assertion(-500593, (af_cfg == dev->if_llocal_addr->ifa.ifa_family));
        assertion(-500599, (is_ip_set(&dev->if_llocal_addr->ip_addr) && dev->if_llocal_addr->ifa.ifa_prefixlen));


	if ( wordsEqual( "lo", dev->name_phy_cfg.str ) ) {

		dev->linklayer = VAL_DEV_LL_LO;

                if (!dev->if_global_addr) {
                        dbg_mute(30, DBGL_SYS, DBGT_WARN, "loopback dev %s should be given with global address",
                                dev->label_cfg.str);

                        cleanup_all(-500621);
                }

                uint8_t prefixlen = ((af_cfg == AF_INET) ? IP4_MAX_PREFIXLEN : IP6_MAX_PREFIXLEN);

                if (!dev->if_global_addr || dev->if_global_addr->ifa.ifa_prefixlen != prefixlen) {
                        dbg_mute(30, DBGL_SYS, DBGT_WARN,
                                "prefix length of loopback interface is %d but SHOULD BE %d and global",
                                dev->if_global_addr->ifa.ifa_prefixlen, prefixlen);
                }


	} else {

                if (dev->linklayer_conf != OPT_CHILD_UNDEFINED) {

                        dev->linklayer = dev->linklayer_conf;

                } else /* check if interface is a wireless interface */ {

                        struct ifreq int_req;

                        if (get_if_req(&dev->name_phy_cfg, &int_req, SIOCGIWNAME) == SUCCESS)
                                dev->linklayer = VAL_DEV_LL_WLAN;

                        else
                                dev->linklayer = VAL_DEV_LL_LAN;

                }
        }



        if (dev->linklayer != VAL_DEV_LL_LO) {

                if (dev_init_sockets(dev) == FAILURE)
                        goto error;

                sysctl_config(dev);

                // from here on, nothing should fail anymore !!:

                dev->mac = *((MAC_T*)&(dev->if_llocal_addr->iln->addr));

        }


        if (new_link_id(dev) == LINK_ID_INVALID)
                goto error;

        assertion(-500592, (!avl_find(&dev_ip_tree, &dev->if_llocal_addr->ip_addr)));
        dev->llocal_ip_key = dev->if_llocal_addr->ip_addr;
        avl_insert(&dev_ip_tree, dev, -300151);


        ip2Str(af_cfg, &dev->if_llocal_addr->ip_addr, dev->ip_llocal_str);

        if ( dev->if_global_addr)
                ip2Str(af_cfg, &dev->if_global_addr->ip_addr, dev->ip_global_str);

        ip2Str(af_cfg, &dev->if_llocal_addr->ip_mcast, dev->ip_brc_str);

        if (dev == primary_dev_cfg) {
                self.primary_ip = dev->if_global_addr->ip_addr;
                ip2Str(af_cfg, &dev->if_global_addr->ip_addr, self.primary_ip_str);
        }

        dev->active = YES;

        assertion(-500595, (primary_dev_cfg));

        if (!(dev->link_hello_sqn )) {
                dev->link_hello_sqn = ((HELLO_SQN_MASK) & rand_num(HELLO_SQN_MAX));
        }

        int i;
        for (i = 0; i < FRAME_TYPE_ARRSZ; i++) {
                LIST_INIT_HEAD(dev->tx_task_lists[i], struct tx_task_node, list);
        }

        AVL_INIT_TREE(dev->tx_task_tree, struct tx_task_node, content);

        if (dev_ip_tree.items == 1)
                register_task(rand_num(bmx_time ? 0 : DEF_TX_DELAY), tx_packets, NULL);




        dev->soft_conf_changed = YES;

	//activate selector for active interfaces
	change_selects();

	//trigger plugins interested in changed interface configuration
        cb_plugin_hooks(PLUGIN_CB_DEV_EVENT, dev);
        cb_plugin_hooks(PLUGIN_CB_CONF, NULL);

	return;

error:
        dbgf(DBGL_SYS, DBGT_ERR, "error intitializing %s=%s", dev->arg_cfg, dev->label_cfg.str);

        ip2Str(af_cfg, &ZERO_IP, dev->ip_llocal_str);
        ip2Str(af_cfg, &ZERO_IP, dev->ip_brc_str);

        dev_deactivate(dev);
}


STATIC_FUNC
void ip_flush_routes( void )
{
	TRACE_FUNCTION_CALL;
        uint16_t table;

        if (primary_dev_cfg) {

                for (table = Rt_table; table <= Rt_table + RT_TABLE_MAX_OFFS; table++) {

                        ip(af_cfg, IP_ROUTE_FLUSH_ALL, DEL, YES/*quiet*/, &ZERO_IP, 0, table,
                                0, NULL, 0, NULL, NULL);
                }
        }
}

STATIC_FUNC
void ip_flush_rules(void)
{
	TRACE_FUNCTION_CALL;
        uint16_t table;

        if (!af_cfg)
                return;


        for (table = Rt_table; table <= Rt_table + RT_TABLE_MAX_OFFS; table++) {

                while (ip(af_cfg, IP_RULE_FLUSH, DEL, YES, &ZERO_IP, 0, table, 0, NULL, 0, NULL, NULL) == SUCCESS) {

                        dbgf(DBGL_SYS, DBGT_ERR, "removed orphan %s rule to table %d", family2Str(af_cfg), table);

                }
        }
}



STATIC_FUNC
void ip_flush_tracked( uint8_t cmd )
{
	TRACE_FUNCTION_CALL;
        struct avl_node *an;
        struct track_node *tn;

        for (an = NULL; (tn = avl_iterate_item(&iptrack_tree, &an));) {

                if (!(cmd == tn->cmd ||
                        (cmd == IP_ROUTE_FLUSH && tn->k.cmd_type == IP_ROUTES) ||
                        (cmd == IP_RULE_FLUSH && tn->k.cmd_type == IP_RULES)))
                        continue;

                ip(tn->k.family, tn->cmd, DEL, NO, &tn->k.net, tn->k.mask, tn->k.table, tn->k.prio, &tn->k.iif, 0, NULL, NULL);

                an = NULL;
        }
}



STATIC_FUNC
int update_interface_rules(void)
{
	TRACE_FUNCTION_CALL;

        ip_flush_tracked(IP_THROW_MY_HNA);
        ip_flush_tracked(IP_THROW_MY_NET);

        struct avl_node *lan = NULL;
        struct if_link_node *iln;

        while (throw_rules && (iln = avl_iterate_item(&if_link_tree, &lan))) {

                if (!((iln->flags & IFF_UP)))
                        continue;

                struct avl_node *aan;
                struct if_addr_node *ian;

                for (aan = NULL; (ian = avl_iterate_item(&iln->if_addr_tree, &aan));) {

                        assertion(-500609, is_ip_set(&ian->ip_addr));

                        if (ian->ifa.ifa_family != af_cfg)
                                continue;

                        if (!ian->ifa.ifa_prefixlen)
                                continue;

                        if (is_ip_net_equal(&ian->ip_addr, &IP6_LINKLOCAL_UC_PREF, IP6_LINKLOCAL_UC_PLEN, AF_INET6))
                                continue;

                        struct avl_node *dan;
                        struct dev_node *dev = NULL;

                        for (dan = NULL; (dev = avl_iterate_item(&dev_ip_tree, &dan));) {

                                if (af_cfg != ian->ifa.ifa_family || !dev->if_global_addr)
                                        continue;

                                if (is_ip_net_equal(&ian->ip_addr, &dev->if_global_addr->ip_addr, ian->ifa.ifa_prefixlen,
                                        af_cfg))
                                        break;

                        }

                        if (dev)
                                continue;

                        IPX_T throw_net = ian->ip_addr;
                        ip_netmask_validate(&throw_net, ian->ifa.ifa_prefixlen, af_cfg, YES);

                        ip(af_cfg, IP_THROW_MY_NET, ADD, NO, &throw_net, ian->ifa.ifa_prefixlen,
                                RT_TABLE_NETS, 0, NULL, 0, NULL, NULL);

                }
        }

#ifdef ADJ_PATCHED_NETW
        struct list_node *throw_pos;
	struct throw_node *throw_node;

        list_for_each(throw_pos, &throw4_list) {

                throw_node = list_entry(throw_pos, struct throw_node, list);

                IPX_T throw6;
                ip42X(&throw6, throw_node->addr);

                configure_route(&throw6, AF_INET, throw_node->netmask, 0, 0, 0, 0, RT_TABLE_HOSTS, RTN_THROW, ADD, IP_THROW_MY_NET);
                configure_route(&throw6, AF_INET, throw_node->netmask, 0, 0, 0, 0, RT_TABLE_NETS, RTN_THROW, ADD, IP_THROW_MY_NET);
                configure_route(&throw6, AF_INET, throw_node->netmask, 0, 0, 0, 0, RT_TABLE_TUNS, RTN_THROW, ADD, IP_THROW_MY_NET);

        }
#endif
	return SUCCESS;
}



STATIC_INLINE_FUNC
void dev_if_fix(void)
{
	TRACE_FUNCTION_CALL;
        struct if_link_node *iln = avl_first_item(&if_link_tree);
        struct avl_node *lan;
        struct dev_node *dev;
        struct avl_node *dan;

        for (dan = NULL; (dev = avl_iterate_item(&dev_name_tree, &dan));) {
                if (dev->hard_conf_changed) {
                        dev->if_link = NULL;
                        if (dev->if_llocal_addr) {
                                dev->if_llocal_addr->dev = NULL;
                                dev->if_llocal_addr = NULL;
                        }
                        if (dev->if_global_addr) {
                                dev->if_global_addr->dev = NULL;
                                dev->if_global_addr = NULL;
                        }
                }
        }


        for (lan = NULL; (iln = avl_iterate_item(&if_link_tree, &lan));) {

                struct dev_node *dev = dev_get_by_name(iln->name.str);

                if (dev && dev->hard_conf_changed && (iln->flags & IFF_UP))
                        dev->if_link = iln;
        }


        for (dan = NULL; (dev = avl_iterate_item(&dev_name_tree, &dan));) {

                if (!(dev->hard_conf_changed && dev->if_link && (dev->if_link->flags & IFF_UP)))
                        continue;

                assertion( -500620, (!dev->if_llocal_addr && !dev->if_global_addr ));

                struct if_addr_node *ian;
                struct avl_node *aan;

                for (aan = NULL; (ian = avl_iterate_item(&dev->if_link->if_addr_tree, &aan));) {

                        IDM_T discarded_llocal_addr = NO;
                        IDM_T discarded_global_addr = NO;

                        if (af_cfg != ian->ifa.ifa_family || strcmp(dev->label_cfg.str, ian->label.str))
                                continue;

                        dbgf(DBGL_SYS, DBGT_INFO, "testing %s=%s %s",
                                dev->arg_cfg, ian->label.str, ipXAsStr(af_cfg, &ian->ip_addr));

                        if (af_cfg == AF_INET6 && is_ip_net_equal(&ian->ip_addr, &IP6_MC_PREF, IP6_MC_PLEN, AF_INET6)) {

                                        dbgf(DBGL_SYS, DBGT_INFO, "skipping multicast");
                                        continue;
                        }

                        IDM_T ip6llocal = is_ip_net_equal(&ian->ip_addr, &IP6_LINKLOCAL_UC_PREF, IP6_LINKLOCAL_UC_PLEN, AF_INET6);

                        if (af_cfg == AF_INET || ip6llocal) {

                                if (cfg_llocal_bmx_prefix_len ?
                                        is_ip_net_equal(&cfg_llocal_bmx_prefix, &ian->ip_addr, cfg_llocal_bmx_prefix_len, af_cfg) :
                                        !dev->if_llocal_addr) {

                                        dev->if_llocal_addr = ian;

                                } else if (af_cfg == AF_INET || ip6llocal) {

                                        discarded_llocal_addr = YES;
                                }
                        }

                        if ((af_cfg == AF_INET || !ip6llocal) &&
                                (dev == primary_dev_cfg || dev->announce)) {

                                if (cfg_global_bmx_prefix_len ?
                                        is_ip_net_equal(&cfg_global_bmx_prefix, &ian->ip_addr, cfg_global_bmx_prefix_len, af_cfg) :
                                        !dev->if_global_addr ) {

                                        dev->if_global_addr = ian;

                                } else if (af_cfg == AF_INET || !ip6llocal) {

                                        discarded_global_addr = YES;
                                }
                        }

                        if (discarded_llocal_addr) {
                                dbg_mute(40, DBGL_SYS, DBGT_WARN,
                                        "ambiguous link-local IP %s for dev %s (selected %s)! Use --%s=... to refine",
                                        ipXAsStr(af_cfg, &ian->ip_addr), ian->label.str,
                                        ipXAsStr(af_cfg, &dev->if_llocal_addr->ip_addr), ARG_LLOCAL_PREFIX);
                        }

                        if (discarded_global_addr) {
                                dbg_mute(40, DBGL_SYS, DBGT_WARN,
                                        "ambiguous global IP %s for dev %s (selected %s)! Use --%s=... to refine",
                                        ipXAsStr(af_cfg, &ian->ip_addr), ian->label.str,
                                        ipXAsStr(af_cfg, &dev->if_global_addr->ip_addr), ARG_GLOBAL_PREFIX);
                        }
                }

                if (wordsEqual("lo", dev->name_phy_cfg.str)) {
                        // the loopback interface usually does not need a link-local address, BUT BMX needs one
                        // And it MUST have a global one.
                        if (!dev->if_global_addr)
                                dev->if_llocal_addr = NULL;
                        else if (!dev->if_llocal_addr)
                                dev->if_llocal_addr = dev->if_global_addr;
                }

                if (dev->if_llocal_addr) {
                        dev->if_llocal_addr->dev = dev;
                } else {
                        dbg_mute(30, DBGL_SYS, DBGT_ERR,
                                "No link-local IP found for %sprimary --%s=%s !",
                                dev == primary_dev_cfg ? "" : "non-", dev->arg_cfg, dev->label_cfg.str);
                }

                if (dev->if_global_addr && dev->if_llocal_addr) {

                        dev->if_global_addr->dev = dev;

                } else if (dev->if_global_addr && !dev->if_llocal_addr) {

                        dev->if_global_addr = NULL;

                } else {
                        if (dev == primary_dev_cfg || dev->announce) {

                                dbg_mute(30, DBGL_SYS, DBGT_ERR,
                                        "No global IP found for %sprimary --%s=%s ! DEACTIVATING !!!",
                                        dev == primary_dev_cfg ? "" : "non-", dev->arg_cfg, dev->label_cfg.str);

                                if (dev->if_llocal_addr) {
                                        dev->if_llocal_addr->dev = NULL;
                                        dev->if_llocal_addr = NULL;
                                }
                        }
                }
        }
}

void dev_check(IDM_T kernel_ip_config_changed)
{
	TRACE_FUNCTION_CALL;

        struct avl_node *an;
        struct dev_node *dev;

	uint8_t cb_conf_hooks = NO;

	dbgf_all( DBGT_INFO, " " );

	if ( !dev_name_tree.items ) {
		dbg( DBGL_SYS, DBGT_ERR, "No interfaces specified");
		cleanup_all( CLEANUP_FAILURE );
        }

        // fix all dev->.._ian stuff here:
        dev_if_fix();

        for (an = NULL; (dev = avl_iterate_item(&dev_name_tree, &an));) {

                if (dev->hard_conf_changed) {

                        if (dev->active) {
                                dbg(DBGL_SYS, DBGT_WARN,
                                        "detected changed but used %sprimary interface: %s ! Deactivating now...",
                                        (dev == primary_dev_cfg ? "" : "non-"), dev->label_cfg.str);

                                cb_conf_hooks = YES;
                                dev_deactivate(dev);
                        }

                }

                IDM_T iff_up = dev->if_llocal_addr && (dev->if_llocal_addr->iln->flags & IFF_UP);

                assertion(-500895, (!(dev->active && !iff_up) == IMPLIES(dev->active, iff_up)));
                assertion(-500896, (IMPLIES(dev->active, iff_up)));
                assertion(-500897, (!(dev->active && !iff_up)));

                assertion(-500898, (!(dev->active && iff_up && dev->hard_conf_changed) == IMPLIES(dev->active, (!iff_up || !dev->hard_conf_changed))));
                assertion(-500901, (IMPLIES(dev->active, (!dev->hard_conf_changed))));
                assertion(-500899, (IMPLIES(dev->active ,(!iff_up || !dev->hard_conf_changed))));
                assertion(-500900, (!(dev->active && iff_up && dev->hard_conf_changed)));

                assertion(-500598, (!(dev->active && !iff_up) && !(dev->active && iff_up && dev->hard_conf_changed)));

                if (!dev->active && iff_up && dev->hard_conf_changed) {

                        struct dev_node *tmp_dev = avl_find_item(&dev_ip_tree, &dev->if_llocal_addr->ip_addr);

                        if (tmp_dev && !wordsEqual(tmp_dev->name_phy_cfg.str, dev->name_phy_cfg.str)) {

                                dbg_mute(40, DBGL_SYS, DBGT_ERR, "%s=%s IP %-15s already used for IF %s",
                                        dev->arg_cfg, dev->label_cfg.str, dev->ip_llocal_str, tmp_dev->label_cfg.str);

                        } else if (dev->announce && !dev->if_global_addr) {

                                dbg(DBGL_SYS, DBGT_ERR,
                                        "%s=%s %s but no global addr", dev->arg_cfg, dev->label_cfg.str, "to-be announced");

                        } else if (dev == primary_dev_cfg && !dev->if_global_addr) {

                                dbg(DBGL_SYS, DBGT_ERR,
                                        "%s=%s %s but no global addr", dev->arg_cfg, dev->label_cfg.str, "primary dev");

                        } else if (wordsEqual("lo", dev->name_phy_cfg.str) && !dev->if_global_addr) {

                                dbg(DBGL_SYS, DBGT_ERR,
                                        "%s=%s %s but no global addr", dev->arg_cfg, dev->label_cfg.str, "loopback");

                        } else  {

				if ( !initializing )
					dbg_mute( 50, DBGL_SYS, DBGT_INFO,
                                        "detected valid but disabled dev: %s ! Activating now...", dev->label_cfg.str);

                                dev_activate(dev);
			}
                }

                dev->hard_conf_changed = NO;

                if (dev->active && (dev->soft_conf_changed || dev_soft_conf_changed)) {

			dev_reconfigure_soft( dev );
		}


		if ( initializing  &&  !dev->active ) {

                        if (dev == primary_dev_cfg) {
				dbg( DBGL_SYS, DBGT_ERR,
                                        "at least primary %s=%s MUST be operational at startup! "
                                        "Use loopback (e.g. lo:bmx a.b.c.d/32 ) if nothing else is available!",
                                        dev->arg_cfg, dev->label_cfg.str);

				cleanup_all( CLEANUP_FAILURE );
			}

			dbg( DBGL_SYS, DBGT_WARN,
			     "not using interface %s (retrying later): interface not ready", dev->label_cfg.str);

                }
        }

        dev_soft_conf_changed = NO;



	if ( cb_conf_hooks)
                cb_plugin_hooks(PLUGIN_CB_CONF, NULL);

        if (kernel_ip_config_changed)
                update_interface_rules();

}




STATIC_FUNC
int32_t opt_policy_rt(uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn)
{
	TRACE_FUNCTION_CALL;

	if ( cmd == OPT_APPLY ) {

		check_apply_parent_option( ADD, OPT_APPLY, _save, get_option( 0, 0, ARG_PRIO_RULES  ), "0", cn );
		check_apply_parent_option( ADD, OPT_APPLY, _save, get_option( 0, 0, ARG_THROW_RULES ), "0", cn );

	} else if ( cmd == OPT_SET_POST  &&  initializing ) {


	} else if ( cmd == OPT_POST  &&  initializing ) {

                dbgf(DBGL_CHANGES, DBGT_INFO, "OPT_POST start");

		// add rule for hosts and announced interfaces and networks
		if ( prio_rules && primary_dev_cfg) {

                        ip_flush_routes();
                        ip_flush_rules();

/*
                        ip_flush_tracked(IP_ROUTE_FLUSH);
                        ip_flush_tracked(IP_RULE_FLUSH);
*/

                        ip(af_cfg, IP_RULE_DEFAULT, ADD, NO, &ZERO_IP, 0,
                                RT_TABLE_HOSTS, RT_PRIO_HOSTS, NULL, 0, NULL, NULL);

                        ip(af_cfg, IP_RULE_DEFAULT, ADD, NO, &ZERO_IP, 0,
                                RT_TABLE_NETS, RT_PRIO_NETS, NULL, 0, NULL, NULL);

		}

                /*
                // add rules and routes for interfaces
                if (update_interface_rules(IF_RULE_UPD_ALL//was IF_RULE_SET_NETWORKS
                  ) < 0)
                        cleanup_all( CLEANUP_FAILURE );
                 */
                dbgf(DBGL_CHANGES, DBGT_INFO, "OPT_POST done");

	}

	return SUCCESS;
}


#ifdef ADJ_PATCHED_NETW

STATIC_FUNC
int32_t opt_throw(uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn)
{

	IPX_T ipX;
	uint8_t maskx;
        uint8_t family;

	char tmp[IPXNET_STR_LEN];
        struct throw_node *throw_node = NULL;
        struct list_node *throw_tmp, *throw_pos;

	if ( cmd == OPT_ADJUST  ||  cmd == OPT_CHECK  ||  cmd == OPT_APPLY ) {

		if ( strchr(patch->p_val, '/')//patch->p_val[0] >= '0'  &&  patch->p_val[0] <= '9'
                        ) {
			// configure an unnamed throw-rule

                        if (str2netw(patch->p_val, &ipX, '/', cn, &maskx, &family) == FAILURE)
				return FAILURE;

                        if (ip_netmask_validate(&ipX, maskx, family) == FAILURE) {
                                dbg_cn(cn, DBGL_SYS, DBGT_ERR, "invalid prefix");
                                return FAILURE;
                        }


                        sprintf(tmp, "%s/%d", ipXAsStr(family, &ipX), maskx);
                        set_opt_parent_val(patch, tmp);

		} else {
                        // configure a named throw-rule

                        // just for adjust and check
			if ( adj_patched_network( opt, patch, tmp, &ip, &mask, cn ) == FAILURE )
				return FAILURE;

			if ( patch->p_diff == ADD ) {
				if ( adj_patched_network( opt, patch, tmp, &ip, &mask, cn ) == FAILURE )
					return FAILURE;
			} else {
				// re-configure network and netmask parameters of an already configured and named throw-rule
                                if (get_tracked_network(opt, patch, tmp, &ip, &mask, cn) == FAILURE)
					return FAILURE;
			}
                }

                struct list_node *prev_pos = (struct list_node *) & throw4_list;
		list_for_each_safe( throw_pos, throw_tmp, &throw4_list ) {
			throw_node = list_entry(throw_pos, struct throw_node, list);
			if ( throw_node->addr == ip  &&  throw_node->netmask == mask )
				break;
			prev_pos = &throw_node->list;
			throw_node = NULL;
		}

		if ( cmd == OPT_ADJUST )
			return SUCCESS;

		if ( ( patch->p_diff != ADD  &&  !throw_node )  ||  ( patch->p_diff == ADD &&  throw_node ) ) {
			dbg_cn( cn, DBGL_SYS, DBGT_ERR, "%s %s does %s exist!",
			        ARG_THROW, tmp, patch->p_diff == ADD ? "already" : "not" );
			return FAILURE;
		}

		if ( cmd == OPT_CHECK )
			return SUCCESS;

		if ( patch->p_diff == DEL || patch->p_diff == NOP ) {
			list_del_next( &throw4_list, prev_pos );
			debugFree( throw_pos, -300077 );
		}

		if ( patch->p_diff == NOP ) {
			// get new network again
			if ( adj_patched_network( opt, patch, tmp, &ip, &mask, cn ) == FAILURE )
				return FAILURE;
		}

		if ( patch->p_diff == ADD || patch->p_diff == NOP ) {

			throw_node = debugMalloc( sizeof(struct throw_node), -300033 );
			memset( throw_node, 0, sizeof(struct throw_node) );
			list_add_tail( &throw4_list, &throw_node->list );

			throw_node->addr = ip;
			throw_node->netmask = mask;
		}


/*
		if ( on_the_fly ) {
		// add rules and routes for interfaces :
			if ( update_interface_rules( IF_RULE_UPD_ALL ) < 0 )
				cleanup_all( CLEANUP_FAILURE );
		}
*/

		return SUCCESS;


	} else if ( cmd == OPT_UNREGISTER ) {

                while ((throw_node = list_rem_head(&throw4_list)))
                        debugFree(throw_node, -300078);

	}
	return SUCCESS;
}
#endif



STATIC_FUNC
int32_t opt_interfaces(uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn)
{
	TRACE_FUNCTION_CALL;

	if ( cmd == OPT_APPLY ) {

#define DBG_STATUS4_DEV_HEAD "%-10s %6s %9s %10s %2s %16s %2s %16s %16s %8s %1s\n"
#define DBG_STATUS6_DEV_HEAD "%-10s %6s %9s %10s %3s %30s %3s %30s %30s %8s %1s\n"
#define DBG_STATUS4_DEV_INFO "%-10s %6s %9s %10ju %16s/%-2d %16s/%-2d %16s %8d %1s\n"
#define DBG_STATUS6_DEV_INFO "%-10s %6s %9s %10ju %30s/%-3d %30s/%-3d %30s %8d %1s\n"

                dbg_printf(cn, (af_cfg == AF_INET ? DBG_STATUS4_DEV_HEAD : DBG_STATUS6_DEV_HEAD),
                        "dev", "status", "type", "maxMetric", " ", "llocal", " ", "global", "broadcast", "helloSqn", "#");

                struct avl_node *it=NULL;
                struct dev_node *dev;
                while ((dev = avl_iterate_item(&dev_name_tree, &it))) {
                        
                        IDM_T iff_up = dev->if_llocal_addr && (dev->if_llocal_addr->iln->flags & IFF_UP);

                        dbg_printf(cn, (af_cfg == AF_INET ? DBG_STATUS4_DEV_INFO : DBG_STATUS6_DEV_INFO),
                                dev->label_cfg.str,
                                iff_up ? "UP":"DOWN",
			        !dev->active ? "INACTIVE" :
			        ( dev->linklayer == VAL_DEV_LL_LO ? "loopback":
			          ( dev->linklayer == VAL_DEV_LL_LAN ? "ethernet":
			            ( dev->linklayer == VAL_DEV_LL_WLAN ? "wireless": "???" ) ) ),
                                dev->umetric_max,
                                dev->ip_llocal_str,
                                dev->if_llocal_addr ? dev->if_llocal_addr->ifa.ifa_prefixlen : -1,
                                dev->ip_global_str,
                                dev->if_global_addr ? dev->if_global_addr->ifa.ifa_prefixlen : -1,
                                dev->ip_brc_str,
			        dev->link_hello_sqn,
                                dev == primary_dev_cfg ? "P" : "N"
			      );
                }
                dbg_printf(cn, "\n");
	}
	return SUCCESS;
}


STATIC_FUNC
int32_t opt_dev(uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn)
{
	TRACE_FUNCTION_CALL;

	struct list_node *list_pos;
	struct dev_node *dev = NULL;

        uint8_t family = 0;

        if (!strcmp(opt->long_name, ARG_DEV6))
                family = AF_INET6;
        else if (!strcmp(opt->long_name, ARG_DEV))
                family = AF_INET;


        if ( cmd == OPT_CHECK  ||  cmd == OPT_APPLY ) {

                if (!family)
                        return FAILURE;

                if (af_cfg && af_cfg != family) {

                        dbg_cn(cn, DBGL_SYS, DBGT_ERR, "only one ip protocol is supported at a time");
                        return FAILURE;
                }

		if ( strlen(patch->p_val) >= IFNAMSIZ ) {
			dbg_cn( cn, DBGL_SYS, DBGT_ERR, "dev name MUST be smaller than %d chars", IFNAMSIZ );
			return FAILURE;
                }

                char *colon_ptr;
                char phy_name[IFNAMSIZ] = {0};
                strcpy(phy_name, patch->p_val);

                // if given interface is an alias then truncate to physical interface name:
                if ((colon_ptr = strchr(phy_name, ':')) != NULL)
                        *colon_ptr = '\0';


                dev = dev_get_by_name(phy_name);

                if ( dev && strcmp(dev->label_cfg.str, patch->p_val)) {
                        dbg_cn(cn, DBGL_SYS, DBGT_ERR,
                                "%s=%s (%s) already used for %s=%s %s!",
                                opt->long_name, patch->p_val, phy_name, opt->long_name, dev->label_cfg.str, dev->ip_llocal_str);

                        return FAILURE;
                }

		if ( patch->p_diff == DEL ) {

                        if (dev && dev == primary_dev_cfg) {

                                dbg_cn(cn, DBGL_SYS, DBGT_ERR,
                                        "primary interface %s %s can not be removed!",
                                        dev->label_cfg.str, dev->ip_llocal_str);

				return FAILURE;

                        } else if (dev && cmd == OPT_APPLY) {

                                if (dev->active)
                                        dev_deactivate(dev);



                                avl_remove(&dev_name_tree, &dev->name_phy_cfg, -300205);

                                uint16_t i;
                                for (i = 0; i < plugin_data_registries[PLUGIN_DATA_DEV]; i++) {
                                        assertion(-500767, (!dev->plugin_data[i]));
                                }

                                debugFree(dev, -300048);

                                return SUCCESS;


                        } else if (!dev) {

                                dbgf_cn(cn, DBGL_SYS, DBGT_ERR, "Interface does not exist!");
                                return FAILURE;
                        }
                }

                if ( cmd == OPT_CHECK )
			return SUCCESS;

		if ( !dev ) {

                        dbgf(DBGL_CHANGES, DBGT_INFO, "cmd: %s opt: %s  %s instance %s",
                                opt_cmd2str[cmd], opt->long_name, family2Str(family), patch ? patch->p_val : "");

                        uint32_t dev_size = sizeof (struct dev_node) + (sizeof (void*) * plugin_data_registries[PLUGIN_DATA_DEV]);
                        dev = debugMalloc(dev_size, -300002);
                        memset(dev, 0, dev_size);

                        if (!primary_dev_cfg) {
                                primary_dev_cfg = dev;

                                af_cfg = family;
                                //self.family = primary_dev_cfg->family_cfg;
                                //self.bmx_afinet = (self.family == AF_INET) ? BMX_AFINET4 : BMX_AFINET6;
                        }

                        strcpy(dev->label_cfg.str, patch->p_val);
                        strcpy(dev->name_phy_cfg.str, phy_name);

                        avl_insert(&dev_name_tree, dev, -300144);

                        dev->hard_conf_changed = YES;

                        dev->arg_cfg = (char*)opt->long_name;


                        if (dev == primary_dev_cfg)
                                dev->announce = YES;
                        else
                                dev->announce = DEF_DEV_ANNOUNCE;

                        dev->umetric_max = DEF_DEV_BANDWIDTH;

                        /*
                         * specifying the outgoing src address for IPv6 seems not working
                         * http://www.ureader.de/msg/12621915.aspx
                         * http://www.davidc.net/networking/ipv6-source-address-selection-linux
                         * http://etherealmind.com/ipv6-which-address-multiple-ipv6-address-default-address-selection/
                         * http://marc.info/?l=linux-net&m=127811438206347&w=2
                         */


                        dbgf_all(DBGT_INFO, "assigned dev %s physical name %s",
                                dev->label_cfg.str, dev->name_phy_cfg.str);

			// some configurable interface values - initialized to unspecified:

			dev->linklayer_conf = OPT_CHILD_UNDEFINED;
                        dev->umetric_max_conf = OPT_CHILD_UNDEFINED;

		}


		list_for_each( list_pos, &patch->childs_instance_list ) {

			struct opt_child *c = list_entry( list_pos, struct opt_child, list );

                        int32_t val = c->c_val ? strtol(c->c_val, NULL, 10) : OPT_CHILD_UNDEFINED;

                        if (!strcmp(c->c_opt->long_name, ARG_DEV_IP)) {

                                dev->hard_conf_changed = YES;

			} else if ( !strcmp( c->c_opt->long_name, ARG_DEV_LL ) ) {

                                dev->linklayer_conf = val;
                                dev->hard_conf_changed = YES;

			} else if ( !strcmp( c->c_opt->long_name, ARG_DEV_BANDWIDTH ) ) {

                                if (c->c_val) {
                                        unsigned long long ull = strtoul(c->c_val, NULL, 10);

                                        if (ull > (umetric_max(DEF_FMETRIC_EXP_OFFSET)) ||
                                                ull < (umetric(0, 0, DEF_FMETRIC_EXP_OFFSET))) {

                                                dbgf(DBGL_SYS, DBGT_ERR, "%s %c%s given with illegal value",
                                                        dev->label_cfg.str, LONG_OPT_ARG_DELIMITER_CHAR, c->c_opt->long_name);

                                                return FAILURE;
                                        }

                                        dev->umetric_max_configured = ull;
                                        dev->umetric_max_conf = 0;

                                } else  {
                                        dev->umetric_max_configured = 0;
                                        dev->umetric_max_conf = OPT_CHILD_UNDEFINED;
                                }

//				dev->hard_conf_changed = YES;

			} else if ( !strcmp( c->c_opt->long_name, ARG_DEV_ANNOUNCE ) ) {

                                dev->announce = val;
                                dev->hard_conf_changed = YES;
			}

			dev->soft_conf_changed = YES;

		}


	} else if ( cmd == OPT_POST  &&  opt  &&  !opt->parent_name ) {


                dev_check(initializing ? kernel_if_config() : NO); //will always be called whenever a parameter is changed (due to OPT_POST)

        }

	return SUCCESS;
}



static struct opt_type ip_options[]=
{
//        ord parent long_name          shrt Attributes				*ival		min		max		default		*func,*syntax,*help
	{ODI,0,0,	                0,  0,0,0,0,0,0,			0,		0,		0,		0,		0,
			0,		"\nip options:"}
        ,
	{ODI,0,ARG_DEV,		        'i',  5,A_PMN,A_ADM,A_DYI,A_CFA,A_ANY,	0,		0, 		0,		0, 		opt_dev,
			"<interface-name>", "add or change IPv4 device or its configuration"}
        ,
	{ODI,ARG_DEV,ARG_DEV_TTL,	't',5,A_CS1,A_ADM,A_DYI,A_CFA,A_ANY,	0,		MIN_TTL,	MAX_TTL,	DEF_TTL,	opt_dev,
			ARG_VALUE_FORM,	"set TTL of generated OGMs"}
        ,
	{ODI,ARG_DEV,ARG_DEV_ANNOUNCE,  'a',5,A_CS1,A_ADM,A_DYI,A_CFA,A_ANY,	0,		0,		1,		DEF_DEV_ANNOUNCE,opt_dev,
			ARG_VALUE_FORM,	"disable/enable announcement of interface IP"}
        ,
	{ODI,ARG_DEV,ARG_DEV_LL,	 'l',5,A_CS1,A_ADM,A_DYI,A_CFA,A_ANY,	0,		VAL_DEV_LL_LAN,	VAL_DEV_LL_WLAN,0,		opt_dev,
			ARG_VALUE_FORM,	"manually set device type for linklayer specific optimization (1=lan, 2=wlan)"}
        ,
	{ODI,ARG_DEV,ARG_DEV_BANDWIDTH,	'b',5,A_CS1,A_ADM,A_DYI,A_CFA,A_ANY,	0,		0,              0,              0,              opt_dev,
			ARG_VALUE_FORM,	"set maximum bandwitdh as bits/sec of dev"}

        ,
	{ODI,0,ARG_DEV6,	        0,  5,A_PMN,A_ADM,A_DYI,A_CFA,A_ANY,	0,		0, 		0,		0, 		opt_dev,
			"<interface-name>", "add or change IPv6 device or its configuration"}
        ,
	{ODI,ARG_DEV6,ARG_DEV_ANNOUNCE,  'a',5,A_CS1,A_ADM,A_DYI,A_CFA,A_ANY,	0,		0,		1,		DEF_DEV_ANNOUNCE,opt_dev,
			ARG_VALUE_FORM,	"disable/enable announcement of interface IP"}
        ,
	{ODI,ARG_DEV6,ARG_DEV_LL,	 'l',5,A_CS1,A_ADM,A_DYI,A_CFA,A_ANY,	0,		VAL_DEV_LL_LAN,	VAL_DEV_LL_WLAN,0,		opt_dev,
			ARG_VALUE_FORM,	"manually set device type for linklayer specific optimization (1=lan, 2=wlan)"}
        ,
	{ODI,ARG_DEV6,ARG_DEV_BANDWIDTH, 'b',5,A_CS1,A_ADM,A_DYI,A_CFA,A_ANY,	0,		0,              0,              0,              opt_dev,
			ARG_VALUE_FORM,	"set maximum bandwitdh as bits/sec of dev"}


        ,
	{ODI,0,ARG_INTERFACES,	         0,  5,A_PS0,A_USR,A_DYI,A_ARG,A_ANY,	0,		0,		1,		0,		opt_interfaces,
			0,		"show configured interfaces"}
        ,
#ifndef LESS_OPTIONS
	{ODI,0,ARG_PEDANTIC_CLEANUP,	0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&Pedantic_cleanup,0,		1,		DEF_PEDANT_CLNUP,0,
			ARG_VALUE_FORM,	"disable/enable pedantic cleanup of system configuration (like ip_forward,..) \n"
			"	at program termination. Its generally safer to keep this disabled to not mess up \n"
			"	with other routing protocols"}
        ,
	{ODI,0,ARG_THROW_RULES,	        0,  4,A_PS1,A_ADM,A_INI,A_CFA,A_ANY,	&throw_rules,	0, 		1,		DEF_THROW_RULES,0,
			ARG_VALUE_FORM,	"disable/enable default throw rules"},

	{ODI,0,ARG_PRIO_RULES,	        0,  4,A_PS1,A_ADM,A_INI,A_CFA,A_ANY,	&prio_rules,	0, 		1,		DEF_PRIO_RULES, 0,
			ARG_VALUE_FORM,	"disable/enable default priority rules"}
        ,
#endif
	{ODI,0,ARG_NO_POLICY_RT,	'n',4,A_PS0,A_ADM,A_INI,A_ARG,A_ANY,	0,		0, 		0,		0,		opt_policy_rt,
			0,		"disable policy routing (throw and priority rules)"},

#ifndef WITH_UNUSED

        {ODI,0,"lo_rule",		0,  4,A_PS1,A_ADM,A_INI,A_CFA,A_ANY,	&Lo_rule,	0, 		1,		DEF_LO_RULE,	0,
			ARG_VALUE_FORM,	"disable/enable autoconfiguration of lo rule"}
#endif
#ifdef ADJ_PATCHED_NETW
        ,
	{ODI,0,ARG_THROW,		0,  5,A_PMN,A_ADM,A_DYI,A_CFA,A_ANY,	0,		0,		0,		0,		opt_throw,
			ARG_PREFIX_FORM, 	"do NOT route packets matching src or dst IP range(s) into gateway tunnel or announced networks"},

	{ODI,ARG_THROW,ARG_UHNA_NETWORK,	'n',5,A_CS1,A_ADM,A_DYI,A_CFA,A_ANY,	0,		0,		0,		0,		opt_throw,
			ARG_NETW_FORM, 	"specify network of throw rule"},

	{ODI,ARG_THROW,ARG_UHNA_PREFIXLEN,	'm',5,A_CS1,A_ADM,A_DYI,A_CFA,A_ANY,	0,		0,		0,		0,		opt_throw,
			ARG_MASK_FORM, 	"specify network of throw rule"}
#endif

};




void init_ip(void)
{
        assertion(-500894, (is_zero(((char*)&ZERO_IP), sizeof (ZERO_IP))));

        if (rtnl_open(&ip_rth) != SUCCESS) {
                dbgf(DBGL_SYS, DBGT_ERR, "failed opening rtnl socket");
                cleanup_all( -500561 );
        }

        errno=0;
	if ( !rt_sock  &&  (rt_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		dbgf( DBGL_SYS, DBGT_ERR, "can't create routing socket %s:",  strerror(errno) );
		cleanup_all( -500021 );
	}

        register_options_array(ip_options, sizeof ( ip_options));
        InitSha(&ip_sha);

	if( ( nlsock_default = open_netlink_socket()) <= 0 )
		cleanup_all( -500067 );

        if ((nlsock_flush_all = open_netlink_socket()) <= 0)
		cleanup_all( -500658 );

}

void cleanup_ip(void)
{

        // if ever started succesfully in daemon mode...
	//if ( !initializing ) {

                ip_flush_tracked( IP_ROUTE_FLUSH );
                ip_flush_routes();

                ip_flush_tracked( IP_RULE_FLUSH );
                ip_flush_rules();

        //}

        kernel_if_fix(YES,0);

        sysctl_restore(NULL);

        if (ip_rth.fd >= 0) {
                close(ip_rth.fd);
                ip_rth.fd = -1;
        }

        if ( rt_sock )
		close( rt_sock );

	rt_sock = 0;


        if( nlsock_default > 0 )
                close( nlsock_default );
        nlsock_default = 0;


        if (nlsock_flush_all> 0)
                close( nlsock_flush_all );
        nlsock_flush_all = 0;


        while (dev_name_tree.items) {

                struct dev_node *dev = dev_name_tree.root->item;

                if (dev->active)
                        dev_deactivate(dev);

                avl_remove(&dev_name_tree, &dev->name_phy_cfg, -300204);

                debugFree(dev, -300046);

        }

}

