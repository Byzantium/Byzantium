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
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/if.h>     /* ifr_if, ifr_tun */
#include <linux/rtnetlink.h>
#include <time.h>

#include "bmx.h"
#include "msg.h"
#include "ip.h"
#include "schedule.h"
#include "tools.h"
#include "metrics.h"
#include "plugin.h"




int32_t dad_to = DEF_DAD_TO;

int32_t my_ttl = DEF_TTL;


static int32_t Asocial_device = DEF_ASOCIAL;

static int32_t ogm_purge_to = DEF_OGM_PURGE_TO;

int32_t my_tx_interval = DEF_TX_INTERVAL;

int32_t my_ogm_interval = DEF_OGM_INTERVAL;   /* orginator message interval in miliseconds */

static int32_t link_purge_to = DEF_LINK_PURGE_TO;


IDM_T terminating = 0;
IDM_T initializing = YES;

static struct timeval start_time_tv;
static struct timeval ret_tv, new_tv, diff_tv, acceptable_m_tv, acceptable_p_tv, max_tv = {0,(2000*MAX_SELECT_TIMEOUT_MS)};


uint32_t My_pid = 0;
static RNG rng;

TIME_T bmx_time = 0;
TIME_SEC_T bmx_time_sec = 0;


uint32_t s_curr_avg_cpu_load = 0;

IDM_T my_description_changed = YES;

struct orig_node self;



AVL_TREE(link_tree, struct link_node, link_id);
AVL_TREE(blacklisted_tree, struct black_node, dhash);

AVL_TREE(link_dev_tree, struct link_dev_node, key);

AVL_TREE(neigh_tree, struct neigh_node, nnkey);

AVL_TREE(dhash_tree, struct dhash_node, dhash);
AVL_TREE(dhash_invalid_tree, struct dhash_node, dhash);
LIST_SIMPEL( dhash_invalid_plist, struct plist_node, list, list );

AVL_TREE(orig_tree, struct orig_node, id);
AVL_TREE(blocked_tree, struct orig_node, id);



/***********************************************************
 Data Infrastructure
 ************************************************************/



void blacklist_neighbor(struct packet_buff *pb)
{
        TRACE_FUNCTION_CALL;
        dbgf(DBGL_SYS, DBGT_ERR, "%s via %s", pb->i.llip_str, pb->i.iif->label_cfg.str);

        EXITERROR(-500697, (0));
}


IDM_T blacklisted_neighbor(struct packet_buff *pb, struct description_hash *dhash)
{
        TRACE_FUNCTION_CALL;
        //dbgf_all(DBGT_INFO, "%s via %s", pb->i.neigh_str, pb->i.iif->label_cfg.str);
        return NO;
}

IDM_T equal_link_key( struct link_dev_key *a, struct link_dev_key *b )
{
        return (a->dev == b->dev && a->link == b->link);
}

IDM_T validate_param(int32_t probe, int32_t min, int32_t max, char *name)
{

        if ( probe < min || probe > max ) {

                dbgf( DBGL_SYS, DBGT_ERR, "Illegal %s parameter value %d ( min %d  max %d )",
                        name, probe, min, max);

                return FAILURE;
        }

        return SUCCESS;
}

struct dhash_node *is_described_neigh_dhn( struct packet_buff *pb )
{
        assertion(-500730, (pb && pb->i.ln));
        
        if (pb->i.ln->neigh && pb->i.ln->neigh->dhn && pb->i.ln->neigh->dhn->on &&
                pb->i.ln->neigh->dhn == iid_get_node_by_neighIID4x(pb->i.ln->neigh, pb->i.transmittersIID)) {

                return pb->i.ln->neigh->dhn;
        }

        return NULL;
}









STATIC_FUNC
void assign_best_rtq_link(struct neigh_node *nn)
{
        TRACE_FUNCTION_CALL;
	struct list_node *lndev_pos;
        struct link_dev_node *lndev_best = NULL;
        struct link_node *ln;
        struct avl_node *an = NULL;

        assertion( -500451, (nn));

        dbgf_all( DBGT_INFO, "%s", nn->dhn->on->id.name);

        while ((ln = avl_iterate_item(&nn->link_tree, &an))) {

                list_for_each(lndev_pos, &ln->lndev_list)
                {
                        struct link_dev_node *lndev = list_entry(lndev_pos, struct link_dev_node, list);

                        if (!lndev_best ||
                                lndev->mr[SQR_RTQ].umetric_final > lndev_best->mr[SQR_RTQ].umetric_final ||
                                U32_GT(lndev->rtq_time_max, lndev_best->rtq_time_max)
                                )
                                lndev_best = lndev;

                }

        }
        assertion( -500406, (lndev_best));

        nn->best_rtq = lndev_best;

}



STATIC_FUNC
void free_neigh_node(struct neigh_node *nn)
{
        TRACE_FUNCTION_CALL;
        paranoia(-500321, (nn->link_tree.items));

        dbgf_all(DBGT_INFO, "freeing %s", nn && nn->dhn && nn->dhn->on ? nn->dhn->on->id.name : "---");

        avl_remove(&neigh_tree, &nn->nnkey, -300196);
        iid_purge_repos(&nn->neighIID4x_repos);
        debugFree(nn, -300129);
}



STATIC_FUNC
struct neigh_node *create_neigh_node(struct link_node *ln, struct dhash_node * dhn)
{
        TRACE_FUNCTION_CALL;
        assertion( -500400, ( ln && !ln->neigh && dhn && !dhn->neigh ) );

        struct neigh_node *nn = debugMalloc(sizeof ( struct neigh_node), -300131);

        memset(nn, 0, sizeof ( struct neigh_node));

        AVL_INIT_TREE(nn->link_tree, struct link_node, link_id);

        avl_insert(&nn->link_tree, ln, -300172);

        nn->dhn = dhn;

        nn->nnkey = nn;
        avl_insert(&neigh_tree, nn, -300141);

        dhn->neigh = ln->neigh = nn;

        return nn;
}



IDM_T update_neigh_node(struct link_node *ln, struct dhash_node *dhn, IID_T neighIID4neigh, struct description *dsc)
{
        TRACE_FUNCTION_CALL;
        struct neigh_node *neigh = NULL;

        dbgf_all( DBGT_INFO, "link_id=0x%X  NB=%s neighIID4neigh=%d  dhn->orig=%s",
                ln->link_id, ipXAsStr(af_cfg, &ln->link_ip), neighIID4neigh, dhn->on->desc->id.name);

        assertion(-500389, (ln && neighIID4neigh > IID_RSVD_MAX));
        assertion(-500390, (dhn && dhn->on));

        if (ln->neigh) {

                assertion(-500405, (ln->neigh->dhn && ln->neigh->dhn->on ));
                assertion(-500391, (ln->neigh->dhn->neigh == ln->neigh));
                ASSERTION(-500392, (avl_find(&ln->neigh->link_tree, &ln->link_id)));

                if (ln->neigh == dhn->neigh) {

                        assertion(-500393, (ln->neigh->dhn == dhn));

                        neigh = ln->neigh;
                        assertion(-500518, (neigh->dhn != self.dhn));


                } else {

                        //neigh = merge_neigh_nodes( ln, dhn); //they are the same!
                        dbgf(DBGL_SYS, DBGT_ERR,
                                "Neigh restarted ?!! purging ln %s %jX and dhn %s %jX, neighIID4neigh %d dsc %s",
                                ln->neigh->dhn->on->desc->id.name, ln->neigh->dhn->on->desc->id.rand.u64[0],
                                dhn->on->desc->id.name, dhn->on->desc->id.rand.u64[0],
                                neighIID4neigh, dsc ? "YES" : "NO");


                        free_dhash_node( dhn );
                        free_dhash_node( ln->neigh->dhn );

                        return FAILURE;
                }

        } else {

                if ( dhn->neigh ) {

                        assertion(-500394, (dhn->neigh->dhn == dhn));
                        assertion(-500395, (dhn->neigh->link_tree.items));
                        ASSERTION(-500396, (!avl_find(&dhn->neigh->link_tree, &ln->link_id)));

                        neigh = ln->neigh = dhn->neigh;
                        avl_insert(&neigh->link_tree, ln, -300173);

                        assertion(-500516, (neigh->dhn != self.dhn));


                } else {

                        neigh = create_neigh_node( ln, dhn );
                        assertion(-500517, (neigh->dhn != self.dhn));
                }
        }

        assign_best_rtq_link(neigh);

        assertion(-500510, (neigh->dhn != self.dhn));
        return SUCCESS;
}


