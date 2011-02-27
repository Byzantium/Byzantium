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


#define ARG_UHNA "unicast_hna"

#define ARG_UHNA_NETWORK     "network"

#define	MIN_UHNA_PREFIXLEN   1
#define	MAX_UHNA_PREFIXLEN   32
#define ARG_UHNA_PREFIXLEN   "prefixlen"

#define MIN_UHNA_METRIC      0
#define MAX_UHNA_METRIC      U32_MAX
#define DEF_UHNA_METRIC      0
#define ARG_UHNA_METRIC      "metric"


struct uhna_key {
	uint8_t family;
	uint8_t prefixlen;
	IPX_T glip;
	uint32_t metric_nl;
};

struct uhna_node {
	struct uhna_key key;
	struct orig_node *on;
};


struct description_msg_hna4 {
	uint8_t prefixlen;
	uint8_t reserved;
	IP4_T    ip4;
	uint32_t metric;
} __attribute__((packed));


struct description_msg_hna6 {
	uint8_t prefixlen;
	uint8_t reserved;
	IP6_T    ip6;
	uint32_t metric;
} __attribute__((packed));

