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


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>



#include "bmx.h"
#include "msg.h"
#include "ip.h"
#include "plugin.h"
#include "hna.h"
#include "tools.h"

AVL_TREE(global_uhna_tree, struct uhna_node, key );
AVL_TREE(local_uhna_tree, struct uhna_node, key );


STATIC_FUNC
void set_uhna_key(struct uhna_key *key, uint8_t family, uint8_t prefixlen, IPX_T *glip, uint32_t metric)
{
        memset( key, 0, sizeof(struct uhna_key));
        key->family = family;
        key->prefixlen = prefixlen;
        key->metric_nl = htonl(metric);
        key->glip = *glip;

}

STATIC_FUNC
void set_uhna_to_key(struct uhna_key *key, struct description_msg_hna4 *uhna4, struct description_msg_hna6 *uhna6)
{
        if (uhna4) {

                IPX_T ipX;
                ip42X(&ipX, uhna4->ip4);

                set_uhna_key(key, AF_INET, uhna4->prefixlen, &ipX, ntohl(uhna4->metric));

        } else {

                set_uhna_key(key, AF_INET6, uhna6->prefixlen, &uhna6->ip6, ntohl(uhna6->metric));

        }
}


STATIC_FUNC
int _create_tlv_hna(int family, uint8_t* data, uint16_t max_size, uint16_t pos,
        IPX_T *ip, uint32_t metric, uint16_t prefixlen)
{
        int i;
        uint16_t msg_size = family == AF_INET ?
                sizeof (struct description_msg_hna4) : sizeof (struct description_msg_hna6);


        if ((pos + msg_size) > max_size) {

                dbgf(DBGL_SYS, DBGT_ERR, "unable to announce %s/%d metric %d due to limiting --%s=%d",
                        ipXAsStr(family, ip), prefixlen, ntohl(metric), ARG_UDPD_SIZE, max_size);

                return pos;
        }

        dbgf_all(DBGT_INFO, "announce %s/%d metric %d ", ipXAsStr(family, ip), prefixlen, ntohl(metric));


        assertion(-500610, (!(family == AF_INET6 &&
                is_ip_net_equal(ip, &IP6_LINKLOCAL_UC_PREF, IP6_LINKLOCAL_UC_PLEN, AF_INET6))));
        // this should be catched during configuration!!


        if (family == AF_INET) {
                struct description_msg_hna4 * msg4 = ((struct description_msg_hna4 *) data);

                struct description_msg_hna4 hna4;
                memset( &hna4, 0, sizeof(hna4));
                hna4.ip4 = ipXto4(*ip);
                hna4.metric = metric;
                hna4.prefixlen = prefixlen;

                for (i = 0; i < pos / msg_size; i++) {

                        if (!memcmp(&(msg4[i]), &hna4, sizeof (struct description_msg_hna4)))
                                return pos;
                }

                msg4[i] = hna4;

        } else {
                struct description_msg_hna6 * msg6 = ((struct description_msg_hna6 *) data);

                struct description_msg_hna6 hna6;
                memset( &hna6, 0, sizeof(hna6));
                hna6.ip6 = *ip;
                hna6.metric = metric;
                hna6.prefixlen = prefixlen;

                for (i = 0; i < pos / msg_size; i++) {

                        if (!memcmp(&(msg6[i]), &hna6, sizeof (struct description_msg_hna6)))
                                return pos;
                }

                msg6[i] = hna6;

        }

        dbgf(DBGL_SYS, DBGT_INFO, "%s %s/%d metric %d",
                family2Str(family), ipXAsStr(family, ip), prefixlen, metric);


        return (pos + msg_size);
}

