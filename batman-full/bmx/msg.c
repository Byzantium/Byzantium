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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <unistd.h>
#include <errno.h>

#include "bmx.h"
#include "msg.h"
#include "ip.h"
#include "metrics.h"
#include "schedule.h"
#include "tools.h"
#include "plugin.h"

union schedule_hello_info {
#define SCHEDULE_HELLO_SQN 0
#define SCHEDULE_DEV_ID 1
        uint8_t u8[2];
        uint16_t u16;
};

static Sha bmx_sha;

AVL_TREE( description_cache_tree, struct description_cache_node, dhash );



static int32_t pref_udpd_size = DEF_UDPD_SIZE;


static int32_t ogm_tx_iters = DEF_OGM_TX_ITERS;
static int32_t my_ogm_ack_tx_iters = DEF_OGM_ACK_TX_ITERS;

int32_t desc_req_tx_iters = DEF_DSC0_REQS_TX_ITERS;
int32_t dhash_req_tx_iters = DEF_DHS0_REQS_TX_ITERS;

int32_t desc_tx_iters = DEF_DSC0_ADVS_TX_ITERS;
int32_t dhash_tx_iters = DEF_DHS0_ADVS_TX_ITERS;
int32_t tx_unsolicited_desc = DEF_UNSOLICITED_DESC_ADVS;

int my_desc0_tlv_len = 0;

IID_T myIID4me = IID_RSVD_UNUSED;
TIME_T myIID4me_timestamp = 0;


LIST_SIMPEL( ogm_aggreg_list, struct ogm_aggreg_node, list, sqn );
uint32_t ogm_aggreg_pending = 0;
static AGGREG_SQN_T ogm_aggreg_sqn_max;


char *tlv_op_str[] = {"TLV_DEL","TLV_TEST","TLV_ADD","TLV_DONE","TLV_DEBUG"};
static struct dhash_node* DHASH_NODE_FAILURE = (struct dhash_node*) & DHASH_NODE_FAILURE;


/***********************************************************
  The core frame/message structures and handlers
 ************************************************************/


struct frame_handl packet_frame_handler[FRAME_TYPE_ARRSZ];

struct frame_handl description_tlv_handl[BMX_DSC_TLV_ARRSZ];

void register_frame_handler(struct frame_handl *array, int pos, struct frame_handl *handl)
{
        assertion(-500659, (pos < BMX_DSC_TLV_ARRSZ));
        assertion(-500660, (!array[pos].min_msg_size)); // the pos MUST NOT be used yet
        assertion(-500661, (handl && handl->min_msg_size && handl->name));
        assertion(-500806, (XOR(handl->rx_frame_handler, handl->rx_msg_handler) && XOR(handl->tx_frame_handler, handl->tx_msg_handler)));
        assertion(-500879, (!(handl->min_msg_size % TLV_DATA_STEPS)));
        assertion(-500880, (!(handl->data_header_size % TLV_DATA_STEPS)));

        array[pos] = *handl;
}


STATIC_FUNC
struct description * remove_cached_description(struct description_hash *dhash)
{
        TRACE_FUNCTION_CALL;
        struct description_cache_node *dcn;

        if (!(dcn = avl_find_item(&description_cache_tree, dhash)))
                return NULL;

        struct description *desc0 = dcn->description;

        avl_remove(&description_cache_tree, &dcn->dhash, -300206);
        debugFree(dcn, -300108);

        return desc0;
}
STATIC_FUNC
struct description_cache_node *purge_cached_descriptions(IDM_T purge_all)
{
        TRACE_FUNCTION_CALL;
        struct description_cache_node *dcn;
        struct description_cache_node *dcn_min = NULL;
        struct description_hash tmp_dhash;
        memset( &tmp_dhash, 0, sizeof(struct description_hash));

        dbgf_all( DBGT_INFO, "%s", purge_all ? "purge_all" : "only_expired");

        while ((dcn = avl_next_item(&description_cache_tree, &tmp_dhash))) {

                memcpy(&tmp_dhash, &dcn->dhash, HASH0_SHA1_LEN);

                if (purge_all || ((TIME_T) (bmx_time - dcn->timestamp)) > DEF_DESC0_CACHE_TO) {

                        avl_remove(&description_cache_tree, &dcn->dhash, -300208);
                        debugFree(dcn->description, -300100);
                        debugFree(dcn, -300101);

                } else {

                        if (!dcn_min || U32_LT(dcn->timestamp, dcn_min->timestamp))
                                dcn_min = dcn;
                }
        }

        return dcn_min;
}

STATIC_FUNC
void cache_description(struct description *desc, struct description_hash *dhash)
{
        TRACE_FUNCTION_CALL;
        struct description_cache_node *dcn;

        uint16_t desc_len = sizeof (struct description) + ntohs(desc->dsc_tlvs_len);

        if ((dcn = avl_find_item(&description_cache_tree, dhash))) {
                dcn->timestamp = bmx_time;
                return;
        }

        dbgf_all( DBGT_INFO, "%8X..", dhash->h.u32[0]);


        paranoia(-500261, (description_cache_tree.items > DEF_DESC0_CACHE_SIZE));

        if ( description_cache_tree.items == DEF_DESC0_CACHE_SIZE ) {


                struct description_cache_node *dcn_min = purge_cached_descriptions( NO );

                dbgf(DBGL_SYS, DBGT_WARN, "desc0_cache_tree reached %d items! cleaned up %d items!",
                        DEF_DESC0_CACHE_SIZE, DEF_DESC0_CACHE_SIZE - description_cache_tree.items);

                if (description_cache_tree.items == DEF_DESC0_CACHE_SIZE) {
                        avl_remove(&description_cache_tree, &dcn_min->dhash, -300209);
                        debugFree(dcn_min->description, -300102);
                        debugFree(dcn_min, -300103);
                }
        }

        paranoia(-500273, (desc_len != sizeof ( struct description) + ntohs(desc->dsc_tlvs_len)));

        dcn = debugMalloc(sizeof ( struct description_cache_node), -300104);
        dcn->description = debugMalloc(desc_len, -300105);
        memcpy(dcn->description, desc, desc_len);
        memcpy( &dcn->dhash, dhash, HASH0_SHA1_LEN );
        dcn->timestamp = bmx_time;
        avl_insert(&description_cache_tree, dcn, -300145);

}



IDM_T process_description_tlvs(struct packet_buff *pb, struct orig_node *on, struct description *desc, IDM_T op, uint8_t filter, struct ctrl_node *cn)
{
        TRACE_FUNCTION_CALL;
        assertion(-500370, (op == TLV_OP_DEL || op == TLV_OP_TEST || op == TLV_OP_ADD || op == TLV_OP_DEBUG));
        assertion(-500590, (IMPLIES(on == &self, op == TLV_OP_DEBUG)));
        assertion(-500807, (desc));
        assertion(-500829, (IMPLIES(op == TLV_OP_DEL, !on->blocked)));

        uint16_t dsc_tlvs_len = ntohs(desc->dsc_tlvs_len);

        IDM_T tlv_result;
        struct rx_frame_iterator it = {
                .caller = __FUNCTION__, .on = on, .cn = cn, .op = op, .pb = pb,
                .handls = description_tlv_handl, .handl_max = (BMX_DSC_TLV_MAX), .process_filter = filter,
                .frames_in = (((uint8_t*) desc) + sizeof (struct description)), .frames_pos = 0,
                .frames_length = dsc_tlvs_len
        };

        dbgf_all(DBGT_INFO, "%s %s dsc_sqn %d size %d ",
                tlv_op_str[op], desc->id.name, ntohs(desc->dsc_sqn), dsc_tlvs_len);


        while ((tlv_result = rx_frame_iterate(&it)) > TLV_RX_DATA_DONE);

        if (tlv_result == TLV_DATA_BLOCKED) {

                assertion(-500356, (op == TLV_OP_TEST));

                dbgf(DBGL_SYS, DBGT_ERR, "%s data_length=%d  BLOCKED",
                        description_tlv_handl[it.frame_type].name, it.frame_data_length);

                on->blocked = YES;

                if (!avl_find(&blocked_tree, &on->id))
                        avl_insert(&blocked_tree, on, -300165);

                return TLV_DATA_BLOCKED;


        } else if (tlv_result == TLV_DATA_FAILURE) {

                dbgf(DBGL_SYS, DBGT_WARN,
                        "rcvd problematic description_ltv from %s near: type=%s  data_length=%d  pos=%d ",
                        pb ? pb->i.llip_str : "---",
                        description_tlv_handl[it.frame_type].name, it.frame_data_length, it.frames_pos);

                return TLV_DATA_FAILURE;
        }

        if ( op == TLV_OP_ADD ) {
                on->blocked = NO;
                avl_remove(&blocked_tree, &on->id, -300211);
        }

        return TLV_RX_DATA_DONE;
}



void purge_tx_task_list(struct list_head *tx_task_lists, struct link_node *only_link, struct dev_node *only_dev)
{
        TRACE_FUNCTION_CALL;
        int i;
        assertion(-500845, (tx_task_lists));

        for (i = 0; i < FRAME_TYPE_ARRSZ; i++) {

                struct list_node *lpos, *tpos, *lprev = (struct list_node*) & tx_task_lists[i];

                list_for_each_safe(lpos, tpos, &tx_task_lists[i])
                {
                        struct tx_task_node * tx_task = list_entry(lpos, struct tx_task_node, list);

                        if ((!only_link || only_link == tx_task->content.link) &&
                                (!only_dev || only_dev == tx_task->content.dev)) {


                                if (packet_frame_handler[tx_task->content.type].min_tx_interval) {
                                        avl_remove(&tx_task->content.dev->tx_task_tree, &tx_task->content, -300313);
                                }

                                list_del_next(&tx_task_lists[i], lprev);

                                dbgf_all(DBGT_INFO, "removed frame_type=%d ln=%s dev=%s tx_tasks_list.items=%d",
                                        tx_task->content.type,
                                        ipXAsStr(af_cfg, tx_task->content.link ? &tx_task->content.link->link_ip : &ZERO_IP),
                                        tx_task->content.dev->label_cfg.str, tx_task_lists[tx_task->content.type].items);

                                debugFree(tx_task, -300066);

                                continue;
                        }

                        lprev = lpos;
                }
        }
}


STATIC_FUNC
IDM_T freed_tx_task_node(struct tx_task_node *tx_task, struct list_head *tx_task_list, struct list_node *lprev)
{
        TRACE_FUNCTION_CALL;
        assertion(-500372, (tx_task && tx_task->content.dev));
        assertion(-500539, lprev);

        if (tx_task->tx_iterations <= 0) {

                if (packet_frame_handler[tx_task->content.type].min_tx_interval) {
                        avl_remove(&tx_task->content.dev->tx_task_tree, &tx_task->content, -300314);
                }

                list_del_next(tx_task_list, lprev);

                debugFree(tx_task, -300169);

                return YES;
        }

        return NO;
}


STATIC_FUNC
IDM_T tx_task_obsolete(struct tx_task_node *tx_task)
{
        TRACE_FUNCTION_CALL;
        struct dhash_node *dhn = NULL;

        if (tx_task->content.myIID4x > IID_RSVD_MAX &&
                (!(dhn = iid_get_node_by_myIID4x(tx_task->content.myIID4x)) || !dhn->on)) {
                goto return_obsolete;
        }

        if (!packet_frame_handler[tx_task->content.type].min_tx_interval)
                return NO;

        if (((TIME_T) (bmx_time - tx_task->send_ts) < packet_frame_handler[tx_task->content.type].min_tx_interval)) {

                goto return_obsolete;
        }

        tx_task->send_ts = bmx_time;

        return NO;


return_obsolete:
        dbgf(DBGL_CHANGES, DBGT_INFO,
                "%s type=%s dev=%s myIId4x=%d neighIID4x=%d link_id=0x%X name=%s or send just %d ms ago",
                (dhn && dhn->on) ? "OMITTING" : (!dhn ? "UNKNOWN" : "INVALIDATED"),
                packet_frame_handler[tx_task->content.type].name, tx_task->content.dev->name_phy_cfg.str,
                tx_task->content.myIID4x, tx_task->content.neighIID4x, tx_task->content.link ? tx_task->content.link->link_id : 0,
                (dhn && dhn->on) ? dhn->on->id.name : "???", (bmx_time - tx_task->send_ts));

        return YES;
}



STATIC_FUNC
struct tx_task_node *tx_task_new(struct link_dev_node *dest_lndev, struct tx_task_node *test)
{
        assertion(-500909, (dest_lndev));
        struct frame_handl *handl = &packet_frame_handler[test->content.type];
        struct tx_task_node * tx_task = NULL;
        struct dhash_node *dhn;

        if (test->content.myIID4x > IID_RSVD_MAX && (!(dhn = iid_get_node_by_myIID4x(test->content.myIID4x)) || !dhn->on)) {
                return NULL;
        }

        if (handl->min_tx_interval) {

                if ((tx_task = avl_find_item(&test->content.dev->tx_task_tree, &test->content))) {

                        ASSERTION(-500905, (tx_task->frame_msgs_length == test->frame_msgs_length));
                        ASSERTION(-500906, (IMPLIES((!handl->is_advertisement), (tx_task->content.link == test->content.link))));
                        //ASSERTION(-500907, (tx_task->u16 == test->u16));

                        tx_task->tx_iterations = MAX(tx_task->tx_iterations, test->tx_iterations);

                        // then it is already scheduled
                        return tx_task;
                }

                tx_task = debugMalloc(sizeof ( struct tx_task_node), -300026);
                memcpy(tx_task, test, sizeof ( struct tx_task_node));

                tx_task->send_ts = ((TIME_T) (bmx_time - handl->min_tx_interval));

                avl_insert(&test->content.dev->tx_task_tree, tx_task, -300315);

                if (test->content.dev->tx_task_tree.items > DEF_TX_TS_TREE_SIZE) {
                        dbg_mute(20, DBGL_SYS, DBGT_WARN,
                                "%s tx_ts_tree reached %d %s neighIID4x=%d u16=%d u32=%d myIID4x=%d",
                                test->content.dev->name_phy_cfg.str, test->content.dev->tx_task_tree.items,
                                handl->name, test->content.neighIID4x, test->content.u16, test->content.u32, test->content.myIID4x);
                }


        } else {

                tx_task = debugMalloc(sizeof ( struct tx_task_node), -300026);
                memcpy(tx_task, test, sizeof ( struct tx_task_node));
        }


        if (handl->is_destination_specific_frame) {

                // ensure, this is NOT a dummy dest_lndev!!!:
                ASSERTION(-500850, (dest_lndev && dest_lndev == avl_find_item(&link_dev_tree, &dest_lndev->key)));

                list_add_tail(&(dest_lndev->tx_task_lists[test->content.type]), &tx_task->list);

                dbgf(DBGL_CHANGES, DBGT_INFO, "added %s to lndev link_id=%X link_ip=%s dev=%s tx_tasks_list.items=%d",
                        handl->name, dest_lndev->key.link->link_id,
                        ipXAsStr(af_cfg, &dest_lndev->key.link->link_ip),
                        dest_lndev->key.dev->label_cfg.str, dest_lndev->tx_task_lists[test->content.type].items);

        } else {

                list_add_tail(&(test->content.dev->tx_task_lists[test->content.type]), &tx_task->list);
        }