STATIC_FUNC
struct link_dev_node *get_link_dev_node(struct link_node *ln, struct dev_node *dev)
{
        TRACE_FUNCTION_CALL;
	struct list_node *lndev_pos;
	struct link_dev_node *lndev;

        assertion(-500607, (dev));

	list_for_each( lndev_pos, &ln->lndev_list ) {

		lndev = list_entry( lndev_pos, struct link_dev_node, list );

		if ( lndev->key.dev == dev )
			return lndev;
	}


	lndev = debugMalloc( sizeof( struct link_dev_node ), -300023 );

	memset( lndev, 0, sizeof( struct link_dev_node ) );

	lndev->key.dev = dev;
	lndev->key.link = ln;

        int i;
        for (i = 0; i < FRAME_TYPE_ARRSZ; i++) {
                LIST_INIT_HEAD(lndev->tx_task_lists[i], struct tx_task_node, list);
        }


        dbgf(DBGL_CHANGES, DBGT_INFO, "creating new lndev %16s %s", ipXAsStr(af_cfg, &ln->link_ip), dev->name_phy_cfg.str);

        list_add_tail(&ln->lndev_list, &lndev->list);

        ASSERTION( -500489, !avl_find( &link_dev_tree, &lndev->key));

        avl_insert( &link_dev_tree, lndev, -300220 );

        cb_plugin_hooks( PLUGIN_CB_LINKDEV_CREATE, lndev);

	return lndev;
}





struct dhash_node* create_dhash_node(struct description_hash *dhash, struct orig_node *on)
{
        TRACE_FUNCTION_CALL;

        struct dhash_node * dhn = debugMalloc(sizeof ( struct dhash_node), -300001);
        memset(dhn, 0, sizeof ( struct dhash_node));
        memcpy(&dhn->dhash, dhash, HASH0_SHA1_LEN);
        avl_insert(&dhash_tree, dhn, -300142);

        dhn->myIID4orig = iid_new_myIID4x(dhn);

        on->updated_timestamp = bmx_time;
        dhn->on = on;
        on->dhn = dhn;

        dbgf(DBGL_CHANGES, DBGT_INFO, "dhash %8X.. myIID4orig %d", dhn->dhash.h.u32[0], dhn->myIID4orig);

        return dhn;
}

STATIC_FUNC
void purge_dhash_iid(struct dhash_node *dhn)
{
        TRACE_FUNCTION_CALL;
        struct avl_node *an;
        struct neigh_node *nn;

        //reset all neigh_node->oid_repos[x]=dhn->mid4o entries
        for (an = NULL; (nn = avl_iterate_item(&neigh_tree, &an));) {

                iid_free_neighIID4x_by_myIID4x(&nn->neighIID4x_repos, dhn->myIID4orig);

        }
}

STATIC_FUNC
 void purge_dhash_invalid_list( IDM_T force_purge_all ) {

        TRACE_FUNCTION_CALL;
        struct dhash_node *dhn;

        dbgf_all( DBGT_INFO, "%s", force_purge_all ? "force_purge_all" : "only_expired");

        while ((dhn = plist_get_first(&dhash_invalid_plist)) ) {

                if (force_purge_all || ((uint32_t) (bmx_time - dhn->referred_by_me_timestamp) > MIN_DHASH_TO)) {

                        dbgf_all( DBGT_INFO, "dhash %8X myIID4orig %d", dhn->dhash.h.u32[0], dhn->myIID4orig);

                        plist_rem_head(&dhash_invalid_plist);
                        avl_remove(&dhash_invalid_tree, &dhn->dhash, -300194);

                        iid_free(&my_iid_repos, dhn->myIID4orig);

                        purge_dhash_iid(dhn);

                        debugFree(dhn, -300112);

                } else {
                        break;
                }
        }
}

STATIC_FUNC
void unlink_dhash_node(struct dhash_node *dhn)
{
        TRACE_FUNCTION_CALL;
        dbgf_all(DBGT_INFO, "dhash %8X myIID4orig %d", dhn->dhash.h.u32[0], dhn->myIID4orig);

        if (dhn->neigh) {

                struct avl_node *an;
                struct link_node *ln;

                while ((an = dhn->neigh->link_tree.root) && (ln = an->item)) {

                        dbgf_all(DBGT_INFO, "dhn->neigh->link_tree item link_id=0x%X", ln->link_id);

                        assertion(-500284, (ln->neigh == dhn->neigh));

                        ln->neigh = NULL;

                        avl_remove(&dhn->neigh->link_tree, &ln->link_id, -300197);
                }

                free_neigh_node(dhn->neigh);
                
                dhn->neigh = NULL;
        }

        if (dhn->on) {
                dhn->on->dhn = NULL;
                free_orig_node( dhn->on );
                dhn->on = NULL;
        }

        avl_remove(&dhash_tree, &dhn->dhash, -300195);
}


void free_dhash_node( struct dhash_node *dhn )
{
        TRACE_FUNCTION_CALL;
        static uint32_t blocked_counter = 1;

        dbgf(terminating ? DBGL_CHANGES : DBGL_SYS, DBGT_INFO,
                "dhash %8X myIID4orig %d", dhn->dhash.h.u32[0], dhn->myIID4orig);

        unlink_dhash_node(dhn); // will clear dhn->on and dhn->neigh

        purge_dhash_iid(dhn);

        // It must be ensured that I am not reusing this IID for a while, so it must be invalidated
        // but the description and its' resulting dhash might become valid again, so I give it a unique and illegal value.
        memset(&dhn->dhash, 0, sizeof ( struct description_hash));
        dhn->dhash.h.u32[(sizeof ( struct description_hash) / sizeof (uint32_t)) - 1] = blocked_counter++;

        avl_insert(&dhash_invalid_tree, dhn, -300168);
        plist_add_tail(&dhash_invalid_plist, dhn);
        dhn->referred_by_me_timestamp = bmx_time;
}


