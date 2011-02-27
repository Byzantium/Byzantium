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

#include "bmx.h"
#include "iid.h"
#include "tools.h"

struct iid_repos my_iid_repos = { 0,0,0,0,{NULL} };

int8_t iid_extend_repos(struct iid_repos *rep)
{
        TRACE_FUNCTION_CALL;

        dbgf_all(DBGT_INFO, "sizeof iid: %zu,  tot_used %d  arr_size %d ",
                (rep == &my_iid_repos) ? sizeof (IID_NODE_T*) : sizeof (IID_T), rep->tot_used, rep->arr_size);

        assertion(-500217, (rep != &my_iid_repos || IID_SPREAD_FK != 1 || rep->tot_used == rep->arr_size));

        if (rep->arr_size + IID_REPOS_SIZE_BLOCK >= IID_REPOS_SIZE_WARN) {

                dbgf( DBGL_SYS, DBGT_WARN, "%d", rep->arr_size);

                if (rep->arr_size + IID_REPOS_SIZE_BLOCK >= IID_REPOS_SIZE_MAX)
                        return FAILURE;
        }

        int field_size = (rep == &my_iid_repos) ? sizeof (IID_NODE_T*) : sizeof (struct iid_ref);

        if (rep->arr_size) {

                rep->arr.u8 = debugRealloc(rep->arr.u8, (rep->arr_size + IID_REPOS_SIZE_BLOCK) * field_size, -300035);

        } else {

                rep->arr.u8 = debugMalloc(IID_REPOS_SIZE_BLOCK * field_size, -300085);
                rep->tot_used = IID_RSVD_MAX+1;
                rep->min_free = IID_RSVD_MAX+1;
                rep->max_free = IID_RSVD_MAX+1;
        }

        memset(&(rep->arr.u8[rep->arr_size * field_size]), 0, IID_REPOS_SIZE_BLOCK * field_size);

        rep->arr_size += IID_REPOS_SIZE_BLOCK;

        return SUCCESS;
}


void iid_purge_repos( struct iid_repos *rep )
{
        TRACE_FUNCTION_CALL;

        if (rep->arr.u8)
                debugFree(rep->arr.u8, -300135);

        memset(rep, 0, sizeof ( struct iid_repos));

}

void iid_free(struct iid_repos *rep, IID_T iid)
{
        TRACE_FUNCTION_CALL;
        int m = (rep == &my_iid_repos);

        assertion(-500330, (iid > IID_RSVD_MAX));
        assertion(-500228, (iid < rep->arr_size && iid < rep->max_free && rep->tot_used > IID_RSVD_MAX));
        assertion(-500229, ((m ? (rep->arr.node[iid] != NULL) : (rep->arr.ref[iid].myIID4x) != 0)));

        if (m) {
                rep->arr.node[iid] = NULL;
        } else {
                rep->arr.ref[iid].myIID4x = 0;
                rep->arr.ref[iid].referred_by_neigh_timestamp_sec = 0;
        }

        rep->min_free = MIN(rep->min_free, iid);

        if (rep->max_free == iid + 1) {

                IID_T i;

                for (i = iid; i > IID_MIN_USED; i--) {

                        if (m ? (rep->arr.node[i - 1] != NULL) : (rep->arr.ref[i - 1].myIID4x) != 0)
                                break;
                }

                rep->max_free = i;
        }

        rep->tot_used--;

        dbgf_all( DBGT_INFO, "mine %d, iid %d tot_used %d, min_free %d max_free %d",
                m, iid, rep->tot_used, rep->min_free, rep->max_free);

        if (rep->tot_used > 0 && rep->tot_used <= IID_MIN_USED) {

                assertion(-500362, (rep->tot_used == IID_MIN_USED && rep->max_free == IID_MIN_USED && rep->min_free == IID_MIN_USED));

                iid_purge_repos( rep );

        }

}

IID_NODE_T* iid_get_node_by_myIID4x(IID_T myIID4x)
{
        TRACE_FUNCTION_CALL;

        if ( my_iid_repos.max_free <= myIID4x )
                return NULL;

        IID_NODE_T *dhn = my_iid_repos.arr.node[myIID4x];

        assertion(-500328, (!dhn || dhn->myIID4orig == myIID4x));

        if (dhn && !dhn->on) {

                dbgf(DBGL_CHANGES, DBGT_INFO, "myIID4x %d INVALIDATED %d sec ago",
                        myIID4x, (bmx_time - dhn->referred_by_me_timestamp) / 1000);

                return NULL;
        }


        return dhn;
}