        return tx_task;
}

void schedule_tx_task(struct link_dev_node *dest_lndev,
        uint16_t frame_type, uint16_t frame_msgs_len, uint16_t u16, uint32_t u32, IID_T myIID4x, IID_T neighIID4x)
{
        TRACE_FUNCTION_CALL;

        if (!dest_lndev)
                return;

        assertion(-500756, (dest_lndev && dest_lndev->key.dev));
        ASSERTION(-500713, (iid_get_node_by_myIID4x(myIID4me)));
        ASSERTION(-500714, (!myIID4x || iid_get_node_by_myIID4x(myIID4x)));

        struct frame_handl *handl = &packet_frame_handler[frame_type];
        struct dev_node *dev_out = dest_lndev->key.dev;

        if (handl->tx_iterations && !(*handl->tx_iterations))
                return;

        self.dhn->referred_by_me_timestamp = bmx_time;

        if (myIID4x)
                iid_get_node_by_myIID4x(myIID4x)->referred_by_me_timestamp = bmx_time;

        dbgf_all(DBGT_INFO, "%s %s  sqn=%d myIID4x=%d neighIID4x=%d",
                handl->name, dev_out->label_cfg.str, u16, myIID4x, neighIID4x);

        if (handl->umetric_rq_min && *(handl->umetric_rq_min) > dest_lndev->mr[SQR_RQ].umetric_final) {

                dbgf(DBGL_SYS, DBGT_INFO, "NOT sending %s (via %s sqn %d myIID4x %d neighIID4x %d) %ju < %ju",
                        handl->name, dev_out->label_cfg.str, u16, myIID4x, neighIID4x,
                        dest_lndev->mr[SQR_RQ].umetric_final, *(handl->umetric_rq_min));
                return;
        }

        struct tx_task_node test_task;
        memset(&test_task, 0, sizeof (test_task));
        test_task.content.u16 = u16;
        test_task.content.u32 = u32;
        test_task.content.dev = dev_out;
        test_task.content.myIID4x = myIID4x;
        test_task.content.neighIID4x = neighIID4x;
        test_task.content.type = frame_type;
        test_task.tx_iterations = handl->tx_iterations ? *handl->tx_iterations : 1;
        test_task.considered_ts = bmx_time - 1;

        // advertisements are send to all and are not bound to a specific destinations,
        // therfore tx_task_obsolete should not filter due to the destination_link_id
        if (!handl->is_advertisement && dest_lndev->key.link) {
                ASSERTION(-500915, (dest_lndev == avl_find_item(&link_dev_tree, &dest_lndev->key)));
                test_task.content.link = dest_lndev->key.link;
                test_task.content.lndev = dest_lndev;
        }

        if (frame_msgs_len) {
                assertion(-500371, IMPLIES(handl->fixed_msg_size, !(frame_msgs_len % handl->min_msg_size)));
                test_task.frame_msgs_length = frame_msgs_len;
        } else {
                assertion(-500769, (handl->fixed_msg_size && handl->min_msg_size));
                test_task.frame_msgs_length = handl->min_msg_size;
        }

        tx_task_new(dest_lndev, &test_task);
}





OGM_SQN_T set_ogmSqn_toBeSend_and_aggregated(struct orig_node *on, OGM_SQN_T to_be_send, OGM_SQN_T aggregated)
{
        TRACE_FUNCTION_CALL;

        to_be_send &= OGM_SQN_MASK;
        aggregated &= OGM_SQN_MASK;

        if (UXX_GT(OGM_SQN_MASK, to_be_send, aggregated)) {

                if (UXX_LE(OGM_SQN_MASK, on->ogmSqn_toBeSend, on->ogmSqn_aggregated))
                        ogm_aggreg_pending++;

        } else {

                assertion(-500830, (UXX_LE(OGM_SQN_MASK, to_be_send, aggregated)));

                if (UXX_GT(OGM_SQN_MASK, on->ogmSqn_toBeSend, on->ogmSqn_aggregated))
                        ogm_aggreg_pending--;
        }

        on->ogmSqn_toBeSend = to_be_send;
        on->ogmSqn_aggregated = aggregated;

        return on->ogmSqn_toBeSend;
}


STATIC_FUNC
IID_T create_ogm(struct orig_node *on, IID_T prev_ogm_iid, struct msg_ogm_adv *ogm)
{
        TRACE_FUNCTION_CALL;
        uint8_t exp_offset = on->path_metricalgo->exp_offset;
        FMETRIC_T fm = umetric_to_fmetric(exp_offset,
                (on == &self ? umetric_max(DEF_FMETRIC_EXP_OFFSET) : on->curr_rn->mr.umetric_final));

        if (UXX_GT(OGM_SQN_MASK, on->ogmSqn_toBeSend, on->ogmSqn_aggregated + OGM_SQN_STEP)) {

                dbgf(DBGL_CHANGES, DBGT_WARN, "%s delayed %d < %d",
                        on->id.name, on->ogmSqn_aggregated, on->ogmSqn_toBeSend);
        } else {

                dbgf_all(DBGT_INFO, "%s in-time %d < %d", on->id.name, on->ogmSqn_aggregated, on->ogmSqn_toBeSend);
        }

        set_ogmSqn_toBeSend_and_aggregated(on, on->ogmSqn_toBeSend, on->ogmSqn_toBeSend);

        on->dhn->referred_by_me_timestamp = bmx_time;

        assertion( -500890, ((on->dhn->myIID4orig - prev_ogm_iid) <= OGM_IIDOFFST_MASK));
        assertion( -500891, ((fm.val.f.exp_total - exp_offset) <= OGM_EXPONENT_MASK));
        assertion( -500892, ((fm.val.f.mantissa) <= OGM_MANTISSA_MASK));

        OGM_MIX_T mix =
                ((on->dhn->myIID4orig - prev_ogm_iid) << OGM_IIDOFFST_BIT_POS) +
                ((fm.val.f.exp_total - exp_offset) << OGM_EXPONENT_BIT_POS) +
                ((fm.val.f.mantissa) << OGM_MANTISSA_BIT_POS);

        ogm->mix = htons(mix);
        ogm->u.ogm_sqn = htons(on->ogmSqn_toBeSend);

        return on->dhn->myIID4orig;
}

STATIC_FUNC
void create_ogm_aggregation(void)
{
        TRACE_FUNCTION_CALL;
        uint32_t target_ogms = MIN(OGMS_PER_AGGREG_MAX,
                ((ogm_aggreg_pending < ((OGMS_PER_AGGREG_PREF / 3)*4)) ? ogm_aggreg_pending : OGMS_PER_AGGREG_PREF));

        struct msg_ogm_adv* msgs =
                debugMalloc((target_ogms + OGM_JUMPS_PER_AGGREGATION) * sizeof (struct msg_ogm_adv), -300177);

        IID_T curr_iid;
        IID_T ogm_iid = 0;
        IID_T ogm_iid_jumps = 0;
        uint16_t ogm_msg = 0;

        dbgf_all(DBGT_INFO, "pending %d target %d", ogm_aggreg_pending, target_ogms);

        for (curr_iid = IID_MIN_USED; curr_iid < my_iid_repos.max_free; curr_iid++) {

                IID_NODE_T *dhn = my_iid_repos.arr.node[curr_iid];
                struct orig_node *on = dhn ? dhn->on : NULL;

                if (on && UXX_GT(OGM_SQN_MASK, on->ogmSqn_toBeSend, on->ogmSqn_aggregated)) {

                        if (on != &self && (!on->curr_rn || on->curr_rn->mr.umetric_final < on->path_metricalgo->umetric_min)) {

                                dbgf(DBGL_SYS, DBGT_WARN,
                                        "%s with %s curr_rn and PENDING ogm_sqn=%d but path_metric=%jd < USABLE=%jd",
                                        on->id.name, on->curr_rn ? " " : "NO", on->ogmSqn_toBeSend,
                                        on->curr_rn ? on->curr_rn->mr.umetric_final : 0,
                                        on->path_metricalgo->umetric_min);

                                assertion(-500816, (0));

                                continue;
                        }

                        if (((IID_T) (dhn->myIID4orig - ogm_iid) >= OGM_IID_RSVD_JUMP)) {

                                if (ogm_iid_jumps == OGM_JUMPS_PER_AGGREGATION)
                                        break;

                                dbgf((ogm_iid_jumps > 1) ? DBGL_SYS : DBGL_ALL, DBGT_INFO,
                                        "IID jump %d from %d to %d", ogm_iid_jumps, ogm_iid, dhn->myIID4orig);

                                ogm_iid = dhn->myIID4orig;

                                msgs[ogm_msg + ogm_iid_jumps].mix = htons((OGM_IID_RSVD_JUMP) << OGM_IIDOFFST_BIT_POS);
                                msgs[ogm_msg + ogm_iid_jumps].u.transmitterIIDabsolute = htons(ogm_iid);

                                ogm_iid_jumps++;
                        }

                        ogm_iid = create_ogm(on, ogm_iid, &msgs[ogm_msg + ogm_iid_jumps]);

                        if ((++ogm_msg) == target_ogms)
                                break;
                }
        }

        assertion(-500817, (IMPLIES(curr_iid == my_iid_repos.max_free, !ogm_aggreg_pending)));

        if (ogm_aggreg_pending) {
                dbgf(DBGL_SYS, DBGT_WARN, "%d ogms left for immediate next aggregation", ogm_aggreg_pending);
        }

        if (!ogm_msg) {
                debugFree( msgs, -300219);
                return;
        }

        struct ogm_aggreg_node *oan = debugMalloc(sizeof (struct ogm_aggreg_node), -300179);
        oan->aggregated_msgs = ogm_msg + ogm_iid_jumps;
        oan->ogm_advs = msgs;
        oan->tx_attempt = 0;
        oan->sqn = ++ogm_aggreg_sqn_max;

        dbgf_all( DBGT_INFO, "aggregation_sqn %d aggregated_ogms %d", oan->sqn, ogm_msg);

        list_add_tail(&ogm_aggreg_list, &oan->list);

        struct avl_node *an = NULL;
        struct neigh_node *nn;

        while ((nn = avl_iterate_item(&neigh_tree, &an))) {

                bit_set(nn->ogm_aggregations_acked, AGGREG_SQN_CACHE_RANGE, ogm_aggreg_sqn_max, 0);
        }

        return;
}





STATIC_FUNC
struct link_dev_node **get_best_lndevs_by_criteria(struct ogm_aggreg_node *oan_criteria, struct dhash_node *dhn_criteria)
{
        TRACE_FUNCTION_CALL;

        static struct link_dev_node **lndev_arr = NULL;
        static uint16_t lndev_arr_items = 0;
        struct avl_node *an;
        struct neigh_node *nn;
        struct dev_node *dev;
        uint16_t d = 0;

        if (lndev_arr_items < dev_ip_tree.items + 1) {

                if (lndev_arr)
                        debugFree(lndev_arr, -300180);

                lndev_arr_items = dev_ip_tree.items + 1;
                lndev_arr = debugMalloc((lndev_arr_items * sizeof (struct link_dev_node*)), -300182);
        }

        if ( oan_criteria || dhn_criteria ) {

                if (oan_criteria) {
                        dbgf_all(DBGT_INFO, "aggreg_sqn %d ", oan_criteria->sqn);
                } else if (dhn_criteria) {
                        dbgf_all(DBGT_INFO, "NOT %s ", dhn_criteria->on->id.name);
                }

                for (an = NULL; (dev = avl_iterate_item(&dev_ip_tree, &an));)
                        dev->tmp = NO;


                for (an = NULL; (nn = avl_iterate_item(&neigh_tree, &an));) {

                        if (oan_criteria &&
                                (bit_get(nn->ogm_aggregations_acked, AGGREG_SQN_CACHE_RANGE, oan_criteria->sqn) ||
                                nn->best_rtq->mr[SQR_RQ].umetric_final <= UMETRIC_RQ_OGM_ACK_MIN))
                                continue;

                        if (dhn_criteria && dhn_criteria->neigh == nn)
                                continue;

                        assertion(-500445, (nn->best_rtq && nn->best_rtq->key.dev));

                        struct dev_node *dev_best = nn->best_rtq->key.dev;

                        assertion(-500446, (dev_best));
                        assertion(-500447, (dev_best->active));

                        dbgf_all( DBGT_INFO, "  via dev=%s to link_id=0x%X  (redundant %d)",
                                dev_best->label_cfg.str, nn->best_rtq->key.link->link_id, dev_best->tmp);

                        if (dev_best->tmp == NO) {

                                lndev_arr[d++] = nn->best_rtq;

                                dev_best->tmp = YES;
                        }

                        assertion(-500444, (d <= dev_ip_tree.items));

                }
        }

        lndev_arr[d] = NULL;

        return lndev_arr;
}

STATIC_FUNC
void schedule_or_purge_ogm_aggregations(IDM_T purge_all)
{
        TRACE_FUNCTION_CALL;

        static TIME_T timestamp = 0;

        dbgf_all(DBGT_INFO, "max %d   active aggregations %d   pending ogms %d  expiery in %d ms",
                ogm_aggreg_sqn_max, ogm_aggreg_list.items, ogm_aggreg_pending,
                (my_tx_interval - ((TIME_T) (bmx_time - timestamp))));

        if (!purge_all && timestamp != bmx_time) {

                timestamp = bmx_time;

                while (ogm_aggreg_pending) {

                        struct ogm_aggreg_node *oan = list_get_first(&ogm_aggreg_list);

                        if (oan && ((AGGREG_SQN_MASK)& ((ogm_aggreg_sqn_max + 1) - oan->sqn)) >= AGGREG_SQN_CACHE_RANGE) {

                                dbgf(DBGL_SYS, DBGT_WARN,
                                        "ogm_aggreg_list full min %d max %d items %d unaggregated %d",
                                        oan->sqn, ogm_aggreg_sqn_max, ogm_aggreg_list.items, ogm_aggreg_pending);

                                debugFree(oan->ogm_advs, -300185);
                                debugFree(oan, -300186);
                                list_del_next(&ogm_aggreg_list, ((struct list_node*) & ogm_aggreg_list));
                        }

                        create_ogm_aggregation();

                        if (!ogm_aggreg_pending) {


#ifdef EXTREME_PARANOIA
                                IID_T curr_iid;
                                for (curr_iid = IID_MIN_USED; curr_iid < my_iid_repos.max_free; curr_iid++) {

                                        IID_NODE_T *dhn = my_iid_repos.arr.node[curr_iid];
                                        struct orig_node *on = dhn ? dhn->on : NULL;

                                        if (on && on->curr_rn &&
                                                UXX_GT(OGM_SQN_MASK, on->ogmSqn_toBeSend, on->ogmSqn_aggregated) &&
                                                on->curr_rn->mr.umetric_final >= on->path_metricalgo->umetric_min) {

                                                dbgf(DBGL_SYS, DBGT_ERR,
                                                        "%s with %s curr_rn and PENDING ogm_sqn=%d but path_metric=%jd < USABLE=%jd",
                                                        on->id.name, on->curr_rn ? " " : "NO", on->ogmSqn_toBeSend,
                                                        on->curr_rn ? on->curr_rn->mr.umetric_final : 0, on->path_metricalgo->umetric_min);

                                                ASSERTION( -500473, (0));

                                        }
                                }
#endif
                        }
                }
        }

        struct list_node *lpos, *tpos, *lprev = (struct list_node*) & ogm_aggreg_list;

        list_for_each_safe(lpos, tpos, &ogm_aggreg_list)
        {
                struct ogm_aggreg_node *oan = list_entry(lpos, struct ogm_aggreg_node, list);

                if (purge_all || oan->tx_attempt >= ogm_tx_iters) {

                        list_del_next(&ogm_aggreg_list, lprev);
                        debugFree(oan->ogm_advs, -300183);
                        debugFree(oan, -300184);

                        continue;

                } else if (oan->tx_attempt < ogm_tx_iters) {

                        struct link_dev_node **lndev_arr = get_best_lndevs_by_criteria(oan, NULL);
                        int d;

                        if (!lndev_arr[0])
                                oan->tx_attempt = ogm_tx_iters;
                        else
                                oan->tx_attempt++;

                        for (d = 0; (lndev_arr[d]); d++) {

                                schedule_tx_task((lndev_arr[d]), FRAME_TYPE_OGM_ADVS,
                                        (oan->aggregated_msgs * sizeof (struct msg_ogm_adv)), oan->sqn, 0, 0, 0);

                                if (oan->tx_attempt >= ogm_tx_iters) {
                                        dbgf(DBGL_CHANGES, DBGT_INFO,
                                                "ogm_aggregation_sqn=%d msgs=%d tx_attempt=%d/%d via dev=%s last send!",
                                                oan->sqn, oan->aggregated_msgs, oan->tx_attempt, ogm_tx_iters,
                                                lndev_arr[d]->key.dev->label_cfg.str);
                                }
                        }


                }

                lprev = lpos;
        }
}