// called due to updated description
void invalidate_dhash_node( struct dhash_node *dhn )
{
        TRACE_FUNCTION_CALL;

        dbgf(terminating ? DBGL_ALL : DBGL_SYS, DBGT_INFO,
                "dhash %8X myIID4orig %d, my_iid_repository: used %d, inactive %d  min_free %d  max_free %d ",
                dhn->dhash.h.u32[0], dhn->myIID4orig,
                my_iid_repos.tot_used, dhash_invalid_tree.items+1, my_iid_repos.min_free, my_iid_repos.max_free);

        assertion( -500698, (!dhn->on));
        assertion( -500699, (!dhn->neigh));
        unlink_dhash_node(dhn); // would clear dhn->on and dhn->neigh

        avl_insert(&dhash_invalid_tree, dhn, -300168);
        plist_add_tail(&dhash_invalid_plist, dhn);
        dhn->referred_by_me_timestamp = bmx_time;
}





STATIC_FUNC
void purge_router_tree(struct orig_node *on_key, struct link_dev_key *rt_key, IDM_T purge_all)
{
        TRACE_FUNCTION_CALL;
        struct orig_node *on;
        struct avl_node *an = NULL;
        while ((on = on_key) || (on = avl_iterate_item( &orig_tree, &an))) {

                struct link_dev_key key;
                memset(&key, 0, sizeof (key));
                struct router_node *rn;

                while (rt_key ? (rn = avl_find_item(&on->rt_tree, rt_key)) : (rn = avl_next_item(&on->rt_tree, &key))) {

                        key = rn->key;

                        if (purge_all || rt_key || !avl_find(&link_dev_tree, &key)) {

                                if (on->best_rn == rn)
                                        on->best_rn = NULL;

                                if (on->curr_rn == rn) {

                                        set_ogmSqn_toBeSend_and_aggregated(on, on->ogmSqn_maxRcvd, on->ogmSqn_maxRcvd);

                                        cb_route_change_hooks(DEL, on);
                                        on->curr_rn = NULL;
                                }

                                avl_remove(&on->rt_tree, &rn->key, -300226);

                                debugFree(rn, -300225);
                        }

                        if (rt_key)
                                break;
                }

                if (on_key)
                        break;
        }
}


STATIC_FUNC
void purge_link_node(LINK_ID_T only_link_id, struct dev_node *only_dev, IDM_T only_expired)
{
        TRACE_FUNCTION_CALL;
        struct avl_node *link_an;
        struct link_node *ln;
        LINK_ID_T it_link_id = LINK_ID_INVALID;

        dbgf_all( DBGT_INFO, "link_id=0x%X %s %s only_expired",
                only_link_id, only_dev ? only_dev->label_cfg.str : "---", only_expired ? " " : "not");

        while ((link_an = (only_link_id >= LINK_ID_MIN ?
                (avl_find(&link_tree, &only_link_id)) : (avl_next(&link_tree, &it_link_id)))) && (ln = link_an->item)) {

                struct list_node *pos, *tmp, *prev = (struct list_node *) & ln->lndev_list;
                struct neigh_node *nn = ln->neigh;
                IDM_T removed_lndev = NO;

                it_link_id = ln->link_id;

                list_for_each_safe(pos, tmp, &ln->lndev_list)
                {
                        struct link_dev_node *lndev = list_entry(pos, struct link_dev_node, list);

                        if ((!only_dev || only_dev == lndev->key.dev) &&
                                (!only_expired || (((TIME_T) (bmx_time - lndev->pkt_time_max)) > (TIME_T) link_purge_to))) {

                                dbgf(DBGL_CHANGES, DBGT_INFO, "purging lndev=%s dev=%10s",
                                        ipXAsStr(af_cfg, &ln->link_ip), lndev->key.dev->label_cfg.str);

                                cb_plugin_hooks( PLUGIN_CB_LINKDEV_DESTROY, lndev);

                                purge_router_tree(NULL, &lndev->key, NO);

                                purge_tx_task_list(lndev->tx_task_lists, NULL, NULL);

                                list_del_next(&ln->lndev_list, prev);
                                avl_remove(&link_dev_tree, &lndev->key, -300221);
                                debugFree(pos, -300044);
                                removed_lndev = YES;

                        } else {
                                prev = pos;
                        }
                }

                assertion(-500323, (only_dev || only_expired || ln->lndev_list.items==0));

                if (!ln->lndev_list.items) {

                        dbgf(DBGL_CHANGES, DBGT_INFO, "purging: link_id=0x%X link_ip=%s %s",
                                ln->link_id, ipXAsStr(af_cfg, &ln->link_ip), only_dev ? only_dev->label_cfg.str : "???");

                        cb_plugin_hooks(PLUGIN_CB_LINK_DESTROY, ln);

                        if (nn) {

                                avl_remove(&nn->link_tree, &ln->link_id, -300198);

                                if (!nn->link_tree.items) {

                                        if (nn->dhn) {
                                                nn->dhn->neigh = NULL;
                                        }

                                        free_neigh_node(nn);
                                        nn = NULL;
                                }
                        }

                        struct avl_node *dev_an;
                        struct dev_node *dev;
                        for(dev_an = NULL; (dev = avl_iterate_item(&dev_ip_tree, &dev_an));) {
                                purge_tx_task_list(dev->tx_task_lists, ln, NULL);
                        }

                        avl_remove(&link_tree, &ln->link_id, -300193);

                        debugFree( ln, -300045 );
                }

                if (nn && removed_lndev)
                        assign_best_rtq_link(nn);

                if (only_link_id >= LINK_ID_MIN)
                        break;
        }
}


void free_orig_node(struct orig_node *on)
{
        TRACE_FUNCTION_CALL;
        dbgf(DBGL_CHANGES, DBGT_INFO, "%s %s", on->id.name, on->primary_ip_str);

        if ( on == &self)
                return;

        //cb_route_change_hooks(DEL, on, 0, &on->ort.rt_key.llip);

        purge_router_tree(on, NULL, YES);

        if (on->desc && !on->blocked)
                process_description_tlvs(NULL, on, on->desc, TLV_OP_DEL, FRAME_TYPE_PROCESS_ALL, NULL);

        if ( on->dhn ) {
                on->dhn->on = NULL;
                free_dhash_node(on->dhn);
        }

        avl_remove(&orig_tree, &on->id, -300200);
        avl_remove(&blocked_tree, &on->id, -300201);

        if (on->desc)
                debugFree(on->desc, -300228);

        debugFree( on, -300086 );
}


void purge_link_and_orig_nodes(struct dev_node *only_dev, IDM_T only_expired)
{
        TRACE_FUNCTION_CALL;

        dbgf_all( DBGT_INFO, "%s %s only expired",
                only_dev ? only_dev->label_cfg.str : "---", only_expired ? " " : "NOT");

        purge_link_node(LINK_ID_INVALID, only_dev, only_expired);

        int i;
        for (i = IID_RSVD_MAX + 1; i < my_iid_repos.max_free; i++) {

                struct dhash_node *dhn;

                if ((dhn = my_iid_repos.arr.node[i]) && dhn->on) {

                        if (!only_dev && (!only_expired ||
                                ((TIME_T) (bmx_time - dhn->referred_by_me_timestamp)) > (TIME_T) ogm_purge_to)) {

                                dbgf(terminating ? DBGL_ALL : DBGL_SYS, DBGT_INFO, "%s referred before: %d > purge_to=%d",
                                        dhn->on->id.name, ((TIME_T) (bmx_time - dhn->referred_by_me_timestamp)), (TIME_T) ogm_purge_to);

                                free_orig_node(dhn->on);
                        }
                }
        }
}