IID_NODE_T* _iid_get_node_by_neighIID4x( const char *f, IID_NEIGH_T *nn, IID_T neighIID4x )
{
        TRACE_FUNCTION_CALL;

        if (!nn || nn->neighIID4x_repos.max_free <= neighIID4x) {

                dbgf(DBGL_CHANGES, DBGT_INFO, "%s NB=%s neighIID4x=%d to large for neighIID4x_repos",
                        f, nn ? nn->dhn->on->id.name: "???", neighIID4x);
                return NULL;
        }

        struct iid_ref *ref = &(nn->neighIID4x_repos.arr.ref[neighIID4x]);


        if (!ref->myIID4x ) {
                dbgf(DBGL_CHANGES, DBGT_WARN, "%s neighIID4x=%d not recorded by neighIID4x_repos", f, neighIID4x);

        } else if (((((uint16_t) bmx_time_sec) - ref->referred_by_neigh_timestamp_sec) >
                ((MIN_DHASH_TO - (MIN_DHASH_TO / DHASH_TO_TOLERANCE_FK)) / 1000))) {

                dbgf(DBGL_CHANGES, DBGT_WARN, "%s neighIID4x=%d outdated in neighIID4x_repos, now_sec=%d, ref_sec=%d",
                        f, neighIID4x, bmx_time_sec, ref->referred_by_neigh_timestamp_sec);

        } else {

                ref->referred_by_neigh_timestamp_sec = bmx_time_sec;

                if (ref->myIID4x < my_iid_repos.max_free) {

                        IID_NODE_T *dhn = my_iid_repos.arr.node[ref->myIID4x];

                        if (dhn)
                                return dhn;

                        dbgf(DBGL_CHANGES, DBGT_WARN, "%s neighIID4x=%d -> myIID4x=%d empty!",
                                f, neighIID4x, ref->myIID4x);

                } else {

                        dbgf(DBGL_CHANGES, DBGT_WARN, "%s neighIID4x=%d -> myIID4x=%d to large!",
                                f, neighIID4x, ref->myIID4x);
                }
        }

        return NULL;
}


STATIC_FUNC
void _iid_set(struct iid_repos *rep, IID_T IIDpos, IID_T myIID4x, IID_NODE_T *dhn)
{
        TRACE_FUNCTION_CALL;
        assertion(-500530, (rep && XOR(myIID4x, dhn))); // eihter the one ore the other !!
        assertion(-500531, (!dhn || rep == &my_iid_repos));
        assertion(-500535, (IIDpos >= IID_MIN_USED));

        rep->tot_used++;
        rep->max_free = MAX( rep->max_free, IIDpos+1 );

        IID_T min = rep->min_free;

        if (min == IIDpos) {
                for (min++; min < rep->arr_size; min++) {

                        if (myIID4x ? !(rep->arr.ref[min].myIID4x) : !(rep->arr.node[min]))
                                break;
                }
        }

        paranoia(-500244, (min > rep->max_free));

        rep->min_free = min;

        if (myIID4x) {
                rep->arr.ref[IIDpos].myIID4x = myIID4x;
                rep->arr.ref[IIDpos].referred_by_neigh_timestamp_sec = bmx_time_sec;
        } else {
                rep->arr.node[IIDpos] = dhn;
                dhn->referred_by_me_timestamp = bmx_time;
        }
}


IID_T iid_new_myIID4x(IID_NODE_T *dhn)
{
        TRACE_FUNCTION_CALL;
        IID_T mid;
#ifndef NO_ASSERTIONS
        IDM_T warn = 0;
#endif

        assertion(-500216, (my_iid_repos.tot_used <= my_iid_repos.arr_size));

        while (my_iid_repos.arr_size <= my_iid_repos.tot_used * IID_SPREAD_FK)
                iid_extend_repos( &my_iid_repos );

        if (IID_SPREAD_FK > 1) {

                uint32_t random = rand_num(my_iid_repos.arr_size);

                // Never put random function intro MAX()! It would be called twice
                mid = MAX(IID_MIN_USED, random);

                while (my_iid_repos.arr.node[mid]) {

                        mid++;
                        if (mid >= my_iid_repos.arr_size) {

                                mid = IID_MIN_USED;

                                assertion(-500533, (!(warn++)));
                        }
                }

        } else {
                
                mid = my_iid_repos.min_free;
        }

        _iid_set(&my_iid_repos, mid, 0, dhn);

        return mid;

}