STATIC_FUNC
int32_t tx_msg_hello_reply(struct tx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        assertion(-500916, (it->ttn->content.lndev));
        assertion(-500585, (it->ttn->content.link && it->ttn->content.link->link_id >= LINK_ID_MIN));
        assertion(-500770, (tx_iterator_cache_data_space(it) >= ((int) sizeof (struct msg_hello_reply))));

        struct tx_task_node *ttn = it->ttn;
        struct msg_hello_reply *msg = (struct msg_hello_reply *) (tx_iterator_cache_msg_ptr(it));
        HELLO_FLAGS_SQN_T flags = ttn->content.lndev->tx_hello_reply_flags;

        msg->hello_flags_sqn = htons((ttn->content.u16 << HELLO_SQN_BIT_POS) + (flags << HELLO_FLAGS_BIT_POS));

        msg->transmitterIID4x = htons(ttn->content.myIID4x);

        dbgf_all(DBGT_INFO, "dev=%s to link_id=0x%X SQN=%d",
                ttn->content.dev->label_cfg.str, ttn->content.link->link_id, ttn->content.u16);

        return sizeof (struct msg_hello_reply);
}



STATIC_FUNC
int32_t tx_msg_hello_adv(struct tx_frame_iterator *it)
{

        TRACE_FUNCTION_CALL;
        assertion(-500771, (tx_iterator_cache_data_space(it) >= ((int) sizeof (struct msg_hello_adv))));
        assertion(-500906, (it->ttn->content.dev->link_hello_sqn <= HELLO_SQN_MASK));

        struct tx_task_node *ttn = it->ttn;
        struct msg_hello_adv *adv = (struct msg_hello_adv *) (tx_iterator_cache_msg_ptr(it));
        struct avl_node *an;
        struct link_dev_node *lndev;

        HELLO_FLAGS_SQN_T sqn_in = ttn->content.dev->link_hello_sqn = ((HELLO_SQN_MASK)&(ttn->content.dev->link_hello_sqn + 1));
        HELLO_FLAGS_SQN_T sqn_min = ((HELLO_SQN_MASK)&(sqn_in + 1 - DEF_HELLO_SQN_RANGE));
        HELLO_FLAGS_SQN_T flags = 0;
        HELLO_FLAGS_SQN_T flags_sqn = (sqn_in << HELLO_SQN_BIT_POS) + (flags << HELLO_FLAGS_BIT_POS);

        adv->hello_flags_sqn = htons(flags_sqn);
        
        for (an = NULL; (lndev = avl_iterate_item(&link_dev_tree, &an));) {

                if (lndev->key.dev != ttn->content.dev)
                        continue;

                //TODO: maybe this should go to update_link_metric()

                update_metric_record(NULL, &lndev->mr[SQR_RTQ], &link_metric_algo[SQR_RTQ], DEF_HELLO_SQN_RANGE, sqn_min, sqn_in, NULL);

                if (lndev->key.link->neigh && lndev->key.link->neigh->dhn && lndev->key.link->neigh->dhn->on && !lndev->key.link->neigh->dhn->on->blocked) {
                        
                        assertion(-500922, (lndev->key.link->neigh->dhn->on->path_metricalgo));

                        UMETRIC_T max = umetric_max(lndev->key.link->neigh->dhn->on->path_metricalgo->exp_offset);

                        apply_metric_algo(&lndev->umetric_link, lndev, &max, lndev->key.link->neigh->dhn->on->path_metricalgo);

                } else {
                        lndev->umetric_link = 0;
                }

        }

        dbgf_all(DBGT_INFO, "%s %s SQN %d", ttn->content.dev->label_cfg.str, ttn->content.dev->ip_llocal_str, sqn_in);

        return sizeof (struct msg_hello_adv);
}


STATIC_FUNC
int32_t tx_msg_dhash_or_description_request(struct tx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        struct tx_task_node *ttn = it->ttn;
        struct hdr_dhash_request *hdr = ((struct hdr_dhash_request*) tx_iterator_cache_hdr_ptr(it));
        struct msg_dhash_request *msg = ((struct msg_dhash_request*) tx_iterator_cache_msg_ptr(it));
        struct dhash_node *dhn;

        dbgf(DBGL_CHANGES, DBGT_INFO, "%s dev=%s to link_id=0x%X iterations=%d time=%d requesting neighIID4x=%d",
                it->handls[ttn->content.type].name, ttn->content.dev->label_cfg.str, ttn->content.link->link_id,
                ttn->tx_iterations, ttn->considered_ts, ttn->content.neighIID4x);

        assertion(-500853, (sizeof ( struct msg_description_request) == sizeof ( struct msg_dhash_request)));
        assertion(-500855, (tx_iterator_cache_data_space(it) >= ((int) (sizeof (struct msg_dhash_request)))));
        assertion(-500856, (ttn->content.link && ttn->content.link->link_id >= LINK_ID_MIN));
        assertion(-500857, (ttn->frame_msgs_length == sizeof ( struct msg_dhash_request)));
        assertion(-500870, (ttn->tx_iterations > 0 && ttn->considered_ts != bmx_time));


        if (ttn->content.link->neigh && (dhn = iid_get_node_by_neighIID4x(ttn->content.link->neigh, ttn->content.neighIID4x))) {

                assertion(-500858, (IMPLIES((dhn && dhn->on), dhn->on->desc)));

                ttn->tx_iterations = 0;

                return TLV_TX_DATA_DONE;
        }

        if (hdr->msg == msg) {

                assertion(-500854, (is_zero(hdr, sizeof (*hdr))));
                hdr->destination_link_id = htonl(ttn->content.link->link_id);

        } else {

                assertion(-500871, (hdr->destination_link_id == htonl(ttn->content.link->link_id)));
        }

        dbgf(DBGL_CHANGES, DBGT_INFO, "creating msg=%d",
                ((int) ((((char*) msg) - ((char*) hdr) - sizeof ( *hdr)) / sizeof (*msg))));

        msg->receiverIID4x = htons(ttn->content.neighIID4x);

        return sizeof (struct msg_dhash_request);
}



STATIC_FUNC
int32_t tx_msg_description_adv(struct tx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        struct tx_task_node * ttn = it->ttn;
        struct dhash_node *dhn;
        struct description *desc0;

        struct msg_description_adv *adv = (struct msg_description_adv *) tx_iterator_cache_msg_ptr(it);

        dbgf_all(DBGT_INFO, "ttn->myIID4x %d", ttn->content.myIID4x);

        assertion( -500555, (ttn->content.myIID4x >= IID_MIN_USED));

        if (ttn->content.myIID4x == myIID4me) { //IID_RSVD_4YOU

                dhn = self.dhn;

        } else if ((dhn = iid_get_node_by_myIID4x(ttn->content.myIID4x))) {

                assertion(-500437, (dhn->on && dhn->on->desc));

        } else {

                dbgf(DBGL_SYS, DBGT_WARN, "INVALID, UNCONFIRMED, or UNKNOWN myIID4x %d !", ttn->content.myIID4x);
                return TLV_DATA_FAILURE;
        }

        adv->transmitterIID4x = htons(ttn->content.myIID4x);
        desc0 = dhn->on->desc;

        uint16_t tlvs_len = ntohs(desc0->dsc_tlvs_len);

        if (tlvs_len + ((int) sizeof (struct msg_description_adv)) > tx_iterator_cache_data_space(it)) {

                dbgf(DBGL_SYS, DBGT_ERR, "tlvs_len=%d min description_len=%zu, cache_data_space=%d",
                        tlvs_len, sizeof (struct msg_description_adv), tx_iterator_cache_data_space(it));

                return TLV_DATA_FULL;
        }

        memcpy((char*) & adv->desc, (char*) desc0, sizeof (struct description) + tlvs_len);

        dbgf(DBGL_CHANGES, DBGT_INFO, "%s descr_size=%zu",
                dhn->on->id.name, (tlvs_len + sizeof (struct msg_description_adv)));

        return (tlvs_len + sizeof (struct msg_description_adv));
}




STATIC_FUNC
int32_t tx_msg_dhash_adv(struct tx_frame_iterator *it)
{

        TRACE_FUNCTION_CALL;
        assertion(-500774, (tx_iterator_cache_data_space(it) >= ((int) sizeof (struct msg_dhash_adv))));
        assertion(-500556, (it && it->ttn->content.myIID4x >= IID_MIN_USED));

        struct tx_task_node *ttn = it->ttn;
        struct msg_dhash_adv *adv = (struct msg_dhash_adv *) tx_iterator_cache_msg_ptr(it);
        struct dhash_node *dhn;

        if (ttn->content.myIID4x == myIID4me) { //IID_RSVD_4YOU

                adv->transmitterIID4x = htons(myIID4me);
                dhn = self.dhn;

        } else if ((dhn = iid_get_node_by_myIID4x(ttn->content.myIID4x))) {

                assertion(-500259, (dhn->on && dhn->on->desc));
                adv->transmitterIID4x = htons(ttn->content.myIID4x);

        } else {
                dbgf(DBGL_SYS, DBGT_WARN, "INVALID, UNCONFIRMED, or UNKNOWN myIID4x %d !", ttn->content.myIID4x);
                return FAILURE;
        }


        memcpy((char*) & adv->dhash, (char*) & dhn->dhash, sizeof ( struct description_hash));

        dbgf(DBGL_CHANGES, DBGT_INFO, "%s", dhn->on->id.name);

        return sizeof (struct msg_dhash_adv);
}




STATIC_FUNC
int32_t tx_frame_ogm_advs(struct tx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        struct tx_task_node *ttn = it->ttn;
        struct list_node *list_pos;

        dbgf_all(DBGT_INFO, " aggregation_sqn %d", ttn->content.u16);

        list_for_each( list_pos, &ogm_aggreg_list )
        {
                struct ogm_aggreg_node *oan = list_entry(list_pos, struct ogm_aggreg_node, list);

                if ( oan->sqn == ttn->content.u16 ) {

                        uint16_t msgs_length = ((oan->aggregated_msgs * sizeof (struct msg_ogm_adv)));
                        struct hdr_ogm_adv* hdr = ((struct hdr_ogm_adv*) tx_iterator_cache_hdr_ptr(it));

                        assertion(-500859, (hdr->msg == ((struct msg_ogm_adv*) tx_iterator_cache_msg_ptr(it))));
                        assertion(-500429, (ttn->frame_msgs_length == msgs_length));
                        assertion(-500428, (((int) (msgs_length)) <= tx_iterator_cache_data_space(it)));

                        hdr->aggregation_sqn = htons(ttn->content.u16);

                        memcpy(hdr->msg, oan->ogm_advs, msgs_length);

                        return (msgs_length);
                }
        }

        // this happens when the to-be-send ogm aggregation has already been purged...
        dbgf(DBGL_SYS, DBGT_WARN, "aggregation_sqn %d does not exist anymore in ogm_aggreg_list", ttn->content.u16);
        ttn->tx_iterations = 0;
        return TLV_TX_DATA_DONE;
}


STATIC_FUNC
int32_t tx_msg_ogm_ack(struct tx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        struct tx_task_node *ttn = it->ttn;
        assertion(-500587, (ttn->content.link && ttn->content.link->link_id >= LINK_ID_MIN));

        struct msg_ogm_ack *ack = (struct msg_ogm_ack *) (tx_iterator_cache_msg_ptr(it));

        ack->transmitterIID4x = htons(ttn->content.myIID4x);
        ack->aggregation_sqn = htons(ttn->content.u16);

        dbgf_all(DBGT_INFO, " aggreg_sqn=%d to link_id=0x%X", ttn->content.u16, ttn->content.link->link_id);

        return sizeof (struct msg_ogm_ack);
}






STATIC_FUNC
int32_t tx_frame_problem_adv(struct tx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        struct tx_task_node *ttn = it->ttn;
        struct msg_problem_adv *adv = ((struct msg_problem_adv*) tx_iterator_cache_msg_ptr(it));

        assertion(-500860, (ttn && ttn->content.u32 >= LINK_ID_MIN));

        dbgf_all(DBGT_INFO, "FRAME_TYPE_PROBLEM_CODE=%d link_id=0x%X", ttn->content.u16, ttn->content.u32);

        if (ttn->content.u16 == FRAME_TYPE_PROBLEM_CODE_DUP_LINK_ID) {
                
                adv->reserved = 0;
                adv->code = ttn->content.u16;
                adv->link_id = htonl(ttn->content.u32);

                return sizeof (struct msg_problem_adv);

        } else {

                assertion(-500846, (0));
        }

        return TLV_DATA_FAILURE;
}




STATIC_FUNC
int32_t rx_frame_problem_adv(struct rx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        struct msg_problem_adv *adv = (struct msg_problem_adv *) it->frame_data;

        if (adv->code == FRAME_TYPE_PROBLEM_CODE_DUP_LINK_ID) {

                if (it->frame_data_length != sizeof (struct msg_problem_adv)) {

                        dbgf(DBGL_SYS, DBGT_ERR,"frame_data_length=%d !!", it->frame_data_length);
                        return TLV_DATA_FAILURE;
                }

                struct packet_buff *pb = it->pb;
                LINK_ID_T link_id =ntohl(adv->link_id);
                
                struct avl_node *an;
                struct dev_node *dev;

                for (an = NULL; (dev = avl_iterate_item(&dev_ip_tree, &an));) {
                        
                        if ( dev->link_id != link_id )
                                continue;

                        if (new_link_id(dev) == LINK_ID_INVALID) {
                                DEV_MARK_PROBLEM(dev);
                                return TLV_DATA_FAILURE;
                        }

                        
                        dbgf(DBGL_SYS, DBGT_ERR, "reselect dev=%s link_id=0x%X (old link_id=0x%X) as signalled by NB=%s",
                                dev->label_cfg.str, dev->link_id, link_id, pb->i.llip_str);
                                
                }


        } else {
                return TLV_RX_DATA_IGNORED;
        }

        return it->frame_data_length;
}