LINK_ID_T new_link_id(struct dev_node *dev)
{
        uint16_t tries = 0;
        LINK_ID_T link_id = 0;

        dev->link_id_timestamp = bmx_time;

        do {
                link_id |= (((LINK_ID_T) (dev->mac.u8[5])) << 0);
                link_id |= (((LINK_ID_T) (dev->mac.u8[4])) << 8);
                link_id |= (((LINK_ID_T) (dev->mac.u8[3])) << 16);
                link_id |= (((LINK_ID_T) ((link_id < LINK_ID_MIN ? (rand_num(((uint8_t) - 1) - LINK_ID_MIN) + LINK_ID_MIN) : (rand_num(((uint8_t) - 1)))))) << 24);

#ifdef TEST_LINK_ID_COLLISION_DETECTION
                link_id = (tries || dev->link_id) ? link_id : LINK_ID_MIN;
#endif


                struct avl_node *an;
                struct dev_node *tmp;

                for (an = NULL; (tmp = avl_iterate_item(&dev_ip_tree, &an));) {
                        if (tmp->link_id == link_id)
                                break;
                }

                if (!tmp && !avl_find_item(&link_tree, &link_id)) {
                        dev->link_id = link_id;
                        return link_id;
                }

                tries++;

                if (tries % LINK_ID_ITERATIONS_WARN == 0) {
                        dbgf(DBGL_SYS, DBGT_ERR, "No free link_id after %d trials (link_tree.items=%d, dev_ip_tree.items=%d)!",
                                tries, link_tree.items, dev_ip_tree.items);
                }

        } while (tries < LINK_ID_ITERATIONS_MAX);


        dev->link_id = LINK_ID_INVALID;
        return LINK_ID_INVALID;
}

STATIC_FUNC
struct link_node *get_link_node(struct packet_buff *pb)
{
        TRACE_FUNCTION_CALL;

        dbgf_all(DBGT_INFO, "NB=%s, link_id=0x%X", pb->i.llip_str, pb->i.link_id);

        struct link_node *ln = avl_find_item(&link_tree, &pb->i.link_id);

        if (ln && !is_ip_equal(&pb->i.llip, &ln->link_ip)) {

                if (((TIME_T) (bmx_time - ln->pkt_time_max)) < (TIME_T) dad_to) {

                        dbgf(DBGL_SYS, DBGT_WARN,
                                " DAD-Alert (link_id collision, this can happen)! NB=%s via dev=%s"
                                "cached llIP=%s link_id=0x%X ! sending problem adv...",
                                pb->i.llip_str, pb->i.iif->label_cfg.str, ipXAsStr(af_cfg, &ln->link_ip), pb->i.link_id);

                        // better be carefull here. Errornous PROBLEM_ADVs cause neighboring nodes to cease!!!
                        struct link_dev_node dummy_lndev = {.key = {.dev = pb->i.iif, .link = ln}, .mr = {ZERO_METRIC_RECORD,ZERO_METRIC_RECORD}};

                        schedule_tx_task(&dummy_lndev, FRAME_TYPE_PROBLEM_ADV, sizeof (struct msg_problem_adv),
                                FRAME_TYPE_PROBLEM_CODE_DUP_LINK_ID, ln->link_id, 0, pb->i.transmittersIID);

                        // we keep this entry until it timeouts
                        return NULL;
                }

                dbgf(DBGL_SYS, DBGT_WARN, "Reinitialized! NB=%s via dev=%s "
                        "cached llIP=%s link_id=0x%X ! Reinitializing link_node...",
                        pb->i.llip_str, pb->i.iif->label_cfg.str, ipXAsStr(af_cfg, &ln->link_ip), pb->i.link_id);

                purge_link_node(ln->link_id, NULL, NO);
                ASSERTION(-500213, !avl_find(&link_tree, &pb->i.link_id));
                ln = NULL;

        }

        if (!ln) {

                ln = debugMalloc(sizeof (struct link_node), -300024);
                memset(ln, 0, sizeof (struct link_node));

                LIST_INIT_HEAD(ln->lndev_list, struct link_dev_node, list);


                ln->link_id = pb->i.link_id;
                ln->link_ip = pb->i.llip;

                avl_insert(&link_tree, ln, -300147);

                cb_plugin_hooks(PLUGIN_CB_LINK_CREATE, ln);

                dbgf(DBGL_CHANGES, DBGT_INFO, "new %s (total %d)", pb->i.llip_str, link_tree.items);

        }

        ln->pkt_time_max = bmx_time;

        return ln;
}



void rx_packet( struct packet_buff *pb )
{
        TRACE_FUNCTION_CALL;
        struct dev_node *oif, *iif = pb->i.iif;

        assertion(-500841, ((iif->active && iif->if_llocal_addr)));

        if (af_cfg == AF_INET) {
                ip42X(&pb->i.llip, (*((struct sockaddr_in*)&(pb->i.addr))).sin_addr.s_addr);

        } else {
                pb->i.llip = (*((struct sockaddr_in6*) &(pb->i.addr))).sin6_addr;

                if (!is_ip_net_equal(&pb->i.llip, &IP6_LINKLOCAL_UC_PREF, IP6_LINKLOCAL_UC_PLEN, AF_INET6)) {
                        dbgf_all(DBGT_ERR, "non-link-local IPv6 source address %s", ip6Str(&pb->i.llip));
                        return;
                }
        }
        //TODO: check broadcast source!!

        struct packet_header *hdr = &pb->packet.header;
        uint16_t pkt_length = ntohs(hdr->pkt_length);
        pb->i.transmittersIID = ntohs(hdr->transmitterIID);
        pb->i.link_id = ntohl(hdr->link_id);
        ip2Str(af_cfg, &pb->i.llip, pb->i.llip_str);

        dbgf_all(DBGT_INFO, "via %s %s %s size %d", iif->label_cfg.str, iif->ip_llocal_str, pb->i.llip_str, pkt_length);

	// immediately drop invalid packets...
	// we acceppt longer packets than specified by pos->size to allow padding for equal packet sizes
        if (    pb->i.total_length < (int) (sizeof (struct packet_header) + sizeof (struct frame_header_long)) ||
                pkt_length < (int) (sizeof (struct packet_header) + sizeof (struct frame_header_long)) ||
                hdr->bmx_version != COMPATIBILITY_VERSION ||
                pkt_length > pb->i.total_length || pkt_length > MAX_UDPD_SIZE ||
                pb->i.link_id < LINK_ID_MIN) {

                goto process_packet_error;
        }

        if ((oif = avl_find_item(&dev_ip_tree, &pb->i.llip))) {

                ASSERTION(-500840, (oif == iif)); // so far, only unique own interface IPs are allowed!!

                if (((oif->link_id != pb->i.link_id) && (((TIME_T) (bmx_time - oif->link_id_timestamp)) > (4 * (TIME_T) my_tx_interval))) ||
                        ((myIID4me != pb->i.transmittersIID) && (((TIME_T) (bmx_time - myIID4me_timestamp)) > (4 * (TIME_T) my_tx_interval)))) {

                        // own link_id  or myIID4me might have just changed and then, due to delay,
                        // I might receive my own packet back containing my previous (now non-matching)link_id of myIID4me
                        dbgf(DBGL_SYS, DBGT_ERR, "DAD-Alert (duplicate Address) from NB=%s via dev=%s "
                                "my_link_id=0x%X rcvd_link_id=0x%X   myIID4me=%d rcvdIID=%d",
                                oif->ip_llocal_str, iif->label_cfg.str, iif->link_id, pb->i.link_id,
                                myIID4me, pb->i.transmittersIID);

                        goto process_packet_error;
                }

                return;
        }

        if (iif->link_id == pb->i.link_id) {

                if (new_link_id(iif) == LINK_ID_INVALID) {
                        DEV_MARK_PROBLEM(iif);
                        goto process_packet_error;
                }

                dbgf(DBGL_SYS, DBGT_WARN, "DAD-Alert (duplicate link ID, this can happen) via dev=%s NB=%s "
                        "is using my link_id=0x%X!  Choosing new link_id=0x%X for myself, dropping packet",
                        iif->label_cfg.str, pb->i.llip_str, pb->i.link_id, iif->link_id);

                return;
        }

        if (!(pb->i.ln = get_link_node(pb)))
                return;

        if (!(pb->i.lndev = get_link_dev_node(pb->i.ln, pb->i.iif)))
                return;

        pb->i.lndev->pkt_time_max = bmx_time;
        pb->i.described_dhn = is_described_neigh_dhn(pb);


        dbgf_all(DBGT_INFO, "version=%i, reserved=%X, size=%i IID=%d rcvd udp_len=%d via NB %s %s %s",
                hdr->bmx_version, hdr->reserved, pkt_length, pb->i.transmittersIID,
                pb->i.total_length, pb->i.llip_str, iif->label_cfg.str, pb->i.unicast ? "UNICAST" : "BRC");


        cb_packet_hooks(pb);

        if (blacklisted_neighbor(pb, NULL))
                return;

        if (rx_frames(pb) == SUCCESS)
                return;


process_packet_error:

        dbgf(DBGL_SYS, DBGT_WARN,
                "Drop (remaining) packet: rcvd problematic packet via NB=%s dev=%s "
                "(version=%i, link_id=0x%X, reserved=0x%X, pkt_size=%i), udp_len=%d my_version=%d, max_udpd_size=%d",
                pb->i.llip_str, iif->label_cfg.str,
                hdr->bmx_version, pb->i.link_id, hdr->reserved, pkt_length, pb->i.total_length,
                COMPATIBILITY_VERSION, MAX_UDPD_SIZE);

        blacklist_neighbor(pb);

        return;
}