STATIC_FUNC
int create_description_tlv_hna(struct tx_frame_iterator *it)
{
        assertion(-500765, (it->frame_type == BMX_DSC_TLV_UHNA4 || it->frame_type == BMX_DSC_TLV_UHNA6));

        uint8_t *data = tx_iterator_cache_msg_ptr(it);
        uint16_t max_size = tx_iterator_cache_data_space(it);
        uint8_t family = it->frame_type == BMX_DSC_TLV_UHNA4 ? AF_INET : AF_INET6;
        uint8_t max_prefixlen = ort_dat[ (AFINET2BMX(family)) ].max_prefixlen;

        struct avl_node *an;
        struct uhna_node *un;
        struct dev_node *dev;
        int pos = 0;

        if (af_cfg != family || !is_ip_set(&self.primary_ip))
                return 0;

        pos = _create_tlv_hna(family, data, max_size, pos, &self.primary_ip, 0, max_prefixlen);

        for (an = NULL; (dev = avl_iterate_item(&dev_ip_tree, &an));) {

                if (!dev->active || !dev->announce)
                        continue;

                pos = _create_tlv_hna(family, data, max_size, pos, &dev->if_global_addr->ip_addr, 0, max_prefixlen);
        }

        for (an = NULL; (un = avl_iterate_item(&local_uhna_tree, &an));) {

                if (un->key.family != family)
                        continue;

                pos = _create_tlv_hna(family, data, max_size, pos, &un->key.glip, un->key.metric_nl, un->key.prefixlen);
        }

        return pos;
}


STATIC_FUNC
IDM_T configure_route(IDM_T del, struct orig_node *on, struct uhna_key *key)
{

        IDM_T primary = is_ip_equal(&key->glip, &on->primary_ip);
        uint8_t cmd = primary ? IP_ROUTE_HOST : IP_ROUTE_HNA;
        int32_t table_macro = primary ? RT_TABLE_HOSTS : RT_TABLE_NETS;

        // update network routes:
        if (del) {


                return ip(key->family, cmd, DEL, NO, &key->glip, key->prefixlen, table_macro, ntohl(key->metric_nl),
                        NULL, 0, NULL, NULL);


        } else {

                struct router_node *curr_rn = on->curr_rn;

                assertion(-500820, (curr_rn));
                ASSERTION(-500239, (avl_find(&link_dev_tree, &(curr_rn->key))));
                assertion(-500579, (curr_rn->key.dev->if_llocal_addr));

                return ip(key->family, cmd, ADD, NO, &key->glip, key->prefixlen, table_macro, ntohl(key->metric_nl),
                        NULL, curr_rn->key.dev->if_llocal_addr->ifa.ifa_index, &(curr_rn->key.link->link_ip), &(self.primary_ip));

        }

}



STATIC_FUNC
void configure_uhna ( IDM_T del, struct uhna_key* key, struct orig_node *on ) {

        struct uhna_node *un = avl_find_item( &global_uhna_tree, key );

        assertion(-500589, (on));
        assertion(-500236, ((del && un) != (!del && !un)));

        // update uhna_tree:
        if ( del ) {

                assertion(-500234, (on == un->on));
                avl_remove(&global_uhna_tree, &un->key, -300212);
                ASSERTION( -500233, (!avl_find( &global_uhna_tree, key)) ); // there should be only one element with this key

                if (on == &self)
                        avl_remove(&local_uhna_tree, &un->key, -300213);

        } else {

                un = debugMalloc( sizeof (struct uhna_node), -300090 );
                un->key = *key;
                un->on = on;
                avl_insert(&global_uhna_tree, un, -300149);

                if (on == &self)
                        avl_insert(&local_uhna_tree, un, -300150);

        }


        if (on == &self) {

                // update throw routes:
                ip(key->family, IP_THROW_MY_HNA, del, NO, &key->glip, key->prefixlen, RT_TABLE_HOSTS, 0, NULL, 0, NULL, NULL);
                ip(key->family, IP_THROW_MY_HNA, del, NO, &key->glip, key->prefixlen, RT_TABLE_NETS, 0, NULL, 0, NULL, NULL);
                ip(key->family, IP_THROW_MY_HNA, del, NO, &key->glip, key->prefixlen, RT_TABLE_TUNS, 0, NULL, 0, NULL, NULL);

                my_description_changed = YES;

        } else if (on->curr_rn) {

                configure_route(del, on, key);
        }


        if (del)
                debugFree(un, -300089);

}