STATIC_FUNC
int32_t rx_frame_ogm_advs(struct rx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        struct hdr_ogm_adv *hdr = (struct hdr_ogm_adv *) it->frame_data;
        struct msg_ogm_adv *ogm = hdr->msg;
        struct packet_buff *pb = it->pb;
        struct neigh_node *nn = pb->i.ln->neigh;

        AGGREG_SQN_T aggregation_sqn = ntohs(hdr->aggregation_sqn);

        uint16_t msgs = (it->frame_data_length - sizeof (struct hdr_ogm_adv)) / sizeof (struct msg_ogm_adv);

        dbgf_all(DBGT_INFO, " ");


        if (((AGGREG_SQN_MASK)& (nn->ogm_aggregation_cleard_max - aggregation_sqn)) >= AGGREG_SQN_CACHE_RANGE) {

                if (nn->ogm_aggregation_cleard_max &&
                        ((AGGREG_SQN_MASK)& (aggregation_sqn - nn->ogm_aggregation_cleard_max)) > AGGREG_SQN_CACHE_WARN) {

                        dbgf(DBGL_CHANGES, DBGT_WARN, "neigh %s with unknown %s aggregation_sqn %d  max %d  ogms %d",
                                pb->i.llip_str, "LOST", aggregation_sqn, nn->ogm_aggregation_cleard_max, msgs);
                }

                bit_clear(nn->ogm_aggregations_rcvd, AGGREG_SQN_CACHE_RANGE, nn->ogm_aggregation_cleard_max + 1, aggregation_sqn);
                nn->ogm_aggregation_cleard_max = aggregation_sqn;

        } else {

                if (bit_get(nn->ogm_aggregations_rcvd, AGGREG_SQN_CACHE_RANGE, aggregation_sqn)) {

                        dbgf_all(DBGT_INFO, "already known ogm_aggregation_sqn %d from neigh %s via dev=%s",
                                aggregation_sqn, nn->dhn->on->id.name, pb->i.iif->label_cfg.str);

                        schedule_tx_task(nn->best_rtq, FRAME_TYPE_OGM_ACKS, 0, aggregation_sqn, 0, nn->dhn->myIID4orig, 0);

                        return it->frame_data_length;

                } else if (((uint16_t) (nn->ogm_aggregation_cleard_max - aggregation_sqn)) > AGGREG_SQN_CACHE_WARN) {

                        dbgf(DBGL_CHANGES, DBGT_WARN, "neigh %s with unknown %s aggregation_sqn %d  max %d  ogms %d",
                                pb->i.llip_str, "OLD", aggregation_sqn, nn->ogm_aggregation_cleard_max, msgs);
                }
        }


        dbgf_all(DBGT_INFO, "neigh %s with unknown %s aggregation_sqn %d  max %d  msgs %d",
                pb->i.llip_str, "NEW", aggregation_sqn, nn->ogm_aggregation_cleard_max, msgs);

        uint16_t m;
        IID_T neighIID4x = 0;

        for (m = 0; m < msgs; m++) {

                uint16_t offset = ((ntohs(ogm[m].mix) >> OGM_IIDOFFST_BIT_POS) & OGM_IIDOFFST_MASK);

                if (offset == OGM_IID_RSVD_JUMP) {

                        uint16_t absolute = ntohs(ogm[m].u.transmitterIIDabsolute);

                        dbgf_all(DBGT_INFO, " IID jump from %d to %d", neighIID4x, absolute);
                        neighIID4x = absolute;

                        if ((m + 1) >= msgs)
                                return FAILURE;

                        continue;

                } else {

                        dbgf_all(DBGT_INFO, " IID offset from %d to %d", neighIID4x, neighIID4x + offset);
                        neighIID4x += offset;
                }

                OGM_SQN_T ogm_sqn = ntohs(ogm[m].u.ogm_sqn);

                IID_NODE_T *dhn = iid_get_node_by_neighIID4x(nn, neighIID4x );
                struct orig_node *on = dhn ? dhn->on : NULL;


                if (on) {

                        if (((OGM_SQN_MASK) & (ogm_sqn - on->ogmSqn_rangeMin)) >= on->ogmSqn_rangeSize) {

                                dbgf(DBGL_SYS, DBGT_ERR,
                                        "EXCEEDED ogm_sqn=%d neighIID4x=%d orig=%s via link=%s sqn_min=%d sqn_range=%d",
                                        ogm_sqn, neighIID4x, on->id.name, pb->i.llip_str,
                                        on->ogmSqn_rangeMin, on->ogmSqn_rangeSize);

                                return FAILURE;
                        }

                        if (dhn == self.dhn || on->blocked) {

                                dbgf_all(DBGT_WARN, "%s orig_sqn=%d/%d orig=%s via link=%s neighIID4x=%d",
                                        dhn == self.dhn ? "MYSELF" : "BLOCKED",
                                        ogm_sqn, on->ogmSqn_toBeSend, on->id.name, pb->i.llip_str, neighIID4x);

                                continue;
                        }


                        OGM_MIX_T mix = ntohs(ogm[m].mix);
                        uint8_t exp = ((mix >> OGM_EXPONENT_BIT_POS) & OGM_EXPONENT_MASK);
                        uint8_t mant = ((mix >> OGM_MANTISSA_BIT_POS) & OGM_MANTISSA_MASK);

                        FMETRIC_T fm = fmetric(mant, exp, on->path_metricalgo->exp_offset);
                        IDM_T valid_metric = is_fmetric_valid(fm);

                        if (!valid_metric) {

                                dbgf_mute(50, DBGL_SYS, DBGT_ERR,
                                        "INVALID metric! orig_sqn=%d/%d orig=%s via link=%s neighIID4x=%d",
                                        ogm_sqn, on->ogmSqn_toBeSend, on->id.name, pb->i.llip_str, neighIID4x);

                                return FAILURE;
                        }

                        UMETRIC_T um = fmetric_to_umetric(fm);

                        if (um < on->path_metricalgo->umetric_min) {

                                dbgf_mute(50, DBGL_SYS, DBGT_ERR,
                                        "UNUSABLE metric=%ju usable=%ju orig_sqn=%d/%d orig=%s via link=%s neighIID4x=%d",
                                        um, on->path_metricalgo->umetric_min,
                                        ogm_sqn, on->ogmSqn_toBeSend, on->id.name, pb->i.llip_str, neighIID4x);

                                continue;
                        } 
                        
                        if (update_path_metrics(pb, on, ogm_sqn, &um) == FAILURE) {

                                dbgf(DBGL_SYS, DBGT_ERR,
                                        "NEW orig_sqn=%d to_be_send=%d sqn_max=%d orig=%s via link=%s neighIID4x=%d",
                                        ogm_sqn, on->ogmSqn_toBeSend, on->ogmSqn_maxRcvd, on->id.name, pb->i.llip_str, neighIID4x);

                                return FAILURE;
                        }

                } else {

                        dbgf(DBGL_CHANGES, DBGT_WARN,
                                "%s orig_sqn=%d or neighIID4x=%d orig=%s via link=%s sqn_min=%d sqn_range=%d",
                                !dhn ? "UNKNOWN DHN" : "INVALIDATED",
                                ogm_sqn, neighIID4x,
                                on ? on->id.name : "---",
                                pb->i.llip_str,
                                on ? on->ogmSqn_rangeMin : 0,
                                on ? on->ogmSqn_rangeSize : 0);

                        if (!dhn && nn->best_rtq && nn->best_rtq->mr[SQR_RTQ].umetric_final > UMETRIC_RQ_NBDISC_MIN)
                                schedule_tx_task(nn->best_rtq, FRAME_TYPE_DHS0_REQS, 0, 0, 0, 0, neighIID4x);

                }
        }

        bit_set(nn->ogm_aggregations_rcvd, AGGREG_SQN_CACHE_RANGE, aggregation_sqn, 1);
        schedule_tx_task(nn->best_rtq, FRAME_TYPE_OGM_ACKS, 0, aggregation_sqn, 0, nn->dhn->myIID4orig, 0);

        return (sizeof (struct hdr_ogm_adv) + (msgs * sizeof (struct msg_ogm_adv)));
}






STATIC_FUNC
int32_t rx_frame_ogm_acks(struct rx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        struct packet_buff *pb = it->pb;
        struct neigh_node *nn = pb->i.ln->neigh;

        uint16_t pos;

        for (pos = 0; pos < it->frame_data_length; pos += sizeof (struct msg_ogm_ack)) {

                struct msg_ogm_ack *ack = (struct msg_ogm_ack *) (it->frame_data + pos);

                if (self.dhn != iid_get_node_by_neighIID4x(nn, ntohs(ack->transmitterIID4x)))
                        continue;

                AGGREG_SQN_T aggregation_sqn = ntohs(ack->aggregation_sqn);

                if (((AGGREG_SQN_MASK)& (ogm_aggreg_sqn_max - aggregation_sqn)) < AGGREG_SQN_CACHE_RANGE) {

                        bit_set(pb->i.ln->neigh->ogm_aggregations_acked, AGGREG_SQN_CACHE_RANGE, aggregation_sqn, 1);

                        dbgf_all(DBGT_INFO, "neigh %s  sqn %d <= sqn_max %d",
                                pb->i.llip_str, aggregation_sqn, ogm_aggreg_sqn_max);

                } else {

                        dbgf(DBGL_SYS, DBGT_ERR, "neigh %s  sqn %d <= sqn_max %d",
                                pb->i.llip_str, aggregation_sqn, ogm_aggreg_sqn_max);

                }
        }

        return pos;
}





STATIC_FUNC
struct dhash_node *process_dhash_description_neighIID4x
(struct packet_buff *pb, struct description_hash *dhash, struct description *dsc, IID_T neighIID4x)
{
        TRACE_FUNCTION_CALL;
        struct dhash_node *orig_dhn = NULL;
        struct link_node *ln = pb->i.ln;
        IDM_T invalid = NO;
        struct description *cache = NULL;
        IDM_T is_transmitter_adv = (neighIID4x == pb->i.transmittersIID);

        assertion(-500688, (dhash));
        assertion(-500689, (!(is_transmitter_adv && !memcmp(dhash, &(self.dhn->dhash), sizeof(*dhash))))); // cant be sender and myself
        assertion(-500511, (!ln || !ln->neigh || (ln->neigh->dhn != self.dhn))); // cant be neighbor and myself

        if (avl_find(&dhash_invalid_tree, dhash)) {

                invalid = YES;

        } else if ((orig_dhn = avl_find_item(&dhash_tree, dhash))) {
                // is about a known dhash:
                
                if (is_transmitter_adv) {
                        // is about the transmitter:

                        if (update_neigh_node(ln, orig_dhn, neighIID4x, dsc) == FAILURE)
                                return DHASH_NODE_FAILURE;

                        if (iid_set_neighIID4x(&ln->neigh->neighIID4x_repos, neighIID4x, orig_dhn->myIID4orig) == FAILURE)
                                return DHASH_NODE_FAILURE;

                        pb->i.described_dhn = is_described_neigh_dhn(pb);

                } else if (ln->neigh) {
                        // received via a known neighbor, and is NOT about the transmitter:

                        if (orig_dhn == self.dhn) {
                                // is about myself:


                                dbgf_all(DBGT_INFO, "msg refers myself via %s neighIID4me %d", pb->i.llip_str, neighIID4x);

                                ln->neigh->neighIID4me = neighIID4x;

                        } else if (orig_dhn == ln->neigh->dhn && !is_transmitter_adv) {
                                // is about a neighbors' dhash itself which is NOT the transmitter ???!!!

                                dbgf(DBGL_SYS, DBGT_ERR, "%s via %s neighIID4x=%d IS NOT transmitter=%d",
                                        orig_dhn->on->id.name, pb->i.llip_str, neighIID4x, pb->i.transmittersIID);

                                return DHASH_NODE_FAILURE;

                        } else {
                                // is about.a another dhash known by me and a (neighboring) transmitter..:
                        }

                        if (iid_set_neighIID4x(&ln->neigh->neighIID4x_repos, neighIID4x, orig_dhn->myIID4orig) == FAILURE)
                                return DHASH_NODE_FAILURE;

                }

        } else {
                // is about an unconfirmed or unkown dhash:

                // its just the easiest to cache and remove because cache description is doing all the checks for us
                if (dsc)
                        cache_description(dsc, dhash);

                if (is_transmitter_adv && (cache = remove_cached_description(dhash))) {
                        //is about the transmitter  and  a description + dhash is known

                        if ((orig_dhn = process_description(pb, cache, dhash))) {

                                if (update_neigh_node(ln, orig_dhn, neighIID4x, dsc) == FAILURE)
                                        return DHASH_NODE_FAILURE;

                                if (iid_set_neighIID4x(&ln->neigh->neighIID4x_repos, neighIID4x, orig_dhn->myIID4orig) == FAILURE)
                                        return DHASH_NODE_FAILURE;

                                pb->i.described_dhn = is_described_neigh_dhn(pb);
                        }

                } else if (ln->neigh && (cache = remove_cached_description(dhash))) {

                        if ((orig_dhn = process_description(pb, cache, dhash))) {

                                if (iid_set_neighIID4x(&ln->neigh->neighIID4x_repos, neighIID4x, orig_dhn->myIID4orig) == FAILURE)
                                        return DHASH_NODE_FAILURE;
                        }
                }
        }


        dbgf(DBGL_CHANGES, DBGT_INFO, "via dev=%s NB=%s dhash=%8X.. %s neighIID4x=%d  is_sender=%d %s",
                pb->i.iif->label_cfg.str, pb->i.llip_str, dhash->h.u32[0],
                (dsc ? "DESCRIPTION" : (cache ? "CACHED_DESCRIPTION" : (orig_dhn?"KNOWN":"UNDESCRIBED"))),
                neighIID4x, is_transmitter_adv,
                invalid ? "INVALIDATED" : (orig_dhn && orig_dhn->on ? orig_dhn->on->id.name : "---"));


        return orig_dhn;
}