/***********************************************************
 Runtime Infrastructure
************************************************************/


#ifndef NO_TRACE_FUNCTION_CALLS
char* function_call_buffer_name_array[FUNCTION_CALL_BUFFER_SIZE] = {0};
TIME_T function_call_buffer_time_array[FUNCTION_CALL_BUFFER_SIZE] = {0};
uint8_t function_call_buffer_pos = 0;

static void debug_function_calls(void)
{
        uint8_t i;
        for (i = function_call_buffer_pos + 1; i != function_call_buffer_pos; i = ((i+1) % FUNCTION_CALL_BUFFER_SIZE)) {

                if (!function_call_buffer_name_array[i])
                        continue;

                dbgf(DBGL_SYS, DBGT_ERR, "%10d %s()", function_call_buffer_time_array[i], function_call_buffer_name_array[i]);

        }
}


void trace_function_call(const char *func)
{
        if (function_call_buffer_name_array[function_call_buffer_pos] != func) {
                function_call_buffer_time_array[function_call_buffer_pos] = bmx_time;
                function_call_buffer_name_array[function_call_buffer_pos] = (char*)func;
                function_call_buffer_pos = ((function_call_buffer_pos+1) % FUNCTION_CALL_BUFFER_SIZE);
        }
}


#endif

void upd_time(struct timeval *precise_tv)
{

	timeradd( &max_tv, &new_tv, &acceptable_p_tv );
	timercpy( &acceptable_m_tv, &new_tv );
	gettimeofday( &new_tv, NULL );

	if ( timercmp( &new_tv, &acceptable_p_tv, > ) ) {

		timersub( &new_tv, &acceptable_p_tv, &diff_tv );
		timeradd( &start_time_tv, &diff_tv, &start_time_tv );

		dbg( DBGL_SYS, DBGT_WARN,
		     "critical system time drift detected: ++ca %ld s, %ld us! Correcting reference!",
		     diff_tv.tv_sec, diff_tv.tv_usec );

                if ( diff_tv.tv_sec > CRITICAL_PURGE_TIME_DRIFT )
                        purge_link_and_orig_nodes(NULL, NO);

	} else 	if ( timercmp( &new_tv, &acceptable_m_tv, < ) ) {

		timersub( &acceptable_m_tv, &new_tv, &diff_tv );
		timersub( &start_time_tv, &diff_tv, &start_time_tv );

		dbg( DBGL_SYS, DBGT_WARN,
		     "critical system time drift detected: --ca %ld s, %ld us! Correcting reference!",
		     diff_tv.tv_sec, diff_tv.tv_usec );

                if ( diff_tv.tv_sec > CRITICAL_PURGE_TIME_DRIFT )
                        purge_link_and_orig_nodes(NULL, NO);

	}

	timersub( &new_tv, &start_time_tv, &ret_tv );

	if ( precise_tv ) {
		precise_tv->tv_sec = ret_tv.tv_sec;
		precise_tv->tv_usec = ret_tv.tv_usec;
	}

	bmx_time = ( (ret_tv.tv_sec * 1000) + (ret_tv.tv_usec / 1000) );
	bmx_time_sec = ret_tv.tv_sec;

}

char *get_human_uptime(uint32_t reference)
{
	//                  DD:HH:MM:SS
	static char ut[32]="00:00:00:00";

	sprintf( ut, "%i:%i%i:%i%i:%i%i",
	         (((bmx_time_sec-reference)/86400)),
	         (((bmx_time_sec-reference)%86400)/36000)%10,
	         (((bmx_time_sec-reference)%86400)/3600)%10,
	         (((bmx_time_sec-reference)%3600)/600)%10,
	         (((bmx_time_sec-reference)%3600)/60)%10,
	         (((bmx_time_sec-reference)%60)/10)%10,
	         (((bmx_time_sec-reference)%60))%10
	       );

	return ut;
}


void wait_sec_msec(TIME_SEC_T sec, TIME_T msec)
{

        TRACE_FUNCTION_CALL;
	struct timeval time;

	//no debugging here because this is called from debug_output() -> dbg_fprintf() which may case a loop!

	time.tv_sec = sec + (msec/1000) ;
	time.tv_usec = ( msec * 1000 ) % 1000000;

	select( 0, NULL, NULL, NULL, &time );

	return;
}

static void handler(int32_t sig)
{

        TRACE_FUNCTION_CALL;
	if ( !Client_mode ) {
		dbgf( DBGL_SYS, DBGT_ERR, "called with signal %d", sig);
	}

	printf("\n");// to have a newline after ^C

	terminating = 1;
        cb_plugin_hooks(PLUGIN_CB_TERM, NULL);

}




static int cleaning_up = NO;