IDM_T iid_set_neighIID4x(struct iid_repos *neigh_rep, IID_T neighIID4x, IID_T myIID4x)
{
        TRACE_FUNCTION_CALL;
        assertion(-500326, (neighIID4x > IID_RSVD_MAX));
        assertion(-500327, (myIID4x > IID_RSVD_MAX));
        assertion(-500384, (neigh_rep && neigh_rep != &my_iid_repos));

        assertion(-500245, (my_iid_repos.max_free > myIID4x));

        IID_NODE_T *dhn = my_iid_repos.arr.node[myIID4x];

        assertion(-500485, (dhn && dhn->on));

        dhn->referred_by_me_timestamp = bmx_time;

        if (neigh_rep->max_free > neighIID4x) {

                struct iid_ref *ref = &(neigh_rep->arr.ref[neighIID4x]);

                if (ref->myIID4x > IID_RSVD_MAX) {

                        if (ref->myIID4x == myIID4x ||
                                (((uint16_t)(((uint16_t) bmx_time_sec) - ref->referred_by_neigh_timestamp_sec)) >=
                                ((MIN_DHASH_TO - (MIN_DHASH_TO / DHASH_TO_TOLERANCE_FK)) / 1000))) {

                                ref->myIID4x = myIID4x;
                                ref->referred_by_neigh_timestamp_sec = bmx_time_sec;
                                return SUCCESS;
                        }

                        IID_NODE_T *dhn_old = my_iid_repos.arr.node[ref->myIID4x];

                        dbgf(DBGL_SYS, DBGT_ERR, "demanding mapping: neighIID4x=%d to myIID4x=%d "
                                "(%s updated=%d last_referred_by_me=%d)  "
                                "already used for ref->myIID4x=%d (last_referred_by_neigh_sec=%d %s=%jd last_referred_by_me=%jd)! Reused faster than allowed!!",
                                neighIID4x, myIID4x, dhn->on->id.name, dhn->on->updated_timestamp,
                                dhn->referred_by_me_timestamp,
                                ref->myIID4x,
                                ref->referred_by_neigh_timestamp_sec,
                                (!dhn_old ? "???" : (dhn_old->on ? dhn_old->on->id.name :
                                (!dhn_old->dhash.h.u32[0] && !dhn_old->dhash.h.u32[1] && !dhn_old->dhash.h.u32[2] ? "FREED" : "INVALIDATED"))),
                                dhn_old ? (int64_t) dhn_old->dhash.h.u32[(sizeof ( struct description_hash) / sizeof (uint32_t)) - 1] : -1,
                                dhn_old ? (int64_t)dhn_old->referred_by_me_timestamp : -1
                                );

//                        EXITERROR(-500701, (0));
                        
                        return FAILURE;
                }

                assertion(-500242, (ref->myIID4x == IID_RSVD_UNUSED));
        }


        while (neigh_rep->arr_size <= neighIID4x) {

                if (
                        neigh_rep->arr_size > IID_REPOS_SIZE_BLOCK &&
                        neigh_rep->arr_size > my_iid_repos.arr_size &&
                        neigh_rep->tot_used < neigh_rep->arr_size / (2 * IID_SPREAD_FK)) {

                        dbgf(DBGL_SYS, DBGT_WARN, "IID_REPOS USAGE WARNING neighIID4x %d myIID4x %d arr_size %d used %d",
                                neighIID4x, myIID4x, neigh_rep->arr_size, neigh_rep->tot_used );
                }

                iid_extend_repos(neigh_rep);
        }

        assertion(-500243, ((neigh_rep->arr_size > neighIID4x &&
                (neigh_rep->max_free <= neighIID4x || neigh_rep->arr.ref[neighIID4x].myIID4x == IID_RSVD_UNUSED))));

        _iid_set( neigh_rep, neighIID4x, myIID4x, NULL);

        return SUCCESS;
}


void iid_free_neighIID4x_by_myIID4x( struct iid_repos *rep, IID_T myIID4x)
{
        TRACE_FUNCTION_CALL;
        assertion(-500282, (rep != &my_iid_repos));
        assertion(-500328, (myIID4x > IID_RSVD_MAX));

        IID_T p;
        uint16_t removed = 0;

        for (p = IID_RSVD_MAX + 1; p < rep->max_free; p++) {

                if (rep->arr.ref[p].myIID4x == myIID4x) {

                        if (removed++) {
                                // there could indeed be several (if the neigh has timeouted this node and learned it again later)
                                dbgf(DBGL_TEST, DBGT_INFO, "removed %d. stale rep->arr.sid[%d] = %d", removed, p, myIID4x);
                        }

                        iid_free(rep, p);
                }
        }
}