STATIC_FUNC
int process_description_tlv_hna(struct rx_frame_iterator *it)
{
        ASSERTION(-500357, (it->frame_type == BMX_DSC_TLV_UHNA4 || it->frame_type == BMX_DSC_TLV_UHNA6));
        assertion(-500588, (it->on));

        struct orig_node *on = it->on;
        IDM_T op = it->op;
        uint8_t family = (it->frame_type == BMX_DSC_TLV_UHNA4 ? AF_INET : AF_INET6);

        if (af_cfg != family) {
                dbgf(DBGL_SYS, DBGT_ERR, "invalid family %s", family2Str(family));
                return TLV_DATA_BLOCKED;
        }

        uint16_t msg_size = it->handls[it->frame_type].min_msg_size;
        uint16_t pos;

        for (pos = 0; pos < it->frame_data_length; pos += msg_size) {

                struct uhna_key key;

                if (it->frame_type == BMX_DSC_TLV_UHNA4)
                        set_uhna_to_key(&key, (struct description_msg_hna4 *) (it->frame_data + pos), NULL);

                else
                        set_uhna_to_key(&key, NULL, (struct description_msg_hna6 *) (it->frame_data + pos));



                dbgf_all(DBGT_INFO, "%s %s %s=%s/%d %s=%d",
                        tlv_op_str[op], family2Str(key.family), ARG_UHNA,
                        ipXAsStr(key.family, &key.glip), key.prefixlen, ARG_UHNA_METRIC, ntohl(key.metric_nl));

                if (op == TLV_OP_DEL) {

                        configure_uhna(DEL, &key, on);

                        if (pos == 0 && key.family == family) {
                                on->primary_ip = ZERO_IP;
                                ip2Str(family, &ZERO_IP, on->primary_ip_str);
                        }

                } else if (op == TLV_OP_TEST) {

                        struct uhna_node *un = NULL;

                        if (!is_ip_set(&key.glip) || is_ip_forbidden( &key.glip, family ) ||
                                (un = avl_find_item(&global_uhna_tree, &key))) {

                                dbgf(DBGL_SYS, DBGT_ERR,
                                        "id.name=%s id.rand=%X... %s=%s/%d %s=%d blocked by id.name=%s id.rand=%X...",
                                        on->id.name, on->id.rand.u32[0],
                                        ARG_UHNA, ipXAsStr(key.family, &key.glip), key.prefixlen,
                                        ARG_UHNA_METRIC, ntohl(key.metric_nl),
                                        un ? un->on->id.name : "---", un ? un->on->id.rand.u32[0] : 0);

                                return TLV_DATA_BLOCKED;
                        }

                        if (is_ip_net_equal(&key.glip, &IP6_LINKLOCAL_UC_PREF, IP6_LINKLOCAL_UC_PLEN, AF_INET6)) {

                                dbgf(DBGL_SYS, DBGT_ERR, "NO link-local addresses %s", ipXAsStr(key.family, &key.glip));

                                return TLV_DATA_BLOCKED;
                        }


                } else if (op == TLV_OP_ADD) {

                        //TODO: return with TLVS_BLOCKED because this happens when node announces the same key twice !!!
                        ASSERTION( -500359, (!avl_find(&global_uhna_tree, &key)));

                        if (pos == 0 && key.family == family) {
                                on->primary_ip = key.glip;
                                ip2Str(key.family, &key.glip, on->primary_ip_str);
                        }

                        configure_uhna(ADD, &key, on);


                } else if ( op == TLV_OP_DEBUG ) {

                        dbg_printf(it->cn, "%s=%s/%d %s=%d",
                                ARG_UHNA, ipXAsStr(key.family, &key.glip), key.prefixlen,
                                ARG_UHNA_METRIC, ntohl(key.metric_nl));

                        if (pos == it->frame_data_length - msg_size) {
                                dbg_printf(it->cn, "\n");
                        } else {
                                dbg_printf(it->cn, ", ");
                        }

                } else {
                        assertion( -500369, (NO));
                }
        }


        return pos;
}


STATIC_FUNC
void hna_status (struct ctrl_node *cn)
{
        process_description_tlvs(NULL, &self, self.desc, TLV_OP_DEBUG, af_cfg == AF_INET ? BMX_DSC_TLV_UHNA4 : BMX_DSC_TLV_UHNA6, cn);
}