STATIC_FUNC
int32_t rx_msg_dhash_adv( struct rx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        struct packet_buff *pb = it->pb;
        struct msg_dhash_adv *adv = (struct msg_dhash_adv*) (it->msg);
        IID_T neighIID4x = ntohs(adv->transmitterIID4x);
        IDM_T is_transmitter_adv = (neighIID4x == pb->i.transmittersIID);
        struct dhash_node *dhn;

        dbgf(DBGL_CHANGES, DBGT_INFO, "via NB: %s", pb->i.llip_str);

        ASSERTION(-500373, (pb->i.ln && pb->i.lndev && pb->i.lndev->mr[SQR_RQ].umetric_final >=
                ((packet_frame_handler[FRAME_TYPE_DHS0_ADVS].umetric_rq_min) ? *(packet_frame_handler[FRAME_TYPE_DHS0_ADVS].umetric_rq_min) : 0)));

        if (neighIID4x <= IID_RSVD_MAX)
                return FAILURE;

        if (!(is_transmitter_adv || pb->i.described_dhn)) {
                dbgf(DBGL_CHANGES, DBGT_INFO, "via unconfirmed NB: %s", pb->i.llip_str);
                return sizeof (struct msg_dhash_adv);
        }

        if ((dhn = process_dhash_description_neighIID4x(pb, &adv->dhash, NULL, neighIID4x)) == DHASH_NODE_FAILURE)
                return FAILURE;

        assertion(-500690, (!dhn || dhn->on));
        assertion(-500488, (!(dhn && is_transmitter_adv) || (dhn->on && pb->i.ln->neigh)));

        if (!dhn)
                schedule_tx_task(pb->i.lndev, FRAME_TYPE_DSC0_REQS, 0, 0, 0, 0, neighIID4x);

        return sizeof (struct msg_dhash_adv);
}


STATIC_FUNC
int32_t rx_frame_description_advs(struct rx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        int32_t pos = 0;
        uint16_t tlvs_len = 0;
        IID_T neighIID4x = 0;
        struct packet_buff *pb = it->pb;

        assertion(-500550, (it->frame_data_length >= ((int)sizeof (struct msg_description_adv))));

        while (pos < it->frame_data_length && pos + ((int) sizeof (struct msg_description_adv)) <= it->frame_data_length) {

                struct msg_description_adv *adv = ((struct msg_description_adv*) (it->frame_data + pos));
                struct description *desc0 = &adv->desc;
                struct description_hash dhash0;
                struct dhash_node *dhn;

                tlvs_len = ntohs(desc0->dsc_tlvs_len);
                neighIID4x = ntohs(adv->transmitterIID4x);
                pos += (sizeof ( struct msg_description_adv) + tlvs_len);

                if (neighIID4x <= IID_RSVD_MAX || tlvs_len > MAX_DESC0_TLV_SIZE || pos > it->frame_data_length)
                        break;

                ShaUpdate(&bmx_sha, (byte*) desc0, (sizeof (struct description) + tlvs_len));
                ShaFinal(&bmx_sha, (byte*)&dhash0);

                dhn = process_dhash_description_neighIID4x(pb, &dhash0, desc0, neighIID4x);

                dbgf_all( DBGT_INFO, "rcvd %s desc0: %jX via %s NB %s",
                        (dhn && dhn != DHASH_NODE_FAILURE) ? "accepted" : "denied",
                        desc0->id.rand.u64[0], pb->i.iif->label_cfg.str, pb->i.llip_str);

                if (dhn == DHASH_NODE_FAILURE)
                        return FAILURE;

                assertion(-500691, (!dhn || (dhn->on)));
                assertion(-500692, (neighIID4x != pb->i.transmittersIID || pb->i.ln->neigh));

                if (tx_unsolicited_desc && dhn && dhn->on->updated_timestamp == bmx_time && pb->i.ln->neigh) {

                        struct link_dev_node **lndev_arr = get_best_lndevs_by_criteria(NULL, pb->i.ln->neigh->dhn);
                        int d;

                        uint16_t desc_len = sizeof ( struct msg_description_adv) + ntohs(dhn->on->desc->dsc_tlvs_len);

                        for (d = 0; (lndev_arr[d]); d++)
                                schedule_tx_task(lndev_arr[d], FRAME_TYPE_DSC0_ADVS, desc_len, 0, 0, dhn->myIID4orig, 0);

                }
        }

        
        if (pos != it->frame_data_length) {

                dbgf(DBGL_SYS, DBGT_ERR, "(pos=%d) + (desc_size=%zu) + (tlvs_len=%d) frame_data_length=%d neighIID4x=%d",
                        pos, sizeof ( struct msg_description_adv), tlvs_len, it->frame_data_length, neighIID4x);

                return FAILURE;
        }

        return pos;
}

STATIC_FUNC
int32_t rx_msg_dhash_or_description_request(struct rx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        assertion( -500365 , (sizeof( struct msg_description_request ) == sizeof( struct msg_dhash_request)));

        struct packet_buff *pb = it->pb;
        struct hdr_dhash_request *hdr = (struct hdr_dhash_request*) (it->frame_data);
        struct msg_dhash_request *req = (struct msg_dhash_request*) (it->msg);
        IID_T myIID4x = ntohs(req->receiverIID4x);
        // remember that the received link_id might be a duplicate:
        LINK_ID_T link_id = ntohl(hdr->destination_link_id);

        dbgf(DBGL_CHANGES, DBGT_INFO, "%s NB %s link_id=0x%X myIID4x %d",
                it->handls[it->frame_type].name, pb->i.llip_str, link_id, myIID4x);


        if (link_id != pb->i.iif->link_id)
                return sizeof ( struct msg_dhash_request);

        if (myIID4x <= IID_RSVD_MAX)
                return sizeof ( struct msg_dhash_request);

        struct dhash_node *dhn = iid_get_node_by_myIID4x(myIID4x);

        assertion(-500270, (IMPLIES(dhn, (dhn->on && dhn->on->desc))));

        if (!dhn || ((TIME_T) (bmx_time - dhn->referred_by_me_timestamp)) > DEF_DESC0_REFERRED_TO) {

                dbgf(DBGL_SYS, DBGT_WARN, "%s from %s requesting %s %s",
                        it->handls[it->frame_type].name, pb->i.llip_str,
                        dhn ? "REFERRED TIMEOUT" : "INVALID or UNKNOWN", dhn ? dhn->on->id.name : "?");

                return sizeof ( struct msg_dhash_request);
        }

        assertion(-500251, (dhn && dhn->myIID4orig == myIID4x));

        uint16_t desc_len = ntohs(dhn->on->desc->dsc_tlvs_len) + sizeof ( struct msg_description_adv);

        if (it->frame_type == FRAME_TYPE_DSC0_REQS) {

                schedule_tx_task(pb->i.lndev, FRAME_TYPE_DSC0_ADVS, desc_len, 0, 0, myIID4x, 0);

        } else {

                schedule_tx_task(pb->i.lndev, FRAME_TYPE_DHS0_ADVS, 0, 0, 0, myIID4x, 0);
        }

        return sizeof ( struct msg_dhash_request);
}

STATIC_FUNC
int32_t rx_frame_hello_replies(struct rx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        struct packet_buff *pb = it->pb;
        struct dev_node *iif = pb->i.iif;
        HELLO_FLAGS_SQN_T sqn = 0;
        int pos, found = 0;

        for (pos = 0; pos < it->frame_data_length; pos += sizeof ( struct msg_hello_reply)) {

                struct msg_hello_reply *msg = (struct msg_hello_reply*) (it->frame_data + pos);
                IID_T neighIID4x = ntohs(msg->transmitterIID4x);

                IID_NODE_T *dhn = iid_get_node_by_neighIID4x(pb->i.ln->neigh, neighIID4x);

                if (dhn == self.dhn) {

                        HELLO_FLAGS_SQN_T flags_sqn = ntohs(msg->hello_flags_sqn);

                        sqn = ((HELLO_SQN_MASK)&(flags_sqn >> HELLO_SQN_BIT_POS));

                        if (((HELLO_SQN_MASK)& (iif->link_hello_sqn - sqn)) > DEF_HELLO_DAD_RANGE) {

                                dbgf(DBGL_SYS, DBGT_WARN,
                                        "DAD-Alert (duplicate link_id, can happen) my hello_sqn=%d != %d via dev=%s NBs' NB=%s !",
                                        iif->link_hello_sqn, sqn, iif->label_cfg.str, pb->i.llip_str);

                                continue;
                        }

                        found++;

                        update_link_metrics(pb->i.lndev, 0, sqn, pb->i.iif->link_hello_sqn, SQR_RTQ, &pb->i.iif->umetric_max);

                        pb->i.lndev->rx_hello_reply_flags = ((HELLO_FLAGS_MASK)&(flags_sqn >> HELLO_FLAGS_BIT_POS));


                } else if (!dhn && pb->i.lndev->mr[SQR_RQ].umetric_final > UMETRIC_RQ_NBDISC_MIN) {

                        schedule_tx_task(pb->i.lndev, FRAME_TYPE_DHS0_REQS, 0, 0, 0, 0, neighIID4x);
                }

        }

        if (found > 2) {
                dbgf(DBGL_SYS, DBGT_WARN,
                        "rcvd %d %s messages in %d-bytes frame_data, last_rcvd_sqn=%d iif->link_hello_sqn=%d umetric=%ju",
                        found, packet_frame_handler[FRAME_TYPE_HELLO_REPS].name, it->frame_data_length,
                        sqn, pb->i.iif->link_hello_sqn, pb->i.lndev->mr[SQR_RTQ].umetric_final);
        }

        return pos;
}



STATIC_FUNC
int32_t rx_msg_hello_adv(struct rx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        struct packet_buff *pb = it->pb;
        struct link_node *ln = pb->i.ln;
        struct msg_hello_adv *msg = (struct msg_hello_adv*) (it->msg);
        HELLO_FLAGS_SQN_T hello_flags_sqn = ntohs(msg->hello_flags_sqn);
        HELLO_FLAGS_SQN_T hello_sqn = ((HELLO_SQN_MASK)&((hello_flags_sqn)>>HELLO_SQN_BIT_POS));

        HELLO_FLAGS_SQN_T hello_flags = ((HELLO_FLAGS_MASK)&((hello_flags_sqn)>>HELLO_FLAGS_BIT_POS));

        dbgf_all(DBGT_INFO, "NB=%s via dev=%s SQN=%d flags=%X",
                pb->i.llip_str, pb->i.iif->label_cfg.str, hello_sqn, hello_flags);

        if (it->msg != it->frame_data) {
                dbgf(DBGL_SYS, DBGT_WARN, "rcvd %d %s messages in %d-bytes frame_data",
                        (it->frame_data_length / ((uint32_t)sizeof (struct msg_hello_adv))),
                        packet_frame_handler[FRAME_TYPE_HELLO_ADVS].name, it->frame_data_length);
        }

        // skip updateing link_node if this SQN is known but not new
        if ((ln->rq_hello_sqn_max || ln->rq_time_max) &&
                ((HELLO_SQN_MASK) &(hello_sqn - ln->rq_hello_sqn_max)) > DEF_HELLO_DAD_RANGE &&
                ((TIME_T) (bmx_time - ln->rq_time_max) < (TIME_T) DEF_HELLO_DAD_RANGE * my_tx_interval)) {

                dbgf(DBGL_SYS, DBGT_INFO, "DAD-Alert NB=%s dev=%s SQN=%d rq_sqn_max=%d dad_range=%d dad_to=%d",
                        pb->i.llip_str, pb->i.iif->label_cfg.str, hello_sqn, ln->rq_hello_sqn_max,
                        DEF_HELLO_DAD_RANGE, DEF_HELLO_DAD_RANGE * my_tx_interval);

                return FAILURE;
        }

        if (pb->i.described_dhn)
                schedule_tx_task(pb->i.lndev, FRAME_TYPE_HELLO_REPS, 0, hello_sqn, 0, pb->i.described_dhn->myIID4orig, 0);

        update_link_metrics(pb->i.lndev, pb->i.transmittersIID, hello_sqn, hello_sqn, SQR_RQ, &pb->i.iif->umetric_max);

        return sizeof (struct msg_hello_adv);
}


int32_t rx_frame_iterate(struct rx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        int t, fl, dl, receptor_result;
        struct frame_handl *fhdl;
        struct packet_buff *pb;

        if (it->frames_pos + ((int)sizeof (struct frame_header_long)) <= it->frames_length) {

                struct frame_header_short *fhs = (struct frame_header_short *) (it->frames_in + it->frames_pos);
                it->frame_type = t = fhs->type;

                assertion(-500775, (fhs->type == ((struct frame_header_long*) fhs)->type));

                if ((it->is_short_header = fhs->is_short)) {

                        fl = fhs->length_2B << 1;
                        it->frame_data_length = dl = fl - sizeof (struct frame_header_short);
                        it->frame_data = it->frames_in + it->frames_pos + sizeof (struct frame_header_short);

                } else {

                        fl = ntohs(((struct frame_header_long*) fhs)->length_1B);
                        it->frame_data_length = dl = fl - sizeof (struct frame_header_long);
                        it->frame_data = it->frames_in + it->frames_pos + sizeof (struct frame_header_long);
                }

                it->frames_pos += fl;

                if (t > it->handl_max || !(it->handls[t].rx_frame_handler || it->handls[t].rx_msg_handler)) {

                        dbgf(DBGL_SYS, DBGT_WARN, "%s - unknown type=%d ! check for updates", it->caller, t);

                        if (t > it->handl_max || fhs->is_relevant)
                                return TLV_DATA_FAILURE;

                        return TLV_RX_DATA_IGNORED;
                }

                fhdl = &it->handls[t];
                pb = it->pb;

                dbgf_all(DBGT_INFO, "%s - type=%s frame_length=%d frame_data_length=%d", it->caller, fhdl->name, fl, dl);

                if (fhdl->is_relevant != fhs->is_relevant) {
                        dbgf(DBGL_SYS, DBGT_ERR, "%s - type=%s frame_length=%d from %s, signals %s but known as %s",
                                it->caller, fhdl->name, fl, pb ? pb->i.llip_str : "---",
                                fhs->is_relevant ? "RELEVANT" : "IRRELEVANT",
                                fhdl->is_relevant ? "RELEVANT" : "IRRELEVANT");
                        return TLV_DATA_FAILURE;
                }


                if (!(it->process_filter == FRAME_TYPE_PROCESS_ALL || it->process_filter == t))
                        return TLV_RX_DATA_IGNORED;

        } else if (it->frames_pos == it->frames_length) {

                return TLV_RX_DATA_DONE;

        } else {
                
                return TLV_DATA_FAILURE;
        }



        if (it->frames_pos > it->frames_length || dl < TLV_DATA_PROCESSED || dl % TLV_DATA_PROCESSED) {
                // not yet processed anything, so return failure:
                return TLV_DATA_FAILURE;

        } else if (dl < fhdl->data_header_size + fhdl->min_msg_size) {

                dbgf(DBGL_SYS, DBGT_WARN, "%s - too small length=%d for type=%s", it->caller, fl, fhdl->name);
                return TLV_DATA_FAILURE;

        } else if (fhdl->fixed_msg_size && (dl - (fhdl->data_header_size)) % fhdl->min_msg_size) {

                dbgf(DBGL_SYS, DBGT_WARN, "%s - nonmaching length=%d for type=%s", it->caller, fl, fhdl->name);
                return TLV_DATA_FAILURE;

        } else if (pb && fhdl->umetric_rq_min &&
                (!pb->i.lndev || pb->i.lndev->mr[SQR_RQ].umetric_final < *(fhdl->umetric_rq_min))) {

                dbg_mute(60, DBGL_CHANGES, DBGT_WARN, "%s - non-sufficient link %s - %s (rq=%ju), skipping type=%s",
                        it->caller, pb->i.iif->ip_llocal_str, pb->i.llip_str,
                        pb->i.lndev ? (pb->i.lndev->mr[SQR_RQ].umetric_final) : 0, fhdl->name);

                return TLV_RX_DATA_IGNORED;

        } else if (pb && fhdl->rx_requires_described_neigh_dhn && !pb->i.described_dhn) {

                dbgf(DBGL_CHANGES, DBGT_INFO, "%s via %s IID=%d of neigh=%s - skipping frame type=%s", it->caller,
                        pb->i.ln->neigh ? "CHANGED" : "UNKNOWN", pb->i.transmittersIID, pb->i.llip_str, fhdl->name);

                return TLV_RX_DATA_IGNORED;

        } else if (pb && fhdl->rx_requires_known_neighIID4me && (!pb->i.ln->neigh || !pb->i.ln->neigh->neighIID4me)) {

                dbgf(DBGL_CHANGES, DBGT_INFO, "%s via %s neighIID4me neigh=%s - skipping frame type=%s", it->caller,
                        pb->i.ln->neigh ? "CHANGED" : "UNKNOWN", pb->i.llip_str, fhdl->name);

                return TLV_RX_DATA_IGNORED;

        } else if (fhdl->rx_msg_handler && fhdl->fixed_msg_size) {
                
                it->msg = it->frame_data + fhdl->data_header_size;

                while (it->msg < it->frame_data + it->frame_data_length &&
                        ((*(fhdl->rx_msg_handler)) (it)) == fhdl->min_msg_size) {

                        it->msg += fhdl->min_msg_size;
                }

                if (it->msg != it->frame_data + it->frame_data_length)
                        return TLV_DATA_FAILURE;

                return it->frame_data_length;

        } else if (fhdl->rx_frame_handler) {

                receptor_result = (*(fhdl->rx_frame_handler)) (it);

                if (receptor_result == TLV_DATA_BLOCKED)
                        return TLV_DATA_BLOCKED;

                if (dl != receptor_result)
                        return TLV_DATA_FAILURE;

                return receptor_result;
        }

        return TLV_DATA_FAILURE;
}