static void segmentation_fault(int32_t sig)
{
        TRACE_FUNCTION_CALL;
        static int segfault = NO;

        if (!segfault) {

                segfault = YES;

                dbg(DBGL_SYS, DBGT_ERR, "First SIGSEGV %d received, try cleaning up...", sig);

#ifndef NO_TRACE_FUNCTION_CALLS
                debug_function_calls();
#endif

                dbg(DBGL_SYS, DBGT_ERR, "Terminating with error code %d (%s-%s-cv%d)! Please notify a developer",
                        sig, BMX_BRANCH, BRANCH_VERSION, CODE_VERSION);

                if (initializing) {
                        dbg(DBGL_SYS, DBGT_ERR,
                        "check up-to-dateness of bmx libs in default lib path %s or customized lib path defined by %s !",
                        BMX_DEF_LIB_PATH, BMX_ENV_LIB_PATH);
                }

                if (!cleaning_up)
                        cleanup_all(CLEANUP_RETURN);

                dbg(DBGL_SYS, DBGT_ERR, "raising SIGSEGV again ...");

        } else {
                dbg(DBGL_SYS, DBGT_ERR, "Second SIGSEGV %d received, giving up! core contains second SIGSEV!", sig);
        }

        signal(SIGSEGV, SIG_DFL);
        errno=0;
	if ( raise( SIGSEGV ) ) {
		dbg( DBGL_SYS, DBGT_ERR, "raising SIGSEGV failed: %s...", strerror(errno) );
        }
}


void cleanup_all(int32_t status)
{
        TRACE_FUNCTION_CALL;

        if (status < 0) {
                segmentation_fault(status);
        }

        if (!cleaning_up) {

                dbgf_all(DBGT_INFO, "cleaning up (status %d)...", status);

                cleaning_up = YES;

                // first, restore defaults...

		terminating = 1;

		cleanup_schedule();

                if (self.dhn) {
                        self.dhn->on = NULL;
                        free_dhash_node(self.dhn);
                }

                avl_remove(&orig_tree, &(self.id), -300203);

                purge_link_and_orig_nodes(NULL, NO);

		cleanup_plugin();

		cleanup_config();

                cleanup_ip();

                purge_dhash_invalid_list(YES);


		// last, close debugging system and check for forgotten resources...

		cleanup_control();

                checkLeak();

                dbgf_all( DBGT_ERR, "...cleaning up done");

                if (status == CLEANUP_SUCCESS) {

                        exit(EXIT_SUCCESS);

                } else if (status == CLEANUP_FAILURE) {

                        exit(EXIT_FAILURE);

                } else if (status == CLEANUP_RETURN) {

                        return;

                }

                exit(EXIT_FAILURE);
                dbg(DBGL_SYS, DBGT_ERR, "exit ignored!?");
        }
}











/***********************************************************
 Configuration data and handlers
************************************************************/


STATIC_FUNC
int32_t opt_status(uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn)
{
        TRACE_FUNCTION_CALL;

        if ( cmd == OPT_APPLY ) {

                if (!strcmp(opt->long_name, ARG_STATUS)) {

                        dbg_printf(cn, "%s-%s (compatibility=%d code=cv%d) primary %s=%s ip=%s uptime=%s CPU=%d.%1d\n",
                                BMX_BRANCH, BRANCH_VERSION, COMPATIBILITY_VERSION, CODE_VERSION,
                                primary_dev_cfg->arg_cfg, primary_dev_cfg->label_cfg.str, self.primary_ip_str,
                                get_human_uptime(0), s_curr_avg_cpu_load / 10, s_curr_avg_cpu_load % 10);

                        cb_plugin_hooks(PLUGIN_CB_STATUS, cn);

                        dbg_printf(cn, "\n");

                } else  if ( !strcmp( opt->long_name, ARG_LINKS ) ) {
#define DBG_STATUS4_LINK_HEAD "%-16s %-10s %3s %3s %3s %7s %1s %7s %8s %8s %5s %5s %4s %4s\n"
#define DBG_STATUS6_LINK_HEAD "%-30s %-10s %3s %3s %3s %7s %1s %7s %8s %8s %5s %5s %4s %4s\n"
#define DBG_STATUS4_LINK_INFO "%-16s %-10s %3ju %3ju %3ju %7ju %1s %7ju %8X %8X %5d %5d %4d %4s\n"
#define DBG_STATUS6_LINK_INFO "%-30s %-10s %3ju %3ju %3ju %7ju %1s %7ju %8X %8X %5d %5d %4d %4s\n"

                        dbg_printf(cn, (af_cfg == AF_INET ? DBG_STATUS4_LINK_HEAD : DBG_STATUS6_LINK_HEAD),
                                "LinkLocalIP", "viaIF", "RTQ", "RQ", "TQ", "linkM/k", " ", "bestM/k",
                                "myLinkID", "nbLinkID", "nbIID", "lSqn", "lVld", "pref");

                        struct avl_node *it;
                        struct link_node *ln;

                        for (it = NULL; (ln = avl_iterate_item(&link_tree, &it));) {

                                struct neigh_node *nn = ln->neigh;

                                struct list_node *lndev_pos;

                                list_for_each( lndev_pos, &ln->lndev_list )
                                {
                                        struct link_dev_node *lndev = list_entry(lndev_pos, struct link_dev_node, list);
                                        UMETRIC_T bestM = ln->neigh && ln->neigh->dhn && ln->neigh->dhn->on->best_rn ? ln->neigh->dhn->on->best_rn->mr.umetric_final : 0;

                                        dbg_printf(cn, (af_cfg == AF_INET ? DBG_STATUS4_LINK_INFO : DBG_STATUS6_LINK_INFO),
                                                ipXAsStr(af_cfg, &ln->link_ip),
                                                lndev->key.dev->label_cfg.str,
                                                (((lndev->mr[SQR_RTQ].umetric_final) *100)/lndev->key.dev->umetric_max),
                                                (((lndev->mr[SQR_RQ].umetric_final) *100)/lndev->key.dev->umetric_max),
                                                lndev->mr[SQR_RQ].umetric_final ? (((lndev->mr[SQR_RTQ].umetric_final) *100) / lndev->mr[SQR_RQ].umetric_final) : 0,
                                                lndev->umetric_link >> 10,
                                                lndev->umetric_link >= bestM ? ">" : "<",
                                                bestM >> 10,
                                                lndev->key.dev->link_id,
                                                ln->link_id,
                                                nn ? nn->neighIID4me : 0,
                                                ln->rq_hello_sqn_max,
                                                (bmx_time - lndev->rtq_time_max) / 1000,
                                                ln->neigh ? (lndev == ln->neigh->best_rtq ? "BEST" : "second") : "???"
                                                );

                                }

                        }
                        dbg_printf(cn, "\n");

                } else if (!strcmp(opt->long_name, ARG_ORIGINATORS)) {

                        struct avl_node *it;
                        struct orig_node *on;
                        UMETRIC_T total_metric = 0;
                        uint32_t  total_lref = 0;
                        char *empty = "";

#define DBG_STATUS4_ORIG_HEAD "%-22s %-16s %3s %-16s %-10s %9s %5s %5s %5s %1s %5s %4s\n"
#define DBG_STATUS6_ORIG_HEAD "%-22s %-40s %3s %-40s %-10s %9s %5s %5s %5s %1s %5s %4s\n"
#define DBG_STATUS4_ORIG_INFO "%-22s %-16s %3d %-16s %-10s %9ju %5d %5d %5d %1d %5d %4d\n"
#define DBG_STATUS6_ORIG_INFO "%-22s %-40s %3d %-40s %-10s %9ju %5d %5d %5d %1d %5d %4d\n"
#define DBG_STATUS4_ORIG_TAIL "%-22s %-16d %3s %-16s %-10s %9ju %5s %5s %5s %1s %5s %4d\n"
#define DBG_STATUS6_ORIG_TAIL "%-22s %-40d %3s %-40s %-10s %9ju %5s %5s %5s %1s %5s %4d\n"



                        dbg_printf(cn, (af_cfg == AF_INET ? DBG_STATUS4_ORIG_HEAD : DBG_STATUS6_ORIG_HEAD),
                                "Orig ID.name", "primaryIP", "RTs", "currRT", "viaDev",
                                "Metric/k","myIID", "desc#", "ogm#", "d", "lUpd", "lRef");

                        for (it = NULL; (on = avl_iterate_item(&orig_tree, &it));) {

                                dbg_printf(cn, (af_cfg == AF_INET ? DBG_STATUS4_ORIG_INFO : DBG_STATUS6_ORIG_INFO),
                                        on->id.name,
                                        on->blocked ? "BLOCKED" : on->primary_ip_str,
                                        on->rt_tree.items,
                                        ipXAsStr(af_cfg, (on->curr_rn ? &on->curr_rn->key.link->link_ip : &ZERO_IP)),
                                        on->curr_rn && on->curr_rn->key.dev ? on->curr_rn->key.dev->name_phy_cfg.str : " ",
                                        on->curr_rn ? ((on->curr_rn->mr.umetric_final) >> 10) :
                                             (on == &self ? (umetric_max(DEF_FMETRIC_EXP_OFFSET)) >> 10 : 0),
                                        on->dhn->myIID4orig, on->descSqn,
                                        on->ogmSqn_toBeSend,
                                        (on->ogmSqn_maxRcvd - on->ogmSqn_toBeSend),
                                        (bmx_time - on->updated_timestamp) / 1000,
                                        (bmx_time - on->dhn->referred_by_me_timestamp) / 1000
                                        );

                                if (on != &self) {
                                        total_metric += (on->curr_rn ? ((on->curr_rn->mr.umetric_final) >> 10) : 0);
                                        total_lref += (bmx_time - on->dhn->referred_by_me_timestamp) / 1000;
                                }

                        }

                        dbg_printf(cn, (af_cfg == AF_INET ? DBG_STATUS4_ORIG_TAIL : DBG_STATUS6_ORIG_TAIL),
                                "Averages:", orig_tree.items, empty, empty, empty,
                                (orig_tree.items > 1 ? total_metric / (orig_tree.items - 1) : 0),
                                empty, empty, empty, empty, empty,
                                orig_tree.items > 1 ? ((total_lref + ((orig_tree.items - 1) / 2)) / (orig_tree.items - 1)) : 0);


		} else {
			return FAILURE;
		}

	}

	return SUCCESS;
}