STATIC_FUNC
int32_t opt_uhna(uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn)
{
        IPX_T ipX;
	uint8_t mask;
        uint32_t metric = 0;
        struct uhna_key key;
	char new[IPXNET_STR_LEN];

	if ( cmd == OPT_ADJUST  ||  cmd == OPT_CHECK  ||  cmd == OPT_APPLY ) {

                uint8_t family = 0;

		dbgf( DBGL_CHANGES, DBGT_INFO, "diff=%d cmd =%s  save=%d  opt=%s  patch=%s",
		        patch->p_diff, opt_cmd2str[cmd], _save, opt->long_name, patch->p_val);


                if (strchr(patch->p_val, '/')) {

                        if (str2netw(patch->p_val, &ipX, '/', cn, &mask, &family) == FAILURE)
                                family = 0;

			// the unnamed UHNA
                        dbgf(DBGL_CHANGES, DBGT_INFO, "unnamed %s %s diff=%d cmd=%s  save=%d  opt=%s  patch=%s",
                                ARG_UHNA, family2Str(family), patch->p_diff, opt_cmd2str[cmd], _save, opt->long_name, patch->p_val);

                        if ( family != AF_INET && family != AF_INET6)
                                return FAILURE;

                        if (is_ip_forbidden(&ipX, family) || ip_netmask_validate(&ipX, mask, family, NO) == FAILURE) {
                                dbg_cn(cn, DBGL_SYS, DBGT_ERR,
                                        "forbidden ip or invalid prefix %s/%d", ipXAsStr(family, &ipX), mask);
                                return FAILURE;
                        }

                        sprintf(new, "%s/%d", ipXAsStr(family, &ipX), mask);

			set_opt_parent_val( patch, new );

			if ( cmd == OPT_ADJUST )
				return SUCCESS;

		} else {
#ifdef ADJ_PATCHED_NETW
			// the named UHNA

			if ( adj_patched_network( opt, patch, new, &ip4, &mask, cn ) == FAILURE )
				return FAILURE;

			if ( cmd == OPT_ADJUST )
				return SUCCESS;

			if ( patch->p_diff == NOP ) {

				// change network and netmask parameters of an already configured and named HNA

				char old[30];

				// 1. check if announcing the new HNA would not block,
				if ( check_apply_parent_option( ADD, OPT_CHECK, NO, opt, new, cn ) == FAILURE )
					return FAILURE;

				if ( get_tracked_network( opt, patch, old, &ip4, &mask, cn ) == FAILURE )
					return FAILURE;

				// 3. remove the old HNA and hope to not mess it up...
                                set_uhna_key(&key, AF_INET, mask, ip4, metric);

                                if ( cmd == OPT_APPLY)
                                        configure_uhna(DEL, &key, &self);

			}

			// then continue with the new HNA
			if ( str2netw( new , &ip4, '/', cn, &mask, 32 ) == FAILURE )
				return FAILURE;
#else
                        return FAILURE;
#endif
                }


                set_uhna_key(&key, family, mask, &ipX, metric);

                struct uhna_node *un;

                if (patch->p_diff != DEL && (un = (avl_find_item(&global_uhna_tree, &key)))) {

			dbg_cn( cn, DBGL_CHANGES, DBGT_ERR,
                                "UHNA %s/%d metric %d already blocked by %s !",
                                ipXAsStr(key.family, &key.glip), mask, metric,
                                (un->on == &self ? "MYSELF" : un->on->id.name));

                        return FAILURE;
		}

		if ( cmd == OPT_APPLY )
                        configure_uhna((patch->p_diff == DEL ? DEL : ADD), &key, &self);



	} else if ( cmd == OPT_UNREGISTER ) {

                struct uhna_node * un;

                while ((un = avl_first_item(&global_uhna_tree)))
                        configure_uhna(DEL, &un->key, &self);

	}

	return SUCCESS;

}




STATIC_FUNC
struct opt_type hna_options[]= {
//     		ord parent long_name   shrt Attributes				*ival		min		max		default		*function

	{ODI,0,0,			0,  5,0,0,0,0,0,				0,		0,		0,		0,		0,
			0,		"\nHost and Network Announcement (HNA) options:"}
        ,
	{ODI,0,ARG_UHNA,	 	'u',5,A_PMN,A_ADM,A_DYI,A_CFA,A_ANY,	0,		0,		0,		0,		opt_uhna,
			ARG_PREFIX_FORM,"perform host-network announcement (HNA) for defined ip range"}
/*
        ,
	{ODI,ARG_UHNA,ARG_UHNA_NETWORK,	'n',5,A_CS1,A_ADM,A_DYI,A_CFA,A_ANY,	0,		0,		0,		0,		opt_uhna,
			ARG_NETW_FORM, 	"specify network of announcement"}
        ,
	{ODI,ARG_UHNA,ARG_UHNA_PREFIXLEN,'p',5,A_CS1,A_ADM,A_DYI,A_CFA,A_ANY,	0,		0,		0,		0,		opt_uhna,
			ARG_MASK_FORM, 	"specify network prefix of announcement"}
        ,
	{ODI,ARG_UHNA,ARG_UHNA_METRIC,   'm',5,A_CS1,A_ADM,A_DYI,A_CFA,A_ANY,	0,		MIN_UHNA_METRIC,MAX_UHNA_METRIC,DEF_UHNA_METRIC,opt_uhna,
			ARG_VALUE_FORM, "specify hna-metric of announcement (0 means highest preference)"}
*/

};