IDM_T rx_frames(struct packet_buff *pb)
{
        TRACE_FUNCTION_CALL;
        int32_t it_result;

        struct rx_frame_iterator it = {
                .caller = __FUNCTION__, .pb = pb,
                .handls = packet_frame_handler, .handl_max = FRAME_TYPE_MAX, .process_filter = FRAME_TYPE_PROCESS_ALL,
                .frames_in = (((uint8_t*) & pb->packet.header) + sizeof (struct packet_header)),
                .frames_length = (ntohs(pb->packet.header.pkt_length) - sizeof (struct packet_header)), .frames_pos = 0
        };

        while ((it_result = rx_frame_iterate(&it)) > TLV_RX_DATA_DONE || it_result == TLV_DATA_BLOCKED);

        if (it_result <= TLV_DATA_FAILURE) {
                dbgf(DBGL_SYS, DBGT_WARN, "problematic frame_type=%s data_length=%d iterator_result=%d pos=%d ",
                        packet_frame_handler[it.frame_type].name, it.frame_data_length, it_result, it.frames_pos);
                return FAILURE;
        }

        if (!pb->i.described_dhn) {
                // this also updates referred_by_neigh_timestamp_sec
                dbgf(DBGL_CHANGES, DBGT_INFO, "%s neighIID4neigh=%d %s",
                        pb->i.ln->neigh ? "CHANGED " : "UNKNOWN", pb->i.transmittersIID, pb->i.llip_str);

                schedule_tx_task(pb->i.lndev, FRAME_TYPE_DHS0_REQS, 0, 0, 0, 0, pb->i.transmittersIID);
        }

        return SUCCESS;
}






STATIC_FUNC
int8_t send_udp_packet(struct packet_buff *pb, struct sockaddr_storage *dst, int32_t send_sock)
{
        TRACE_FUNCTION_CALL;
	int status;

        dbgf_all(DBGT_INFO, "len=%d via dev=%s", pb->i.total_length, pb->i.oif->label_cfg.str);

	if ( send_sock == 0 )
		return 0;

	/*
        static struct iovec iov;
        iov.iov_base = udp_data;
        iov.iov_len  = udp_data_len;

        static struct msghdr m = { 0, sizeof( *dst ), &iov, 1, NULL, 0, 0 };
        m.msg_name = dst;

        status = sendmsg( send_sock, &m, 0 );
         */

        status = sendto(send_sock, pb->packet.data, pb->i.total_length, 0, (struct sockaddr *) dst, sizeof (struct sockaddr_storage));

	if ( status < 0 ) {

		if ( errno == 1 ) {

                        dbg_mute(60, DBGL_SYS, DBGT_ERR, "can't send: %s. Does firewall accept %s dev=%s port=%i ?",
                                strerror(errno), family2Str(((struct sockaddr_in*) dst)->sin_family),
                                pb->i.oif->label_cfg.str ,ntohs(((struct sockaddr_in*) dst)->sin_port));

		} else {

                        dbg_mute(60, DBGL_SYS, DBGT_ERR, "can't send via fd=%d dev=%s : %s",
                                send_sock, pb->i.oif->label_cfg.str, strerror(errno));

		}

		return -1;
	}

	return 0;
}




/*
 * iterates over to be created frames and stores them (including frame_header) in it->frames_out  */
STATIC_FUNC
int tx_frame_iterate(IDM_T iterate_msg, struct tx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        uint8_t t = it->frame_type;
        struct frame_handl *handl = &(it->handls[t]);
        int32_t tlv_result;// = TLV_DATA_DONE;
        assertion(-500977, (handl->min_msg_size));
        assertion(-500776, (it->cache_data_array));
        ASSERTION(-500777, (is_zero((tx_iterator_cache_msg_ptr(it)), tx_iterator_cache_data_space(it))));
        assertion(-500779, (it->frames_out_pos <= it->frames_out_max));
        assertion(-500780, (it->frames_out));
        assertion(-500781, (it->frame_type <= it->handl_max));
        assertion(-500784, (IMPLIES(it->cache_msgs_size, it->cache_msgs_size >= TLV_DATA_PROCESSED)));

        dbgf_all(DBGT_INFO, "from %s iterate_msg=%d frame_type=%d cache_msgs_size=%d cache_data_space=%d",
                it->caller, iterate_msg, it->frame_type, it->cache_msgs_size, tx_iterator_cache_data_space(it));

        if (iterate_msg || handl->tx_frame_handler) {

                if (handl->min_msg_size > tx_iterator_cache_data_space(it))
                        return TLV_DATA_FULL;

                if (it->ttn && it->ttn->frame_msgs_length > tx_iterator_cache_data_space(it))
                        return TLV_DATA_FULL;
        }

        if (handl->tx_msg_handler && iterate_msg) {

                assertion(-500814, (tx_iterator_cache_data_space(it) >= 0));

                if ((tlv_result = (*(handl->tx_msg_handler)) (it)) >= TLV_DATA_PROCESSED) {
                        it->cache_msgs_size += tlv_result;

                } else {
                        assertion(-500810, (tlv_result != TLV_DATA_FAILURE));
                        assertion(-500874, (IMPLIES(!it->cache_msgs_size,
                                is_zero(tx_iterator_cache_hdr_ptr(it), handl->data_header_size))));
                }

                return tlv_result;
        }

        assertion(-500862, (!iterate_msg));

        if (handl->tx_msg_handler && !iterate_msg) {

                tlv_result = it->cache_msgs_size;

                assertion(-500863, (tlv_result >= TLV_DATA_PROCESSED));

        } else {
                assertion(-500803, (handl->tx_frame_handler));
                assertion(-500864, (it->cache_msgs_size == 0));

                tlv_result = (*(handl->tx_frame_handler)) (it);

                if (tlv_result >= TLV_DATA_PROCESSED) {
                        it->cache_msgs_size = tlv_result;
                } else {
                        assertion(-500811, (tlv_result != TLV_DATA_FAILURE));
                        return tlv_result;
                }
        }


        assertion(-500865, (it->cache_msgs_size == tlv_result));
        assertion(-500881, (tlv_result >= TLV_DATA_PROCESSED));
        assertion(-500786, (tx_iterator_cache_data_space(it) >= 0));
        assertion(-500787, (!(tlv_result % TLV_DATA_STEPS)));
        assertion(-500355, (IMPLIES(handl->fixed_msg_size, !(tlv_result % handl->min_msg_size))));

        int32_t cache_pos = tlv_result + handl->data_header_size;
        IDM_T is_short_header = (cache_pos <= SHORT_FRAME_DATA_MAX);
        struct frame_header_short *fhs = (struct frame_header_short *) (it->frames_out + it->frames_out_pos);

        if (is_short_header) {
                fhs->length_2B = ((cache_pos + sizeof ( struct frame_header_short)) >> 1);
                it->frames_out_pos += sizeof ( struct frame_header_short);
        } else {
                struct frame_header_long *fhl = (struct frame_header_long *) fhs;
                memset(fhl, 0, sizeof (struct frame_header_long));
                fhl->length_1B = htons(cache_pos + sizeof ( struct frame_header_long));
                it->frames_out_pos += sizeof ( struct frame_header_long);
        }

        fhs->is_short = is_short_header;
        fhs->is_relevant = handl->is_relevant;
        fhs->type = t;

        memcpy(it->frames_out + it->frames_out_pos, it->cache_data_array, cache_pos);
        it->frames_out_pos += cache_pos;

        dbgf_all(DBGT_INFO, "added %s frame_header type=%s frame_data_length=%d frame_msgs_length=%d",
                is_short_header ? "SHORT" : "LONG", handl->name, cache_pos, tlv_result);

        memset(it->cache_data_array, 0, tlv_result + handl->data_header_size);
        it->cache_msgs_size = 0;

        return tlv_result;
}

STATIC_FUNC
void next_tx_task_list(struct dev_node *dev, struct tx_frame_iterator *it, struct avl_node **link_tree_iterator)
{
        TRACE_FUNCTION_CALL;

        struct link_node *ln = NULL;

        if (it->tx_task_list && it->tx_task_list->items &&
                ((struct tx_task_node*) (list_get_last(it->tx_task_list)))->considered_ts != bmx_time
                ) {
                return;
        }

        if (it->tx_task_list == &(dev->tx_task_lists[it->frame_type]))
                it->frame_type++;

        while ((ln = avl_iterate_item(&link_tree, link_tree_iterator))) {
                struct list_node *lndev_pos;
                struct link_dev_node *lndev = NULL;

                list_for_each(lndev_pos, &ln->lndev_list)
                {
                        lndev = list_entry(lndev_pos, struct link_dev_node, list);

                        if (lndev->key.dev == dev && lndev->tx_task_lists[it->frame_type].items) {

                                assertion(-500866, (lndev->key.link == ln));

                                it->tx_task_list = &(lndev->tx_task_lists[it->frame_type]);

                                dbgf(DBGL_CHANGES, DBGT_INFO,
                                        "found %s lndev ln=%s dev=%s tx_tasks_list.items=%d",
                                        it->handls[it->frame_type].name,
                                        ipXAsStr(af_cfg, &lndev->key.link->link_ip),
                                        dev->label_cfg.str, it->tx_task_list->items);

                                return;
                        }
                }
        }

        *link_tree_iterator = NULL;
        it->tx_task_list = &(dev->tx_task_lists[it->frame_type]);
        return;
}