STATIC_FUNC
int32_t opt_purge(uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn)
{
        TRACE_FUNCTION_CALL;

	if ( cmd == OPT_APPLY)
                purge_link_and_orig_nodes(NULL, NO);

	return SUCCESS;
}


STATIC_FUNC
int32_t opt_update_description(uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn)
{
        TRACE_FUNCTION_CALL;

	if ( cmd == OPT_APPLY )
		my_description_changed = YES;


        if (cmd == OPT_POST && my_description_changed)
                update_my_description_adv();


	return SUCCESS;
}



static struct opt_type bmx_options[]=
{
//        ord parent long_name          shrt Attributes				*ival		min		max		default		*func,*syntax,*help

	{ODI,0,0,			0,  5,0,0,0,0,0,			0,		0,		0,		0,		0,
			0,		"\nProtocol options:"},

	{ODI,0,ARG_STATUS,		0,  5,A_PS0,A_USR,A_DYN,A_ARG,A_ANY,	0,		0, 		0,		0, 		opt_status,
			0,		"show status\n"},

/*
	{ODI,0,ARG_ROUTES,		0,  5,A_PS0,A_USR,A_DYN,A_ARG,A_ANY,	0,		0, 		0,		0, 		opt_status,
			0,		"show routes\n"},
*/

	{ODI,0,ARG_LINKS,		0,  5,A_PS0,A_USR,A_DYN,A_ARG,A_ANY,	0,		0, 		0,		0, 		opt_status,
			0,		"show links\n"},

	{ODI,0,ARG_ORIGINATORS,	        0,  5,A_PS0,A_USR,A_DYN,A_ARG,A_ANY,	0,		0, 		0,		0, 		opt_status,
			0,		"show originators\n"},

#ifndef LESS_OPTIONS
	


	{ODI,0,"asocial_device",	0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&Asocial_device,MIN_ASOCIAL,	MAX_ASOCIAL,	DEF_ASOCIAL,	0,
			ARG_VALUE_FORM,	"disable/enable asocial mode for devices unwilling to forward other nodes' traffic"},

#endif
	{ODI,0,ARG_TTL,			't',5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&my_ttl,	MIN_TTL,	MAX_TTL,	DEF_TTL,	opt_update_description,
			ARG_VALUE_FORM,	"set time-to-live (TTL) for OGMs"}
        ,
        {ODI,0,ARG_TX_INTERVAL,         0,  5, A_PS1, A_ADM, A_DYI, A_CFA, A_ANY, &my_tx_interval, MIN_TX_INTERVAL, MAX_TX_INTERVAL, DEF_TX_INTERVAL, opt_update_description,
			ARG_VALUE_FORM,	"set aggregation interval (SHOULD be smaller than the half of your and others OGM interval)"}
        ,
        {ODI,0,ARG_OGM_INTERVAL,        'o',5, A_PS1, A_ADM, A_DYI, A_CFA, A_ANY, &my_ogm_interval,  MIN_OGM_INTERVAL,   MAX_OGM_INTERVAL,   DEF_OGM_INTERVAL,   0,
			ARG_VALUE_FORM,	"set interval in ms with which new originator message (OGM) are send"}
        ,
	{ODI,0,ARG_OGM_PURGE_TO,    	0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&ogm_purge_to,	MIN_OGM_PURGE_TO,	MAX_OGM_PURGE_TO,	DEF_OGM_PURGE_TO,	0,
			ARG_VALUE_FORM,	"timeout in ms for purging stale originators"}
        ,
	{ODI,0,ARG_LINK_PURGE_TO,    	0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&link_purge_to,	MIN_LINK_PURGE_TO,MAX_LINK_PURGE_TO,DEF_LINK_PURGE_TO,0,
			ARG_VALUE_FORM,	"timeout in ms for purging stale originators"}
        ,
	{ODI,0,ARG_DAD_TO,        	0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&dad_to,	MIN_DAD_TO,	MAX_DAD_TO,	DEF_DAD_TO,	0,
			ARG_VALUE_FORM,	"duplicate address (DAD) detection timout in ms"}
        ,
	{ODI,0,"flush_all",		0,  5,A_PS0,A_ADM,A_DYN,A_ARG,A_ANY,	0,		0, 		0,		0, 		opt_purge,
			0,		"purge all neighbors and routes on the fly"},

};