STATIC_FUNC
void hna_route_change_hook(uint8_t del, struct orig_node *on)
{
        dbgf_all(DBGT_INFO, "%s", on->id.name);

        uint16_t tlvs_length = ntohs(on->desc->dsc_tlvs_len);
        uint8_t uhna_type = (af_cfg == AF_INET ? BMX_DSC_TLV_UHNA4 : BMX_DSC_TLV_UHNA6);

        struct rx_frame_iterator it = {.caller = __FUNCTION__, .on = on,
                .frames_in = (((uint8_t*) on->desc) + sizeof (struct description)),
                .frames_pos = 0, .frames_length = tlvs_length,
                .handls = description_tlv_handl, .handl_max = FRAME_TYPE_MAX, .process_filter = FRAME_TYPE_PROCESS_NONE
        };

        while (rx_frame_iterate(&it) > TLV_RX_DATA_DONE && it.frame_type != uhna_type);

        if (it.frame_type != uhna_type)
                return;

        uint16_t pos;
        uint16_t msg_size = it.handls[it.frame_type].min_msg_size;

        for (pos = 0; pos < it.frame_data_length; pos += msg_size) {

                struct uhna_key key;

                if (uhna_type == BMX_DSC_TLV_UHNA4)
                        set_uhna_to_key(&key, (struct description_msg_hna4 *) (it.frame_data + pos), NULL);

                else
                        set_uhna_to_key(&key, NULL, (struct description_msg_hna6 *) (it.frame_data + pos));


                if (configure_route(del, on, &key) != SUCCESS) {

                        //assertion(-500670, (0));

                }

        }
}









STATIC_FUNC
void hna_cleanup( void )
{
        set_route_change_hooks(hna_route_change_hook, DEL);
}



STATIC_FUNC
int32_t hna_init( void )
{
        struct frame_handl tlv_handl;

        memset( &tlv_handl, 0, sizeof(tlv_handl));
        tlv_handl.min_msg_size = sizeof (struct description_msg_hna4);
        tlv_handl.fixed_msg_size = 1;
        tlv_handl.is_relevant = 1;
        tlv_handl.name = "desc_tlv_uhna4";
        tlv_handl.tx_frame_handler = create_description_tlv_hna;
        tlv_handl.rx_frame_handler = process_description_tlv_hna;
        register_frame_handler(description_tlv_handl, BMX_DSC_TLV_UHNA4, &tlv_handl);


        memset( &tlv_handl, 0, sizeof(tlv_handl));
        tlv_handl.min_msg_size = sizeof (struct description_msg_hna6);
        tlv_handl.fixed_msg_size = 1;
        tlv_handl.is_relevant = 1;
        tlv_handl.name = "desc_tlv_uhna6";
        tlv_handl.tx_frame_handler = create_description_tlv_hna;
        tlv_handl.rx_frame_handler = process_description_tlv_hna;
        register_frame_handler(description_tlv_handl, BMX_DSC_TLV_UHNA6, &tlv_handl);


        set_route_change_hooks(hna_route_change_hook, ADD);

        register_options_array( hna_options, sizeof( hna_options ) );

        return SUCCESS;
}


struct plugin *hna_get_plugin( void ) {

	static struct plugin hna_plugin;
	memset( &hna_plugin, 0, sizeof ( struct plugin ) );

	hna_plugin.plugin_name = "bmx6_hna_plugin";
	hna_plugin.plugin_size = sizeof ( struct plugin );
        hna_plugin.plugin_code_version = CODE_VERSION;
        hna_plugin.cb_init = hna_init;
	hna_plugin.cb_cleanup = hna_cleanup;
        hna_plugin.cb_plugin_handler[PLUGIN_CB_STATUS] = (void (*) (void*)) hna_status;

        return &hna_plugin;
}