STATIC_FUNC
void tx_packet(void *devp)
{
        TRACE_FUNCTION_CALL;

        static uint8_t cache_data_array[MAX_UDPD_SIZE] = {0};
        static struct packet_buff pb;
        struct dev_node *dev = devp;

        dbgf_all(DBGT_INFO, "dev=%s", dev->label_cfg.str);

        assertion(-500204, (dev));
        assertion(-500205, (dev->active));
        ASSERTION(-500788, ((pb.packet.data) == ((uint8_t*) (&pb.packet.header))));
        ASSERTION(-500789, ((pb.packet.data + sizeof (struct packet_header)) == ((uint8_t*) &((&pb.packet.header)[1]))));

        struct link_dev_node dummy_lndev = {.key = {.dev = dev, .link = NULL},  .mr = {ZERO_METRIC_RECORD,ZERO_METRIC_RECORD}};

        schedule_tx_task(&dummy_lndev, FRAME_TYPE_HELLO_ADVS, 0, 0, 0, 0, 0);

        memset(&pb.i, 0, sizeof (pb.i));

        struct tx_frame_iterator it = {
                .caller = __FUNCTION__, .handls = packet_frame_handler, .handl_max = FRAME_TYPE_MAX,
                .frames_out = (pb.packet.data + sizeof (struct packet_header)), .frames_out_pos = 0,
                .frames_out_max = (pref_udpd_size - sizeof (struct packet_header)),
                .cache_data_array = cache_data_array, .cache_msgs_size = 0,
                .frame_type = 0, .tx_task_list = NULL
        };

        struct avl_node *link_tree_iterator = NULL;

        while (it.frame_type < FRAME_TYPE_NOP) {

                next_tx_task_list(dev, &it, &link_tree_iterator);

                struct list_node *lpos, *ltmp, *lprev = (struct list_node*) it.tx_task_list;
                int32_t tlv_result = TLV_TX_DATA_DONE;
                struct frame_handl *handl = &packet_frame_handler[it.frame_type];
                uint16_t old_frames_out_pos = it.frames_out_pos;
                uint32_t item =0;

                list_for_each_safe(lpos, ltmp, it.tx_task_list)
                {
                        it.ttn = list_entry(lpos, struct tx_task_node, list);
                        item++;

                        assertion(-500440, (it.ttn->content.type == it.frame_type));
                        ASSERTION(-500917, (IMPLIES(!handl->is_advertisement, it.ttn->content.lndev &&
                                it.ttn->content.lndev == avl_find_item(&link_dev_tree, &it.ttn->content.lndev->key))));
                        ASSERTION(-500918, (IMPLIES(!handl->is_advertisement, it.ttn->content.link &&
                                it.ttn->content.lndev->key.link == it.ttn->content.link &&
                                it.ttn->content.link == avl_find_item(&link_tree, &it.ttn->content.link->link_id))));
                        ASSERTION(-500920, (IMPLIES(!handl->is_advertisement, it.ttn->content.dev &&
                                it.ttn->content.lndev->key.dev == it.ttn->content.dev &&
                                it.ttn->content.dev == avl_find_item(&dev_ip_tree, &it.ttn->content.dev->llocal_ip_key))));


                        dbgf_all(DBGT_INFO, "%s type=%d =%s", dev->label_cfg.str, it.frame_type, handl->name);

                        if (it.ttn->tx_iterations <= 0) {

                                tlv_result = TLV_TX_DATA_DONE;

                        } else if (it.ttn->considered_ts == bmx_time) {
                                // already considered during this tx iteration
                                tlv_result = TLV_TX_DATA_IGNORED;

                        } else if (tx_task_obsolete(it.ttn)) {
                                // too recently send! send later;
                                tlv_result = TLV_TX_DATA_IGNORED;

                        } else if (handl->tx_frame_handler) {

                                tlv_result = tx_frame_iterate(NO/*iterate_msg*/, &it);

                        } else if (handl->tx_msg_handler) {

                                tlv_result = tx_frame_iterate(YES/*iterate_msg*/, &it);

                        } else {
                                assertion(-500818, (0));
                        }

                        if (handl->tx_msg_handler && it.cache_msgs_size &&
                                (tlv_result == TLV_DATA_FULL || lpos == it.tx_task_list->prev)) {// last element in list:
                                
                                int32_t it_result = tx_frame_iterate(NO/*iterate_msg*/, &it);

                                if (it_result < TLV_DATA_PROCESSED) {
                                        dbgf(DBGL_SYS, DBGT_ERR, "unexpected it_result=%d (tlv_result=%d) type=%d",
                                                it_result, tlv_result, it.frame_type);

                                        cleanup_all(-500790);
                                }
                        }

                        dbgf_all(DBGT_INFO, "%s type=%d =%s considered=%d iterations=%d tlv_result=%d item=%d/%d",
                                dev->label_cfg.str, it.frame_type, handl->name, it.ttn->considered_ts,
                                it.ttn->tx_iterations, tlv_result, item, it.tx_task_list->items);

                        if (tlv_result == TLV_TX_DATA_DONE) {

                                it.ttn->considered_ts = bmx_time;
                                it.ttn->tx_iterations--;

                                if (freed_tx_task_node(it.ttn, it.tx_task_list, lprev) == NO)
                                        lprev = lpos;

                                continue;

                        } else if (tlv_result == TLV_TX_DATA_IGNORED) {

                                it.ttn->considered_ts = bmx_time;

                                lprev = lpos;
                                continue;

                        } else if (tlv_result >= TLV_DATA_PROCESSED) {

                                it.ttn->considered_ts = bmx_time;
                                it.ttn->tx_iterations--;

                                if (freed_tx_task_node(it.ttn, it.tx_task_list, lprev) == NO)
                                        lprev = lpos;

                                if (handl->tx_frame_handler || lpos == it.tx_task_list->prev)
                                        break;

                                continue;

                        } else if (tlv_result == TLV_DATA_FULL) {
                                // means not created because would not fit!
                                assertion(-500430, (it.cache_msgs_size || it.frames_out_pos)); // single message larger than max_udpd_size
                                break;

                        } else {

                                dbgf(DBGL_SYS, DBGT_ERR, "frame_type=%d tlv_result=%d",
                                        it.frame_type, tlv_result);
                                assertion(-500791, (0));
                        }
                }

                if (it.frames_out_pos > old_frames_out_pos) {
                        dbgf_all(DBGT_INFO, "prepared frame_type=%s frame_size=%d frames_out_pos=%d",
                                handl->name, (it.frames_out_pos - old_frames_out_pos), it.frames_out_pos);
                }

                assertion(-500796, (!it.cache_msgs_size));
                assertion(-500800, (it.frames_out_pos >= old_frames_out_pos));

                if (tlv_result == TLV_DATA_FULL || (it.frame_type == FRAME_TYPE_NOP && it.frames_out_pos)) {

                        struct packet_header *packet_hdr = &pb.packet.header;

                        assertion(-500208, (it.frames_out_pos && it.frames_out_pos <= it.frames_out_max));

                        pb.i.oif = dev;
                        pb.i.total_length = (it.frames_out_pos + sizeof ( struct packet_header));

                        memset(packet_hdr, 0, sizeof (struct packet_header));

                        packet_hdr->bmx_version = COMPATIBILITY_VERSION;
                        packet_hdr->reserved = 0;
                        packet_hdr->pkt_length = htons(pb.i.total_length);
                        packet_hdr->link_id = htonl(dev->link_id);
                        packet_hdr->transmitterIID = htons(myIID4me);

                        cb_packet_hooks(&pb);

                        send_udp_packet(&pb, &dev->tx_netwbrc_addr, dev->unicast_sock);

                        dbgf_all(DBGT_INFO, "send packet size=%d  via dev=%s",
                                pb.i.total_length, dev->label_cfg.str);

                        memset(&pb.i, 0, sizeof (pb.i));

                        it.frames_out_pos = 0;
                }

        }

        assertion(-500797, (!it.frames_out_pos));
}

void tx_packets( void *unused ) {

        TRACE_FUNCTION_CALL;

        struct avl_node *an;
        struct dev_node *dev;

        TIME_T dev_interval = (my_tx_interval / 10) / dev_ip_tree.items;
        TIME_T dev_next = 0;

        dbgf_all(DBGT_INFO, " ");

        schedule_or_purge_ogm_aggregations(NO);
        // this might schedule a new tx_packet because schedule_tx_packet() believes
        // the stuff we are about to send now is still waiting to be send.

        //remove_task(tx_packet, NULL);
        register_task((my_tx_interval + rand_num(my_tx_interval / 10) - (my_tx_interval / 20)), tx_packets, NULL);

        for (an = NULL; (dev = avl_iterate_item(&dev_ip_tree, &an));) {

                if (dev->linklayer == VAL_DEV_LL_LO)
                        continue;

                register_task(dev_next, tx_packet, dev);

                dev_next += dev_interval;

        }
}


void schedule_my_originator_message( void* unused )
{
        TRACE_FUNCTION_CALL;

        if (((OGM_SQN_MASK) & (self.ogmSqn_toBeSend + 1 - self.ogmSqn_rangeMin)) >= self.ogmSqn_rangeSize)
                update_my_description_adv();

        self.ogmSqn_maxRcvd = set_ogmSqn_toBeSend_and_aggregated(&self, (self.ogmSqn_toBeSend + OGM_SQN_STEP), self.ogmSqn_aggregated);

        dbgf_all(DBGT_INFO, "ogm_sqn %d", self.ogmSqn_toBeSend);

        register_task(my_ogm_interval, schedule_my_originator_message, NULL);
}


STATIC_FUNC
IDM_T validate_description(struct description *desc)
{
        TRACE_FUNCTION_CALL;
        int id_name_len;

        if (
                (id_name_len = strlen(desc->id.name)) >= DESCRIPTION0_ID_NAME_LEN ||
                !is_zero(&desc->id.name[id_name_len], DESCRIPTION0_ID_NAME_LEN - id_name_len) ||
                validate_name(desc->id.name) == FAILURE) {

                dbg(DBGL_SYS, DBGT_ERR, "illegal hostname .. %jX", desc->id.rand.u64[0]);
                return FAILURE;
        }

        if (
                validate_param(desc->ttl_max, MIN_TTL, MAX_TTL, ARG_TTL) ||
                validate_param(ntohs(desc->ogm_sqn_range), MIN_OGM_SQN_RANGE, MAX_OGM_SQN_RANGE, ARG_OGM_SQN_RANGE) ||
                validate_param(ntohs(desc->tx_interval), MIN_TX_INTERVAL, MAX_TX_INTERVAL, ARG_TX_INTERVAL) ||
                0
                ) {

                return FAILURE;
        }

        return SUCCESS;
}


struct dhash_node * process_description(struct packet_buff *pb, struct description *desc, struct description_hash *dhash)
{
        TRACE_FUNCTION_CALL;
        assertion(-500262, (pb && pb->i.ln && desc));
        assertion(-500381, (!avl_find( &dhash_tree, dhash )));

        struct dhash_node *dhn;
        struct orig_node *on = NULL;


        if ( validate_description( desc ) != SUCCESS )
                goto process_desc0_error;


        if ((on = avl_find_item(&orig_tree, &desc->id))) {

                dbgf(DBGL_CHANGES, DBGT_INFO, "%s desc SQN=%d (old_sqn=%d) from orig=%s via dev=%s neigh=%s",
                        pb ? "RECEIVED NEW" : "CHECKING OLD (BLOCKED)", ntohs(desc->dsc_sqn), on->descSqn, desc->id.name,
                        pb ? pb->i.iif->label_cfg.str : "---", pb ? pb->i.llip_str : "---");

                assertion(-500383, (on->dhn));

                if (pb && ((TIME_T) (bmx_time - on->dhn->referred_by_me_timestamp)) < (TIME_T) dad_to) {

                        if (((DESC_SQN_MASK)&(ntohs(desc->dsc_sqn) - (on->descSqn + 1))) > DEF_DESCRIPTION_DAD_RANGE) {

                                dbgf(DBGL_SYS, DBGT_ERR, "DAD-Alert: new dsc_sqn %d not > old %d + 1",
                                        ntohs(desc->dsc_sqn), on->descSqn);

                                goto process_desc0_ignore;
                        }

                        if (UXX_LT(OGM_SQN_MASK, ntohs(desc->ogm_sqn_min), (on->ogmSqn_rangeMin + MAX_OGM_SQN_RANGE))) {

                                dbgf(DBGL_SYS, DBGT_ERR, "DAD-Alert: new ogm_sqn_min %d not > old %d + %d",
                                        ntohs(desc->ogm_sqn_min), on->ogmSqn_rangeMin, MAX_OGM_SQN_RANGE);

                                goto process_desc0_ignore;
                        }
                }

        } else {
                // create new orig:
                on = debugMalloc( sizeof( struct orig_node ), -300128 );
                init_orig_node(on, &desc->id);
        }




        int32_t tlv_result;

        if (pb) {

                if (on->desc && !on->blocked) {
                        tlv_result = process_description_tlvs(pb, on, on->desc, TLV_OP_DEL, FRAME_TYPE_PROCESS_ALL, NULL);
                        assertion(-500808, (tlv_result == TLV_RX_DATA_DONE));
                }

                on->updated_timestamp = bmx_time;
                on->descSqn = ntohs(desc->dsc_sqn);

                on->ogmSqn_rangeMin = ntohs(desc->ogm_sqn_min);
                on->ogmSqn_rangeSize = ntohs(desc->ogm_sqn_range);


                on->ogmSqn_maxRcvd = (OGM_SQN_MASK & (on->ogmSqn_rangeMin - OGM_SQN_STEP));
                set_ogmSqn_toBeSend_and_aggregated(on, on->ogmSqn_maxRcvd, on->ogmSqn_maxRcvd);

        }

        if ((tlv_result = process_description_tlvs(pb, on, desc, TLV_OP_TEST, FRAME_TYPE_PROCESS_ALL, NULL)) == TLV_RX_DATA_DONE) {
                tlv_result = process_description_tlvs(pb, on, desc, TLV_OP_ADD, FRAME_TYPE_PROCESS_ALL, NULL);
                assertion(-500831, (tlv_result == TLV_RX_DATA_DONE)); // checked, so MUST SUCCEED!!
        }

        if (tlv_result == TLV_DATA_FAILURE)
                goto process_desc0_error;

/*
        // actually I want to accept descriptions without any primary IP:
        if (!on->blocked && !ip_set(&on->ort.primary_ip))
                goto process_desc0_error;
*/


        if (on->desc)
                debugFree(on->desc, -300111);

        on->desc = desc;
        desc = NULL;

        struct neigh_node *dhn_neigh = NULL;

        if (on->dhn) {
                dhn_neigh = on->dhn->neigh;
                on->dhn->neigh = NULL;
                on->dhn->on = NULL;
                invalidate_dhash_node(on->dhn);
        }

        on->dhn = dhn = create_dhash_node(dhash, on);

        if ( dhn_neigh ) {
                dhn_neigh->dhn = dhn;
                dhn->neigh = dhn_neigh;
        }

        assertion(-500309, (dhn == avl_find_item(&dhash_tree, &dhn->dhash) && dhn->on == on));
        assertion(-500310, (on == avl_find_item(&orig_tree, &on->desc->id) && on->dhn == dhn));

        return dhn;

process_desc0_error:

        if (on)
                free_orig_node(on);

        blacklist_neighbor(pb);

process_desc0_ignore:

        dbgf(DBGL_SYS, DBGT_WARN,
                "%jX rcvd via %s NB %s", desc ? desc->id.rand.u64[0] : 0, pb->i.iif->label_cfg.str, pb->i.llip_str);

        if (desc)
                debugFree(desc, -300109);

        return NULL;
}


void update_my_description_adv(void)
{
        TRACE_FUNCTION_CALL;
        static uint8_t cache_data_array[MAX_UDPD_SIZE] = {0};
        struct description_hash dhash;
        struct description *dsc = self.desc;

        if (terminating)
                return;

        // put obligatory stuff:
        memset(dsc, 0, sizeof (struct description));

        memcpy(&dsc->id, &self.id, sizeof(struct description_id));


        // add some randomness to the ogm_sqn_range, that not all nodes invalidate at the same time:
        uint16_t random_range = ((DEF_OGM_SQN_RANGE - (DEF_OGM_SQN_RANGE/5)) > MIN_OGM_SQN_RANGE) ?
                DEF_OGM_SQN_RANGE - rand_num(DEF_OGM_SQN_RANGE/5) : DEF_OGM_SQN_RANGE + rand_num(DEF_OGM_SQN_RANGE/5);

        self.ogmSqn_rangeSize = ((OGM_SQN_MASK)&(random_range + OGM_SQN_STEP - 1));

        self.ogmSqn_rangeMin = ((OGM_SQN_MASK)&(self.ogmSqn_rangeMin + MAX_OGM_SQN_RANGE));

        self.ogmSqn_maxRcvd = (OGM_SQN_MASK)&(self.ogmSqn_rangeMin - OGM_SQN_STEP);
        set_ogmSqn_toBeSend_and_aggregated(&self, self.ogmSqn_maxRcvd, self.ogmSqn_maxRcvd);

        dsc->ogm_sqn_min = htons(self.ogmSqn_rangeMin);
        dsc->ogm_sqn_range = htons(self.ogmSqn_rangeSize);
        dsc->tx_interval = htons(my_tx_interval);

        dsc->code_version = htons(CODE_VERSION);
        dsc->dsc_sqn = htons(++(self.descSqn));
        dsc->ttl_max = my_ttl;

        // add all tlv options:
        
        struct tx_frame_iterator it = {
                .caller = __FUNCTION__, .frames_out = (((uint8_t*) dsc) + sizeof (struct description)),
                .handls = description_tlv_handl, .handl_max = FRAME_TYPE_MAX, .frames_out_pos = 0,
                .frames_out_max = MAX_UDPD_SIZE -
                (sizeof (struct packet_header) + sizeof (struct frame_header_long) + sizeof (struct msg_description_adv)),
                .cache_data_array = cache_data_array
        };

        int iterator_result;
        for (it.frame_type = 0; it.frame_type < BMX_DSC_TLV_ARRSZ; it.frame_type++) {

                if (!it.handls[it.frame_type].min_msg_size)
                        continue;

                iterator_result = tx_frame_iterate(NO/*iterate_msg*/, &it);

                assertion(-500792, (iterator_result >= TLV_TX_DATA_DONE));
        }

        dsc->dsc_tlvs_len = htons(it.frames_out_pos);


        dbgf_all(DBGT_INFO, "added description_tlv_size=%d ", it.frames_out_pos);

        // calculate hash: like shown in CTaoCrypt Usage Reference:
        ShaUpdate(&bmx_sha, (byte*) dsc, (it.frames_out_pos + sizeof (struct description)));
        ShaFinal(&bmx_sha, (byte*) & dhash);

        if (self.dhn) {
                self.dhn->on = NULL;
                invalidate_dhash_node( self.dhn );
        }

        self.dhn = create_dhash_node(&dhash, &self);

        myIID4me = self.dhn->myIID4orig;
        myIID4me_timestamp = bmx_time;

        if (tx_unsolicited_desc) {
                uint16_t desc_len = it.frames_out_pos + sizeof (struct msg_description_adv);
                struct link_dev_node **lndev_arr = get_best_lndevs_by_criteria(NULL, self.dhn);
                int d;

                for (d = 0; (lndev_arr[d]); d++)
                        schedule_tx_task(lndev_arr[d], FRAME_TYPE_DSC0_ADVS, desc_len, 0, 0, myIID4me, 0);
        }

        my_description_changed = NO;

}