IDM_T validate_name( char* name ) {

        int i,len;
        if ( (len = strlen( name )) >= DESCRIPTION0_ID_NAME_LEN )
                return FAILURE;

        for (i = 0; i < len; i++) {

                char c = name[i];

                if (c == '"' || c < ' ' || c > '~')
                        return FAILURE;

        }

        return SUCCESS;
}

void init_orig_node(struct orig_node *on, struct description_id *id)
{
        TRACE_FUNCTION_CALL;
        memset(on, 0, sizeof ( struct orig_node));
        memcpy(&on->id, id, sizeof ( struct description_id));

        AVL_INIT_TREE(on->rt_tree, struct router_node, key);

        avl_insert(&orig_tree, on, -300148);

}


STATIC_FUNC
void init_bmx(void)
{

        static uint8_t my_desc0[MAX_PKT_MSG_SIZE];
        static struct description_id id;
        memset(&id, 0, sizeof (id));

        if (gethostname(id.name, DESCRIPTION0_ID_NAME_LEN))
                cleanup_all(-500240);

        id.name[DESCRIPTION0_ID_NAME_LEN - 1] = 0;

        if (validate_name(id.name) == FAILURE) {
                dbg(DBGL_SYS, DBGT_ERR, "illegal hostname %s", id.name);
                cleanup_all(-500272);
        }

        RNG_GenerateBlock(&rng, id.rand.u8, DESCRIPTION0_ID_RANDOM_LEN);

        init_orig_node(&self, &id);

        self.desc = (struct description *) my_desc0;


        self.ogmSqn_rangeMin = ((OGM_SQN_MASK) & rand_num(OGM_SQN_MAX));

        self.descSqn = ((DESC_SQN_MASK) & rand_num(DESC_SQN_MAX));

        register_options_array(bmx_options, sizeof ( bmx_options));
}




STATIC_FUNC
void bmx(void)
{

	struct list_node *list_pos;
	struct dev_node *dev;
	TIME_T regular_timeout, statistic_timeout;

	TIME_T s_last_cpu_time = 0, s_curr_cpu_time = 0;

	regular_timeout = statistic_timeout = bmx_time;

	initializing = NO;

        while (!terminating) {

		TIME_T wait = whats_next( );

		if ( wait )
			wait4Event( MIN( wait, MAX_SELECT_TIMEOUT_MS ) );

		// The regular tasks...
		if ( U32_LT( regular_timeout + 1000,  bmx_time ) ) {

			// check for changed interface konfigurations...
                        struct avl_node *an = NULL;
                        while ((dev = avl_iterate_item(&dev_name_tree, &an))) {

				if ( dev->active ) {
				
                                        sysctl_config( dev );

                                        //purge_tx_timestamp_tree(dev, NO);
                                }

                        }

                        purge_link_and_orig_nodes(NULL, YES);

                        purge_dhash_invalid_list(NO);

			close_ctrl_node( CTRL_CLEANUP, 0 );

			list_for_each( list_pos, &dbgl_clients[DBGL_ALL] ) {

				struct ctrl_node *cn = (list_entry( list_pos, struct dbgl_node, list ))->cn;

				dbg_printf( cn, "------------------ DEBUG ------------------ \n" );

				check_apply_parent_option( ADD, OPT_APPLY, 0, get_option( 0, 0, ARG_STATUS ), 0, cn );
				check_apply_parent_option( ADD, OPT_APPLY, 0, get_option( 0, 0, ARG_LINKS ), 0, cn );
                                check_apply_parent_option( ADD, OPT_APPLY, 0, get_option( 0, 0, ARG_ORIGINATORS ), 0, cn );
				dbg_printf( cn, "--------------- END DEBUG ---------------\n" );

			}

			/* preparing the next debug_timeout */
			regular_timeout = bmx_time;
		}


		if ( U32_LT( statistic_timeout + 5000, bmx_time ) ) {

                        struct orig_node *on;
                        struct description_id id;
                        memset(&id, 0, sizeof (struct description_id));

                        while ((on = avl_next_item(&blocked_tree, &id))) {

                                memcpy( &id, &on->id, sizeof(struct description_id));

                                dbgf_all( DBGT_INFO, "trying to unblock %s...", on->desc->id.name);

                                IDM_T tlvs_res;
                                if ((tlvs_res = process_description_tlvs(NULL, on, on->desc, TLV_OP_TEST, FRAME_TYPE_PROCESS_ALL, NULL)) == TLV_RX_DATA_DONE) {
                                        tlvs_res = process_description_tlvs(NULL, on, on->desc, TLV_OP_ADD, FRAME_TYPE_PROCESS_ALL, NULL);
                                        assertion(-500364, (tlvs_res == TLV_RX_DATA_DONE)); // checked, so MUST SUCCEED!!
                                }

                                dbgf(DBGL_CHANGES, DBGT_INFO, "unblocking %s %s !",
                                        on->desc->id.name, tlvs_res == TLV_RX_DATA_DONE ? "success" : "failed");

                        }

			// check for corrupted memory..
			checkIntegrity();


			/* generating cpu load statistics... */
			s_curr_cpu_time = (TIME_T)clock();

			s_curr_avg_cpu_load = ( (s_curr_cpu_time - s_last_cpu_time) / (TIME_T)(bmx_time - statistic_timeout) );

			s_last_cpu_time = s_curr_cpu_time;

			statistic_timeout = bmx_time;
		}
	}
}



int main(int argc, char *argv[])
{
        // make sure we are using compatible description0 sizes:
        assertion(-500201, (MSG_DESCRIPTION0_ADV_SIZE == sizeof ( struct msg_description_adv)));


	gettimeofday( &start_time_tv, NULL );
	gettimeofday( &new_tv, NULL );

	upd_time( NULL );

	My_pid = getpid();


        if ( InitRng(&rng) != 0 ) {
                cleanup_all( -500525 );
        }

        unsigned int random;

        RNG_GenerateBlock(&rng, (byte*)&random, sizeof (random));

	srand( random );


	signal( SIGINT, handler );
	signal( SIGTERM, handler );
	signal( SIGPIPE, SIG_IGN );
	signal( SIGSEGV, segmentation_fault );

        init_tools();

	init_control();

        init_ip();

	init_bmx();

	init_schedule();

        init_avl();


        if (init_plugin() == SUCCESS) {

                activate_plugin((msg_get_plugin()), NULL, NULL);

                activate_plugin((metrics_get_plugin()), NULL, NULL);

                struct plugin * hna_get_plugin(void);
                activate_plugin((hna_get_plugin()), NULL, NULL);

#ifndef NO_TRAFFICDUMP
                struct plugin * dump_get_plugin(void);
                activate_plugin((dump_get_plugin()), NULL, NULL);
#endif


#ifdef BMX6_TODO

#ifndef	NO_VIS
                activate_plugin((vis_get_plugin_v1()), NULL, NULL);
#endif

#ifndef	NO_TUNNEL
                activate_plugin((tun_get_plugin_v1()), NULL, NULL);
#endif

#ifndef	NO_SRV
                activate_plugin((srv_get_plugin_v1()), NULL, NULL);
#endif

#endif

        } else {
                assertion(-500809, (0));
        }


	apply_init_args( argc, argv );

        bmx();

	cleanup_all( CLEANUP_SUCCESS );

	return -1;
}