STATIC_FUNC
void dbg_msg_status ( struct ctrl_node *cn )
{
        TRACE_FUNCTION_CALL;

        dbg_printf(cn, "%s=%d %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d\n",
                ARG_UDPD_SIZE, pref_udpd_size,
                ARG_TX_INTERVAL, my_tx_interval,
                ARG_UNSOLICITED_DESC_ADVS, tx_unsolicited_desc,
                ARG_DHS0_ADVS_TX_ITERS, dhash_tx_iters,
                ARG_DSC0_ADVS_TX_ITERS, desc_tx_iters,
                ARG_OGM_INTERVAL, my_ogm_interval,
                ARG_OGM_TX_ITERS, ogm_tx_iters,
                ARG_OGM_ACK_TX_ITERS, my_ogm_ack_tx_iters);
}


STATIC_FUNC
int32_t opt_show_descriptions(uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn)
{
        TRACE_FUNCTION_CALL;

	if ( cmd == OPT_APPLY ) {

                struct avl_node *an = NULL;
                uint16_t count = 0;
                struct orig_node *on;
                struct list_node *list_pos;
                char *name = NULL;

                int32_t filter = strtol(patch->p_val, NULL, 10);

		list_for_each( list_pos, &patch->childs_instance_list ) {

			struct opt_child *c = list_entry( list_pos, struct opt_child, list );

                        if (!strcmp(c->c_opt->long_name, ARG_DESCRIPTION_NAME)) {

                                name = c->c_val;

			} else if ( !strcmp( c->c_opt->long_name, ARG_DESCRIPTION_IP ) ) {

			}
		}


                while ((on = avl_iterate_item(&orig_tree, &an))) {

                        assertion(-500361, (!on || on->desc));

                        if (name && strcmp(name, on->desc->id.name))
                                continue;

                        dbg_printf(cn, "\nID.name=%-10s ID.rand=%s.. dhash=%s.. code=%-3d last_upd=%-4d last_ref=%-3d path_metric=%ju %s\n",
                                on->id.name,
                                memAsStr(((char*) &(on->id.rand)), 4),
                                memAsStr(((char*) &(on->dhn->dhash)), 4),
                                ntohs(on->desc->code_version),
                                (bmx_time - on->updated_timestamp) / 1000,
                                (bmx_time - on->dhn->referred_by_me_timestamp) / 1000,
                                on->curr_rn ? on->curr_rn->mr.umetric_final : 0, on->blocked ? "BLOCKED" : " ");

                        process_description_tlvs(NULL, on, on->desc, TLV_OP_DEBUG, filter, cn);
                        count++;
                }

		dbg_printf( cn, "\n" );
	}
	return SUCCESS;
}


STATIC_FUNC
struct opt_type msg_options[]=
{
//       ord parent long_name             shrt Attributes                            *ival              min                 max                default              *func,*syntax,*help

	{ODI, 0,0,                         0,  5,0,0,0,0,0,                          0,                 0,                  0,                 0,                   0,
			0,		"\nMessage options:"}
#ifndef LESS_OPTIONS
        ,
        {ODI, 0, ARG_UDPD_SIZE,            0,  5, A_PS1, A_ADM, A_DYI, A_CFA, A_ANY, &pref_udpd_size,   MIN_UDPD_SIZE,      MAX_UDPD_SIZE,     DEF_UDPD_SIZE,       0,
			ARG_VALUE_FORM,	"set preferred udp-data size for send packets"}
        ,
        {ODI, 0, ARG_OGM_TX_ITERS,         0,  5, A_PS1, A_ADM, A_DYI, A_CFA, A_ANY, &ogm_tx_iters,MIN_OGM_TX_ITERS,MAX_OGM_TX_ITERS,DEF_OGM_TX_ITERS,0,
			ARG_VALUE_FORM,	"set maximum resend attempts for ogm aggregations"}
        ,
        {ODI, 0, ARG_UNSOLICITED_DESC_ADVS,0,  5, A_PS1, A_ADM, A_DYI, A_CFA, A_ANY, &tx_unsolicited_desc,MIN_UNSOLICITED_DESC_ADVS,MAX_UNSOLICITED_DESC_ADVS,DEF_UNSOLICITED_DESC_ADVS,0,
			ARG_VALUE_FORM,	"send unsolicited description advertisements after receiving a new one"}
        ,
        {ODI, 0, ARG_DSC0_REQS_TX_ITERS,   0,  5, A_PS1, A_ADM, A_DYI, A_CFA, A_ANY, &desc_req_tx_iters,MIN_DSC0_REQS_TX_ITERS,MAX_DSC0_REQS_TX_ITERS,DEF_DSC0_REQS_TX_ITERS,0,
			ARG_VALUE_FORM,	"set tx iterations for description requests"}
        ,
        {ODI, 0, ARG_DHS0_REQS_TX_ITERS,   0,  5, A_PS1, A_ADM, A_DYI, A_CFA, A_ANY, &dhash_req_tx_iters,MIN_DHS0_REQS_TX_ITERS,MAX_DHS0_REQS_TX_ITERS,DEF_DHS0_REQS_TX_ITERS,0,
			ARG_VALUE_FORM,	"set tx iterations for description-hash requests"}
        ,
        {ODI, 0, ARG_DSC0_ADVS_TX_ITERS,   0,  5, A_PS1, A_ADM, A_DYI, A_CFA, A_ANY, &desc_tx_iters,MIN_DSC0_ADVS_TX_ITERS,MAX_DSC0_ADVS_TX_ITERS,DEF_DSC0_ADVS_TX_ITERS,0,
			ARG_VALUE_FORM,	"set tx iterations for descriptions"}
        ,
        {ODI, 0, ARG_DHS0_ADVS_TX_ITERS,   0,  5, A_PS1, A_ADM, A_DYI, A_CFA, A_ANY, &dhash_tx_iters,MIN_DHS0_ADVS_TX_ITERS,MAX_DHS0_ADVS_TX_ITERS,DEF_DHS0_ADVS_TX_ITERS,0,
			ARG_VALUE_FORM,	"set tx iterations for description hashes"}
        ,
        {ODI, 0, ARG_OGM_ACK_TX_ITERS,     0,  5, A_PS1, A_ADM, A_DYI, A_CFA, A_ANY, &my_ogm_ack_tx_iters,MIN_OGM_ACK_TX_ITERS,MAX_OGM_ACK_TX_ITERS,DEF_OGM_ACK_TX_ITERS,0,
			ARG_VALUE_FORM,	"set tx iterations for ogm acknowledgements"}
        ,
#endif
	{ODI, 0, ARG_DESCRIPTION_GREP,	   0,  5, A_PMN, A_USR, A_DYN, A_ARG, A_ANY, 0,            MIN_DESCRIPTION_GREP,MAX_DESCRIPTION_GREP,DEF_DESCRIPTION_GREP,                  opt_show_descriptions,
			ARG_VALUE_FORM,		"show descriptions of nodes\n"}
        ,
	{ODI,ARG_DESCRIPTION_GREP,ARG_DESCRIPTION_NAME,'n',5,A_CS1,A_USR,A_DYN,A_ARG,A_ANY,	0,		0,	0,0,		opt_show_descriptions,
			"<NAME>",	"only show description of nodes with given name"}

};


STATIC_FUNC
int32_t init_msg( void )
{
        paranoia( -500347, ( sizeof(struct description_hash) != HASH0_SHA1_LEN));

        memset(description_tlv_handl, 0, sizeof(description_tlv_handl));

        ogm_aggreg_sqn_max = ((AGGREG_SQN_MASK)&rand_num(AGGREG_SQN_MAX));

	register_options_array( msg_options, sizeof( msg_options ) );

        InitSha(&bmx_sha);

        register_task(my_ogm_interval, schedule_my_originator_message, NULL);

        struct frame_handl handl;

        memset(&handl, 0, sizeof ( handl));
        handl.is_advertisement = 1;
        handl.is_relevant = 1;
        handl.min_msg_size = sizeof (struct msg_problem_adv);
        handl.fixed_msg_size = 0;
        handl.name = "PROBLEM_ADV";
        handl.tx_frame_handler = tx_frame_problem_adv;
        handl.rx_frame_handler = rx_frame_problem_adv;
        register_frame_handler(packet_frame_handler, FRAME_TYPE_PROBLEM_ADV, &handl);

        memset(&handl, 0, sizeof ( handl));
        handl.is_destination_specific_frame = 1;
        handl.tx_iterations = &desc_req_tx_iters;
        handl.umetric_rq_min = &UMETRIC_RQ_NBDISC_MIN;
        handl.data_header_size = sizeof( struct hdr_description_request);
        handl.min_msg_size = sizeof (struct msg_description_request);
        handl.fixed_msg_size = 1;
        handl.min_tx_interval = DEF_TX_DESC0_REQ_TO;
        handl.name = "DESC_REQ";
        handl.tx_msg_handler = tx_msg_dhash_or_description_request;
        handl.rx_msg_handler = rx_msg_dhash_or_description_request;
        register_frame_handler(packet_frame_handler, FRAME_TYPE_DSC0_REQS, &handl);

        memset(&handl, 0, sizeof ( handl));
        handl.is_advertisement = 1;
        handl.tx_iterations = &desc_tx_iters;
        handl.umetric_rq_min = &UMETRIC_RQ_NBDISC_MIN;
        handl.min_msg_size = sizeof (struct msg_description_adv);
        handl.min_tx_interval = DEF_TX_DESC0_ADV_TO;
        handl.name = "DESC_ADV";
        handl.tx_msg_handler = tx_msg_description_adv;
        handl.rx_frame_handler = rx_frame_description_advs;
        register_frame_handler(packet_frame_handler, FRAME_TYPE_DSC0_ADVS, &handl);

        memset(&handl, 0, sizeof ( handl));
        handl.is_destination_specific_frame = 1;
        handl.tx_iterations = &dhash_req_tx_iters;
        handl.umetric_rq_min = &UMETRIC_RQ_NBDISC_MIN;
        handl.data_header_size = sizeof( struct hdr_dhash_request);
        handl.min_msg_size = sizeof (struct msg_dhash_request);
        handl.fixed_msg_size = 1;
        handl.min_tx_interval = DEF_TX_DHASH0_REQ_TO;
        handl.name = "DHASH_REQ";
        handl.tx_msg_handler = tx_msg_dhash_or_description_request;
        handl.rx_msg_handler = rx_msg_dhash_or_description_request;
        register_frame_handler(packet_frame_handler, FRAME_TYPE_DHS0_REQS, &handl);

        memset(&handl, 0, sizeof ( handl));
        handl.is_advertisement = 1;
        handl.tx_iterations = &dhash_tx_iters;
        handl.umetric_rq_min = &UMETRIC_RQ_NBDISC_MIN;
        handl.min_msg_size = sizeof (struct msg_dhash_adv);
        handl.fixed_msg_size = 1;
        handl.min_tx_interval = DEF_TX_DHASH0_ADV_TO;
        handl.name = "DHASH_ADV";
        handl.tx_msg_handler = tx_msg_dhash_adv;
        handl.rx_msg_handler = rx_msg_dhash_adv;
        register_frame_handler(packet_frame_handler, FRAME_TYPE_DHS0_ADVS, &handl);

        memset(&handl, 0, sizeof ( handl));
        handl.is_advertisement = 1;
        handl.rx_requires_described_neigh_dhn = 0;
        handl.min_msg_size = sizeof (struct msg_hello_adv);
        handl.fixed_msg_size = 1;
        handl.name = "HELLO_ADV";
        handl.tx_msg_handler = tx_msg_hello_adv;
        handl.rx_msg_handler = rx_msg_hello_adv;
        register_frame_handler(packet_frame_handler, FRAME_TYPE_HELLO_ADVS, &handl);

        memset(&handl, 0, sizeof ( handl));
        handl.rx_requires_described_neigh_dhn = 1;
        handl.rx_requires_known_neighIID4me = NO; //otherwise I'll never ask for my neighIID4me
        handl.min_msg_size = sizeof (struct msg_hello_reply);
        handl.fixed_msg_size = 1;
        handl.name = "HELLO_REPLY";
        handl.tx_msg_handler = tx_msg_hello_reply;
        handl.rx_frame_handler = rx_frame_hello_replies;
        register_frame_handler(packet_frame_handler, FRAME_TYPE_HELLO_REPS, &handl);

        memset(&handl, 0, sizeof ( handl));
        handl.is_advertisement = 1;
        handl.rx_requires_described_neigh_dhn = 1;
        handl.rx_requires_known_neighIID4me = 1;
        handl.data_header_size = sizeof ( struct hdr_ogm_adv);
        handl.min_msg_size = sizeof (struct msg_ogm_adv);
        handl.fixed_msg_size = 1;
        handl.name = "OGM_ADV";
        handl.tx_frame_handler = tx_frame_ogm_advs;
        handl.rx_frame_handler = rx_frame_ogm_advs;
        register_frame_handler(packet_frame_handler, FRAME_TYPE_OGM_ADVS, &handl);

        memset(&handl, 0, sizeof ( handl));
        handl.rx_requires_described_neigh_dhn = 1;
        handl.rx_requires_known_neighIID4me = 1;
        handl.tx_iterations = &my_ogm_ack_tx_iters;
        handl.min_msg_size = sizeof (struct msg_ogm_ack);
        handl.fixed_msg_size = 1;
        handl.name = "OGM_ACK";
        handl.tx_msg_handler = tx_msg_ogm_ack;
        handl.rx_frame_handler = rx_frame_ogm_acks;
        register_frame_handler(packet_frame_handler, FRAME_TYPE_OGM_ACKS, &handl);

        return SUCCESS;
}

STATIC_FUNC
void cleanup_msg( void )
{
        schedule_or_purge_ogm_aggregations(YES /*purge_all*/);

        debugFree(get_best_lndevs_by_criteria(NULL, NULL), -300218);
        
        purge_cached_descriptions(YES);

}


struct plugin *msg_get_plugin( void ) {

	static struct plugin msg_plugin;
	memset( &msg_plugin, 0, sizeof ( struct plugin ) );

	msg_plugin.plugin_name = "bmx6_msg_plugin";
	msg_plugin.plugin_size = sizeof ( struct plugin );
        msg_plugin.plugin_code_version = CODE_VERSION;
        msg_plugin.cb_init = init_msg;
	msg_plugin.cb_cleanup = cleanup_msg;
        msg_plugin.cb_plugin_handler[PLUGIN_CB_STATUS] = (void (*) (void*)) dbg_msg_status;

        return &msg_plugin;
}
