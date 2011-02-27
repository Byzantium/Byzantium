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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#include "bmx.h"
#include "msg.h"
#include "ip.h"
#include "plugin.h"
#include "schedule.h"
#include "tools.h"
#include "metrics.h"



static int32_t my_path_metricalgo = DEF_METRIC_ALGO;
static int32_t my_path_metric_flags = DEF_PATH_METRIC_FLAGS;
static int32_t my_fmetric_exp_min = DEF_FMETRIC_EXPONENT_MIN;
static int32_t my_path_window = DEF_PATH_WINDOW;
static int32_t my_path_lounge = DEF_PATH_LOUNGE;
static int32_t my_path_regression_slow = DEF_PATH_REGRESSION_SLOW;
static int32_t my_path_regression_fast = DEF_PATH_REGRESSION_FAST;
static int32_t my_path_regression_diff = DEF_PATH_REGRESSION_DIFF;
static int32_t my_path_hystere = DEF_PATH_HYST;
static int32_t my_hop_penalty = DEF_HOP_PENALTY;
static int32_t my_late_penalty = DEF_LATE_PENAL;


static int32_t my_link_window = DEF_LINK_WINDOW;
static int32_t my_link_metric_flags = DEF_LINK_METRIC_FLAGS;
static int32_t my_link_regression_slow = DEF_LINK_REGRESSION_SLOW;
static int32_t my_link_regression_fast = DEF_LINK_REGRESSION_FAST;
static int32_t my_link_regression_diff = DEF_LINK_REGRESSION_DIFF;

static int32_t my_link_rtq_lounge = DEF_LINK_RTQ_LOUNGE;

int32_t link_ignore_min = DEF_LINK_IGNORE_MIN;

int32_t link_ignore_max = DEF_LINK_IGNORE_MAX;


struct host_metricalgo link_metric_algo[SQR_RANGE];

//TODO: evaluate my_fmetric_exp_offset based on max configured dev_metric_max:
//static int32_t my_fmetric_exp_offset = DEF_FMETRIC_EXP_OFFSET;
//TODO: reevaluate my_dev_metric_max based on deduced my_fmetric_exp_offset (see above)
//UMETRIC_T my_dev_metric_max = umetric_max(DEF_FMETRIC_EXP_OFFSET);

static
void (*path_metric_algos[BIT_METRIC_ALGO_ARRSZ])
(UMETRIC_T *path_out, struct link_dev_node *link, struct host_metricalgo *algo) = {NULL};



//TODO: remove:
UMETRIC_T UMETRIC_RQ_NBDISC_MIN = 2048;
UMETRIC_T UMETRIC_RQ_OGM_ACK_MIN = 1024;



FMETRIC_T fmetric(uint8_t mantissa, uint8_t exp_reduced, uint8_t exp_offset)
{

        FMETRIC_T fm = {.val.f =
                {.mantissa = mantissa, .exp_total = exp_offset + exp_reduced},
                .exp_offset = exp_offset,
                .reserved = FMETRIC_UNDEFINED};

        return fm;
}

FMETRIC_T fmetric_max(uint8_t exp_offset)
{
        FMETRIC_T fm = {.val.f = {.exp_total = exp_offset + OGM_EXPONENT_MAX, .mantissa = OGM_MANTISSA_MAX}, .exp_offset = exp_offset, .reserved = FMETRIC_UNDEFINED};
        return fm;
}


UMETRIC_T umetric(uint8_t mantissa, uint8_t exp_reduced, uint8_t exp_offset)
{
	return fmetric_to_umetric(fmetric(mantissa, exp_reduced, exp_offset));
}



UMETRIC_T umetric_max(uint8_t exp_offset)
{

        UMETRIC_T max = fmetric_to_umetric(fmetric_max(exp_offset));

        ASSERTION(-500703, (max == (((((UMETRIC_T) 1) << (OGM_MANTISSA_BIT_SIZE + 1)) - 1) *
                (((UMETRIC_T) 1) << (exp_offset + ((1 << (OGM_EXPONENT_BIT_SIZE)) - 1) - OGM_MANTISSA_BIT_SIZE)))));

        return max;
}



IDM_T is_umetric_valid(uint8_t exp_offset, UMETRIC_T *um)
{
        assertion(-500704, (um));
        return ( *um <= umetric_max(exp_offset));
}


IDM_T is_fmetric_valid(FMETRIC_T fm)
{
        return (
                fm.exp_offset <= MAX_FMETRIC_EXP_OFFSET &&
                fm.val.f.exp_total >= fm.exp_offset &&
                fm.val.f.exp_total <= OGM_EXPONENT_MAX + fm.exp_offset &&
                fm.val.f.mantissa <= OGM_MANTISSA_MAX
                );
}



IDM_T fmetric_cmp(FMETRIC_T a, unsigned char cmp, FMETRIC_T b)
{


        assertion(-500706, (is_fmetric_valid(a) && is_fmetric_valid(b)));

        switch (cmp) {

        case '!':
                return (a.val.u != b.val.u);
        case '<':
                return (a.val.u < b.val.u);
        case '[':
                return (a.val.u <= b.val.u);
        case '=':
                return (a.val.u == b.val.u);
        case ']':
                return (a.val.u >= b.val.u);
        case '>':
                return (a.val.u > b.val.u);
        }

        assertion(-500707, (0));
        return FAILURE;
}


UMETRIC_T fmetric_to_umetric(FMETRIC_T fm)
{
        TRACE_FUNCTION_CALL;

        uint8_t exp_sum = (fm.val.f.exp_total + OGM_MANTISSA_BIT_SIZE);

        UMETRIC_T val =
                (((UMETRIC_T) 1) << exp_sum) +
                (((UMETRIC_T) fm.val.f.mantissa) << (exp_sum - OGM_MANTISSA_BIT_SIZE));


        if (!is_fmetric_valid(fm)) {

                dbgf(DBGL_SYS, DBGT_ERR, "mantissa=%d exp_total=%d exp_offset=%d mant_bit_size=%d -> U64x2eM=%ju ( %ju )",
                        fm.val.f.mantissa, fm.val.f.exp_total, fm.exp_offset, OGM_MANTISSA_BIT_SIZE, val, val >> TMP_MANTISSA_BIT_SHIFT);

                assertion(-500680, (0));
        }


        return (val >> TMP_MANTISSA_BIT_SHIFT);
}



FMETRIC_T umetric_to_fmetric(uint8_t exp_offset, UMETRIC_T val)
{
        TRACE_FUNCTION_CALL;

        FMETRIC_T fm = {.val.f={.mantissa = 0, .exp_total = exp_offset}, .exp_offset = exp_offset, .reserved = FMETRIC_UNDEFINED};
        uint8_t exp_sum;

        // these fixes are used to improove (average) rounding errors
#define INPUT_FIXA (OGM_MANTISSA_BIT_SIZE+1)
#define INPUT_FIXB (OGM_MANTISSA_BIT_SIZE+5)

        UMETRIC_T tmp = (val = (val + (val>>INPUT_FIXA) - (val>>INPUT_FIXB) )<< TMP_MANTISSA_BIT_SHIFT); // 4-bit-mantissa


        LOG2(exp_sum, tmp, UMETRIC_T);


        //assertion(-500678, ((exp_sum - (exp_offset + MANTISSA_BIT_SIZE)) < (1 << EXPONENT_BIT_SIZE)));

        if (exp_sum > (exp_offset + OGM_MANTISSA_BIT_SIZE + (1 << OGM_EXPONENT_BIT_SIZE) - 1)) {

                fm.val.f.exp_total += (1 << OGM_EXPONENT_BIT_SIZE) - 1;
                fm.val.f.mantissa = (1 << OGM_MANTISSA_BIT_SIZE) - 1;

        } else if (exp_sum >= (exp_offset + OGM_MANTISSA_BIT_SIZE)) {

                fm.val.f.exp_total = (exp_sum - OGM_MANTISSA_BIT_SIZE);

                fm.val.f.mantissa = ((val - (((uint64_t) 1) << exp_sum)) >> (exp_sum - OGM_MANTISSA_BIT_SIZE));

                //check for overflow, so dont exchange this expression with fm.val.f.mantissa:
                assertion(-500677, (((val - (((uint64_t) 1) << exp_sum)) >> (exp_sum - OGM_MANTISSA_BIT_SIZE)) < (1 << OGM_MANTISSA_BIT_SIZE)));
        }

        if (!is_fmetric_valid(fm)) {

                dbgf(DBGL_SYS, DBGT_ERR, "mantissa=%d exp_total=%d exp_offset=%d mant_bit_size=%d -> U64x2eM=%ju ( %ju )",
                        fm.val.f.mantissa, fm.val.f.exp_total, fm.exp_offset, OGM_MANTISSA_BIT_SIZE, val, val >> TMP_MANTISSA_BIT_SHIFT);

                assertion(-500681, (0));
        }

        return fm;
}

STATIC_INLINE_FUNC
FMETRIC_T fmetric_substract_min(FMETRIC_T f)
{

        if (f.val.f.mantissa) {
                
                f.val.f.mantissa--;

        } else if (f.val.f.exp_total > f.exp_offset) {
                
                f.val.f.mantissa = OGM_MANTISSA_MAX;
                f.val.f.exp_total--;
        }
        
        return f;
}

STATIC_INLINE_FUNC
UMETRIC_T umetric_substract_min(uint8_t exp_offset, UMETRIC_T *val)
{
        return fmetric_to_umetric(fmetric_substract_min(umetric_to_fmetric(exp_offset, *val)));
}




STATIC_FUNC
void path_metricalgo_RQv0(UMETRIC_T *path_out, struct link_dev_node *link, struct host_metricalgo *algo)
{
        // make sure this product does not exceed U64 range:
        assertion(-500833, ((*path_out * link->mr[SQR_RQ].umetric_final) >= link->mr[SQR_RQ].umetric_final));

        *path_out = (*path_out * link->mr[SQR_RQ].umetric_final) / link->key.dev->umetric_max;
}

STATIC_FUNC
void path_metricalgo_TQv0(UMETRIC_T *path_out, struct link_dev_node *link, struct host_metricalgo *algo)
{
        // make sure this product does not exceed U64 range:
        // TQ = RTQ / RQ
        assertion(-500834, ((*path_out * link->mr[SQR_RTQ].umetric_final) >= link->mr[SQR_RTQ].umetric_final));

        *path_out = (*path_out * link->mr[SQR_RTQ].umetric_final) / link->mr[SQR_RQ].umetric_final;
}

STATIC_FUNC
void path_metricalgo_RTQv0(UMETRIC_T *path_out, struct link_dev_node *link, struct host_metricalgo *algo)
{
        // make sure this product does not exceed U64 range:
        assertion(-500835, ((*path_out * link->mr[SQR_RTQ].umetric_final) >= link->mr[SQR_RTQ].umetric_final));

        *path_out = (*path_out * link->mr[SQR_RTQ].umetric_final) / link->key.dev->umetric_max;
}

STATIC_FUNC
void path_metricalgo_ETXv0(UMETRIC_T *path_out, struct link_dev_node *link, struct host_metricalgo *algo)
{
        // make sure this product does not exceed U64 range:
        assertion(-500836, ((umetric_max(algo->exp_offset) * link->mr[SQR_RTQ].umetric_final) >= link->mr[SQR_RTQ].umetric_final));

        UMETRIC_T rtq_link = (umetric_max(algo->exp_offset) * link->mr[SQR_RTQ].umetric_final) / link->key.dev->umetric_max;

        if (*path_out < 2 || rtq_link < 2)
                *path_out = umetric(FMETRIC_MANTISSA_ZERO, 0, algo->exp_offset);
        else
                *path_out = (U64_MAX / ((U64_MAX / *path_out) + (U64_MAX / rtq_link)));
}

STATIC_FUNC
void path_metricalgo_ETTv0(UMETRIC_T *path_out, struct link_dev_node *link, struct host_metricalgo *algo)
{
        // make sure this product does not exceed U64 range:
        assertion(-500827, ((umetric_max(algo->exp_offset) * link->mr[SQR_RTQ].umetric_final) >= link->mr[SQR_RTQ].umetric_final));

        UMETRIC_T bw_link = link->mr[SQR_RTQ].umetric_final;

        if (*path_out < 2 || bw_link < 2)
                *path_out = umetric(FMETRIC_MANTISSA_ZERO, 0, algo->exp_offset);
        else
                *path_out = (U64_MAX / ((U64_MAX / *path_out) + (U64_MAX / bw_link)));
}

STATIC_FUNC
void path_metricalgo_MINBANDWIDTH(UMETRIC_T *path_out, struct link_dev_node *link, struct host_metricalgo *algo)
{
        *path_out = MIN(*path_out, link->mr[SQR_RTQ].umetric_final);
}


STATIC_FUNC
void register_path_metricalgo(uint8_t algo_type_bit, void (*algo) (UMETRIC_T *path_out, struct link_dev_node *link, struct host_metricalgo *algo))
{
        assertion(-500838, (!path_metric_algos[algo_type_bit]));
        assertion(-500839, (algo_type_bit < BIT_METRIC_ALGO_ARRSZ));
        path_metric_algos[algo_type_bit] = algo;
}


void apply_metric_algo(UMETRIC_T *out, struct link_dev_node *link, UMETRIC_T *path, struct host_metricalgo *algo)
{
        TRACE_FUNCTION_CALL;
        assertion(-500823, (link->key.dev->umetric_max));

        ALGO_T unsupported_algos = 0;
        ALGO_T algo_type = algo->algo_type;
        UMETRIC_T max_out, path_out;

        if (algo_type) {
                max_out = umetric_substract_min(algo->exp_offset, path);
                path_out = *path;

                while (algo_type) {

                        uint8_t algo_type_bit;
                        ALGO_T algo_type_tmp = algo_type;
                        LOG2(algo_type_bit, algo_type_tmp, ALGO_T);

                        algo_type -= (0x01 << algo_type_bit);

                        if (path_metric_algos[algo_type_bit]) {
                                (*(path_metric_algos[algo_type_bit])) (&path_out, link, algo);
                                dbgf_all(DBGT_INFO, "pathalgo_type=%d returned path_out=%ju", algo_type_bit, path_out);
                        } else {
                                unsupported_algos |= (0x01 << algo_type_bit);
                        }
                }

        } else {
                max_out = path_out = umetric(FMETRIC_MANTISSA_ZERO, 0, algo->exp_offset);
        }


        if (unsupported_algos) {
                uint8_t i = bits_count(unsupported_algos);

                dbgf(DBGL_SYS, DBGT_WARN,
                        "unsupported %s=%d (0x%X) - Need an update?! - applying pessimistic ETTv0 %d times",
                        ARG_PATH_METRIC_ALGO, unsupported_algos, unsupported_algos, i);

                while (i--)
                        (*(path_metric_algos[BIT_METRIC_ALGO_ETTv0])) (&path_out, link, algo);
        }

        if (algo->hop_penalty)
                path_out = (path_out * ((UMETRIC_T) (MAX_HOP_PENALTY - algo->hop_penalty))) >> MAX_HOP_PENALTY_PRECISION_EXP;


        if (path_out > max_out) {
                dbgf_all(DBGT_WARN,
                        "out=%ju > out_max=%ju, %s=%d, path=%ju, dev=%s, link_MAX=%ju, link_RTQ=%ju, link_RQ=%ju",
                        path_out, max_out, ARG_PATH_METRIC_ALGO, algo->algo_type, *path,
                        link->key.dev->label_cfg.str, link->key.dev->umetric_max,
                        link->mr[SQR_RTQ].umetric_final, link->mr[SQR_RQ].umetric_final);
        }

        *out = MIN(path_out, max_out); // ensure out always decreases
}

STATIC_FUNC
void _reconfigure_metric_record_position(const char *f, struct metric_record *rec, struct host_metricalgo *alg,
        SQN_T min, SQN_T in, uint8_t sqn_bit_size, uint8_t reset)
{
        TRACE_FUNCTION_CALL;
        assertion(-500737, (XOR(sqn_bit_size, rec->sqn_bit_mask)));

        if (sqn_bit_size)
                rec->sqn_bit_mask = (~(SQN_MAX << sqn_bit_size));

        assertion(-500738, (rec->sqn_bit_mask == U16_MAX || rec->sqn_bit_mask == U8_MAX ||
                rec->sqn_bit_mask == HELLO_SQN_MAX));


        if (rec->clr && /*alg->window_size > 1 &&*/ ((rec->sqn_bit_mask)&(in - rec->clr)) > (alg->lounge_size + 1)) {
                dbgf(DBGL_CHANGES, DBGT_WARN, "reset_metric=%d sqn_bit_size=%d sqn_bit_mask=0x%X umetric=%ju clr=%d to ((in=%d) - (lounge=%d))=%d",
                        reset, sqn_bit_size, rec->sqn_bit_mask, rec->umetric_final, rec->clr, in, alg->lounge_size, ((rec->sqn_bit_mask)& (in - alg->lounge_size)));
        }

        rec->clr = MAX_UXX(rec->sqn_bit_mask, min, ((rec->sqn_bit_mask)&(in - alg->lounge_size)));

        rec->set = ((rec->sqn_bit_mask)&(rec->clr - 1));

        if (reset) {
                rec->umetric_slow = 0;
                rec->umetric_fast = 0;
                rec->umetric_final = 0;
        }

        dbgf_all(DBGT_WARN, "%s reset_metric=%d sqn_bit_size=%d sqn_bit_mask=0x%X min=%d in=%d lounge_size=%d umetric=%ju clr=%d",
                f, reset, sqn_bit_size, rec->sqn_bit_mask, min, in, alg->lounge_size, rec->umetric_final, rec->clr);

}

#define reconfigure_metric_record_position(rec, alg, min, in, sqn_bit_size, reset) \
  _reconfigure_metric_record_position(__FUNCTION__, rec, alg, min, in, sqn_bit_size, reset)

IDM_T update_metric_record(struct orig_node *on, struct metric_record *rec, struct host_metricalgo *alg, SQN_T range, SQN_T min, SQN_T in, UMETRIC_T *probe)
{
        TRACE_FUNCTION_CALL;
        char *scenario = NULL;
        SQN_T dbg_clr = rec->clr;
        SQN_T dbg_set = rec->set;
        SQN_T i, purge = 0;

        ASSERTION(-500739, (!((~(rec->sqn_bit_mask))&(min))));
        ASSERTION(-500740, (!((~(rec->sqn_bit_mask))&(in))));
        ASSERTION(-500741, (!((~(rec->sqn_bit_mask))&(rec->clr))));
        ASSERTION(-500742, (!((~(rec->sqn_bit_mask))&(rec->set))));

        assertion(-500743, (range <= MAX_SQN_RANGE));
        assertion(-500744, (is_umetric_valid(alg->exp_offset, &rec->umetric_slow)));
        assertion(-500907, (is_umetric_valid(alg->exp_offset, &rec->umetric_fast)));
        assertion(-500908, (is_umetric_valid(alg->exp_offset, &rec->umetric_final)));

        if (((rec->sqn_bit_mask)&(rec->clr - min)) >= range) {

                if (rec->clr) {
                        dbgf(DBGL_CHANGES, DBGT_ERR, "sqn_bit_mask=0x%X clr=%d out of range, in=%d valid_min=%d defined_range=%d",
                                rec->sqn_bit_mask, rec->clr, in, min, range);
                }

                reconfigure_metric_record_position(rec, alg, min, in, 0, YES/*reset_metric*/);
        }

        if (probe && !is_umetric_valid(alg->exp_offset, probe)) {

                scenario = "probe contains illegal value";
                goto update_metric_error;

        } else if (((rec->sqn_bit_mask)&(in - min)) >= range) {

                scenario = "in out of defined_range (sector J)";
                goto update_metric_error;

        } else if (((rec->sqn_bit_mask)&(rec->clr - min)) >= range) {

                scenario = "clr out of defined_range (sector J)";
                goto update_metric_error;

        } else if (((rec->sqn_bit_mask)&(rec->clr - rec->set)) > 1) {

                scenario = "set invalid (compared to clr)";
                goto update_metric_error;
        }


        if (UXX_LE(rec->sqn_bit_mask, in, (rec->clr - alg->window_size))) {

                scenario = "in within valid past (sector I|H)";
                goto update_metric_success;

        } else if (in == rec->set) {

                //scenario = "in within (closed) window, in == set (sector E)";
                goto update_metric_success;

        } else if (UXX_LE(rec->sqn_bit_mask, in, rec->set)) {

                scenario = "in within (closed) window, in < set (sector F|G)";
                goto update_metric_success;

        } else if (probe) {

                dbgf_all(DBGT_INFO,"in=%d min=%d range=%d clr=%d", in, min, range, rec->clr);
                assertion(-500708, (UXX_LE(rec->sqn_bit_mask, in, min + range)));
                assertion(-500721, (UXX_GE(rec->sqn_bit_mask, in, rec->clr)));

                purge = ((rec->sqn_bit_mask)&(in - rec->clr));

                if (purge > 2)
                        scenario = "resetting and updating: probe!=NULL, in within lounge or valid future (sector E|D|C|B|A)";

                if (alg->wavg_slow == 1 /*|| alg->flags & TYP_METRIC_FLAG_STRAIGHT*/) {

                        rec->umetric_slow = rec->umetric_fast = rec->umetric_final = *probe;

                } else {

                        if (purge < alg->window_size) {

                                for (i = 0; i < purge; i++)
                                        rec->umetric_slow -= (rec->umetric_slow / alg->wavg_slow);
                        } else {
                                reconfigure_metric_record_position(rec, alg, min, in, 0, YES/*reset_metric*/);
                        }

                        if ((rec->umetric_slow += (*probe / alg->wavg_slow)) > *probe) {
                                dbgf(DBGL_CHANGES, DBGT_WARN, "resulting path metric_slow=%ju > probe=%ju",
                                        rec->umetric_slow, *probe);
                                rec->umetric_slow = *probe;
                        }

                        if (alg->wavg_slow > alg->wavg_fast) {

                                if (purge < alg->window_size) {

                                        for (i = 0; i < purge; i++)
                                                rec->umetric_fast -= (rec->umetric_fast / alg->wavg_fast);
                                }

                                if ((rec->umetric_fast += (*probe / alg->wavg_fast)) > *probe) {
                                        dbgf(DBGL_CHANGES, DBGT_WARN, "resulting path metric_fast=%ju > probe=%ju",
                                                rec->umetric_fast, *probe);
                                        rec->umetric_fast = *probe;
                                }

                                if (rec->umetric_slow * (alg->wavg_correction + 1) > rec->umetric_fast) {

                                        rec->umetric_final =
                                                (rec->umetric_slow + (rec->umetric_slow / alg->wavg_correction)) -
                                                (rec->umetric_fast / alg->wavg_correction);
                                } else {
                                        rec->umetric_final = 0;
                                }

                                if (rec->umetric_final > *probe) {
                                        dbgf(DBGL_CHANGES, DBGT_WARN, "resulting path metric_final=%ju > probe=%ju",
                                                rec->umetric_final, *probe);
                                        rec->umetric_final = *probe;
                                }

                        } else {
                                rec->umetric_final = rec->umetric_fast = rec->umetric_slow;
                        }
                }

                rec->clr = rec->set = in;


        } else if (UXX_LE(rec->sqn_bit_mask, in, rec->clr + alg->lounge_size)) {

                //scenario = "ignoring: probe==NULL, in within lounge (sector E,D,C)";

        } else {

                assertion(-500709, (UXX_LE(rec->sqn_bit_mask, in, min + range)));
//[20609  1347921] ERROR metric_record_init: reset_metric=0 sqn_bit_size=0 sqn_bit_mask=0xFF clr=186 to ((in=188) - (lounge=1))=187

                purge = ((rec->sqn_bit_mask)&((in - alg->lounge_size) - rec->clr));

                if (purge > 2)
                        scenario = "purging: probe==NULL, in within valid future (sector B|A)";

                assertion(-500710, (purge > 0));

                if (purge >= alg->window_size) {

                        reconfigure_metric_record_position(rec, alg, min, in, 0, YES/*reset_metric*/);

                } else {

                        for (i = 0; i < purge; i++)
                                rec->umetric_slow -= (rec->umetric_slow / alg->wavg_slow);

                        if (alg->wavg_slow > alg->wavg_fast) {

                                for (i = 0; i < purge; i++)
                                        rec->umetric_fast -= (rec->umetric_fast / alg->wavg_fast);

                                if (rec->umetric_slow * (alg->wavg_correction + 1) > rec->umetric_fast) {
                                        rec->umetric_final =
                                                (rec->umetric_slow + (rec->umetric_slow / alg->wavg_correction)) -
                                                (rec->umetric_fast / alg->wavg_correction);
                                } else {
                                        rec->umetric_final = 0;
                                }

                        } else {
                                rec->umetric_final = rec->umetric_fast = rec->umetric_slow;
                        }


                        reconfigure_metric_record_position(rec, alg, min, in, 0, NO/*reset_metric*/);
                }
        }

        assertion(-500711, (is_umetric_valid(alg->exp_offset, &rec->umetric_slow)));
        assertion(-500909, (is_umetric_valid(alg->exp_offset, &rec->umetric_fast)));
        assertion(-500910, (is_umetric_valid(alg->exp_offset, &rec->umetric_final)));

        goto update_metric_success;



        IDM_T ret = FAILURE;

update_metric_success:
        ret = SUCCESS;

update_metric_error:

        if (scenario) {
                dbgf(DBGL_CHANGES, ret == FAILURE ? DBGT_ERR : DBGT_WARN,
                        "[%s] sqn_bit_mask=0x%X in=%d clr=%d>%d set=%d>%d valid_min=%d range=%d lounge=%d window=%d purge=%d probe=%ju ",
                        scenario, rec->sqn_bit_mask, in, dbg_clr, rec->clr, dbg_set, rec->set, min, range, alg->lounge_size, alg->window_size, purge, probe ? *probe : 0);
        }


        EXITERROR(-500712, (ret != FAILURE));

        if (on && &on->curr_rn->mr == rec && rec->umetric_final < on->path_metricalgo->umetric_min)
                set_ogmSqn_toBeSend_and_aggregated(on, on->ogmSqn_aggregated, on->ogmSqn_aggregated);

        return ret;
}

STATIC_FUNC
void purge_rq_metrics(void *link_node_pointer)
{
        TRACE_FUNCTION_CALL;
        struct link_node *ln = link_node_pointer;
        assertion(-500731, (link_node_pointer == avl_find_item(&link_tree, &ln->link_id)));
        struct link_dev_node *lndev = list_get_first(&ln->lndev_list);

        //assertion(-500843, (ln->neigh && ln->neigh->dhn)); this can disappear ??????????
        if (lndev && ln && lndev->key.link == ln && ln->neigh && ln->neigh->dhn ) {

                if (ln->rq_purge_interval) {

                        HELLO_FLAGS_SQN_T sqn = (HELLO_SQN_MASK & (lndev->mr[SQR_RQ].clr + 1));
                        update_link_metrics(lndev, 0, sqn, sqn, SQR_RQ, NULL);

                        if ((--ln->rq_purge_iterations) && lndev->mr[SQR_RQ].umetric_final) {
                                register_task(ln->rq_purge_interval, purge_rq_metrics, ln);

                        } else {
                                lndev->mr[SQR_RQ].umetric_slow = 0;
                                lndev->mr[SQR_RQ].umetric_fast = 0;
                                lndev->mr[SQR_RQ].umetric_final = 0;
                                lndev->umetric_link = 0;
                                ln->rq_purge_interval = 0;
                        }


                } else {

                        HELLO_FLAGS_SQN_T sqn_diff = (HELLO_SQN_MASK& (ln->rq_hello_sqn_max - ln->rq_hello_sqn_max_prev));
                        TIME_T interval = ((TIME_T) (ln->rq_time_max - ln->rq_time_max_prev));

                        if (ln->rq_hello_sqn_max && ln->rq_hello_sqn_max_prev &&
                                sqn_diff && sqn_diff < my_link_window &&
                                interval && interval < ((uint32_t)(my_link_window * MAX_TX_INTERVAL))
                                ) {

                                ln->rq_purge_iterations = my_link_window;

                                ln->rq_purge_interval = (((interval / sqn_diff) * 5) / 4);

                                register_task(ln->rq_purge_interval, purge_rq_metrics, ln);
                        }
                }

        } else {
                dbgf(DBGL_SYS, DBGT_ERR, "ln=%s disappeared! lndev=%p, ln->neigh=%p",
                        ipXAsStr(af_cfg, ln ? &ln->link_ip : &ZERO_IP), (void*)(lndev), (void*)(ln ? ln->neigh : NULL));
        }
}


STATIC_FUNC
void linkdev_create(struct link_dev_node *lndev)
{
        reconfigure_metric_record_position(&(lndev->mr[SQR_RTQ]), &(link_metric_algo[SQR_RTQ]),
                ((HELLO_SQN_MASK)& (lndev->key.dev->link_hello_sqn - link_metric_algo[SQR_RTQ].lounge_size)),
                lndev->key.dev->link_hello_sqn, HELLO_SQN_BIT_SIZE, YES/*reset_metric*/);

        reconfigure_metric_record_position(&(lndev->mr[SQR_RQ]), &(link_metric_algo[SQR_RQ]), 0,
                0, HELLO_SQN_BIT_SIZE, YES/*reset_metric*/);

        lndev->umetric_link = 0;
}


STATIC_FUNC
void remove_task_purge_rq_metrics( struct link_node *ln )
{
        remove_task(purge_rq_metrics, ln);
}





void update_link_metrics(struct link_dev_node *lndev, IID_T transmittersIID,
        HELLO_FLAGS_SQN_T sqn, HELLO_FLAGS_SQN_T valid_max, uint8_t sqr, UMETRIC_T *probe)
{
        TRACE_FUNCTION_CALL;
        struct link_node *ln = lndev->key.link;
        struct dev_node *dev = lndev->key.dev;
        struct list_node *lndev_pos;
        struct link_dev_node *this_lndev = NULL;
        HELLO_FLAGS_SQN_T valid_min = ((HELLO_SQN_MASK)&(valid_max + 1 - DEF_HELLO_SQN_RANGE));


        list_for_each(lndev_pos, &ln->lndev_list)
        {
                struct link_dev_node *lnd = list_entry(lndev_pos, struct link_dev_node, list);

                update_metric_record(NULL, &lndev->mr[sqr], &link_metric_algo[sqr], DEF_HELLO_SQN_RANGE, valid_min, sqn,
                        lnd == lndev ? probe : NULL);

                if (sqr == SQR_RTQ) {
                        
                        // this hits if node is blocked
                        //assertion(-500921, (IMPLIES(ln->neigh, ln->neigh->dhn && ln->neigh->dhn->on && ln->neigh->dhn->on->path_metricalgo)));

                        if (ln->neigh && ln->neigh->dhn && ln->neigh->dhn->on && !ln->neigh->dhn->on->blocked) {

                                assertion(-500921, (ln->neigh->dhn->on->path_metricalgo));

                                UMETRIC_T max = umetric_max(ln->neigh->dhn->on->path_metricalgo->exp_offset);
                                apply_metric_algo(&lnd->umetric_link, lnd, &max ,ln->neigh->dhn->on->path_metricalgo);
                        } else {
                                lnd->umetric_link = 0;
                        }

                }


                if (lnd->key.dev == dev) {
                        assertion(-500912,(lnd == lndev));
                        this_lndev = lnd;
                        continue;
                }
                assertion(-500913, (lnd != lndev));

                //update_metric_record(NULL, &lnd->mr[sqr], &link_metric_algo[sqr], DEF_HELLO_SQN_RANGE, valid_min, sqn, NULL);
        }

        assertion(-500732, (lndev == this_lndev));

        //update_metric_record(NULL, &lndev->mr[sqr], &link_metric_algo[sqr], DEF_HELLO_SQN_RANGE, valid_min, sqn, probe);


        if (probe) {

                if (sqr == SQR_RTQ) {

                        lndev->rtq_time_max = bmx_time;

                        if (
                                ln->neigh &&
                                (!ln->neigh->best_rtq ||
                                ln->neigh->best_rtq->mr[SQR_RTQ].umetric_final <= lndev->mr[SQR_RTQ].umetric_final)) {

                                ln->neigh->best_rtq = lndev;
                        }

                } else {
                        assertion(-500733, (sqr == SQR_RQ));

                        ln->rq_time_max_prev = ln->rq_time_max;
                        ln->rq_time_max = bmx_time;

                        ln->rq_hello_sqn_max_prev = ln->rq_hello_sqn_max;
                        ln->rq_hello_sqn_max = sqn;

                        ln->rq_purge_interval = 0;

                        remove_task(purge_rq_metrics, ln);

                        if (ln->neigh && ln->neigh->dhn &&
                                ln->neigh->dhn == iid_get_node_by_neighIID4x(ln->neigh, transmittersIID)) {

                                purge_rq_metrics(ln);
                        }
                }
        }

        dbgf_all(DBGT_INFO, "%s dev %s metric %ju", ipXAsStr(af_cfg, &ln->link_ip), dev->label_cfg.str, lndev->mr[sqr].umetric_final);
}


IDM_T update_path_metrics(struct packet_buff *pb, struct orig_node *on, OGM_SQN_T in_sqn, UMETRIC_T *in_umetric)
{
        TRACE_FUNCTION_CALL;
        assertion(-500876, (!on->blocked));
        assertion(-500734, (on->path_metricalgo));
        ASSERTION(-500735, (!((~(OGM_SQN_MASK))&(in_sqn))));
        ASSERTION(-500736, (!((~(OGM_SQN_MASK))&(on->ogmSqn_maxRcvd))));


        struct host_metricalgo *algo = on->path_metricalgo;
        struct router_node *in_rn = NULL;
        OGM_SQN_T ogm_sqn_max_rcvd = UXX_GET_MAX(OGM_SQN_MASK, on->ogmSqn_maxRcvd, in_sqn);


        dbgf_all(DBGT_INFO, "%s orig_sqn %d via neigh %s", on->id.name, in_sqn, pb->i.llip_str);

        if (!is_ip_set(&on->primary_ip))
                return FAILURE;

        if ((((OGM_SQN_MASK)&(in_sqn - on->ogmSqn_rangeMin)) >= on->ogmSqn_rangeSize))
                return FAILURE;

        if (UXX_LT(OGM_SQN_MASK, in_sqn, (OGM_SQN_MASK & (ogm_sqn_max_rcvd - algo->lounge_size)))) {

                dbgf(DBGL_SYS, DBGT_WARN, "dropping late ogm_sqn=%d (max_rcvd=%d lounge_size=%d) from orig=%s via neigh=%s",
                        in_sqn, ogm_sqn_max_rcvd, algo->lounge_size, on->id.name, pb->i.llip_str);

                return SUCCESS;
        }

        UMETRIC_T upd_metric;
        apply_metric_algo(&upd_metric, pb->i.lndev, in_umetric, algo);


        if (UXX_LE(OGM_SQN_MASK, in_sqn, on->ogmSqn_toBeSend) &&
                upd_metric <= on->metricSqnMaxArr[in_sqn % (algo->lounge_size + 1)]) {

                if ((in_rn = avl_find_item(&on->rt_tree, &pb->i.lndev->key))) {

                        upd_metric = 0;
                        update_metric_record(on, &in_rn->mr, algo, on->ogmSqn_rangeSize, on->ogmSqn_rangeMin, in_sqn, &upd_metric);

                        if (in_rn == on->best_rn)
                                on->best_rn = NULL;

                }

                return SUCCESS;

        } else {

                OGM_SQN_T i = in_sqn;
                while (UXX_GE(OGM_SQN_MASK, i, (ogm_sqn_max_rcvd - algo->lounge_size))) {

                        if ((UXX_GT(OGM_SQN_MASK, i, on->ogmSqn_maxRcvd)) ||
                                (on->metricSqnMaxArr[i % (algo->lounge_size + 1)] < upd_metric)) {

                                on->metricSqnMaxArr[i % (algo->lounge_size + 1)] = upd_metric;
                                i = (OGM_SQN_MASK & (i - 1));

                        } else {

                                break;
                        }
                }
        }


        if (!on->best_rn || UXX_LT(OGM_SQN_MASK, on->ogmSqn_maxRcvd, in_sqn)) {

                struct router_node *rn;
                struct avl_node *an;
                on->best_rn = NULL;

                for (an = NULL; (rn = avl_iterate_item(&on->rt_tree, &an));) {

                        ASSERTION(-500580, (avl_find(&link_dev_tree, &rn->key)));

                        if (!in_rn && equal_link_key(&rn->key, &pb->i.lndev->key)) {

                                in_rn = rn;

                        } else {

                                update_metric_record(on, &rn->mr, algo, on->ogmSqn_rangeSize, on->ogmSqn_rangeMin, ogm_sqn_max_rcvd, NULL);

                                if (!on->best_rn || on->best_rn->mr.umetric_final < rn->mr.umetric_final)
                                        on->best_rn = rn;
                        }
                }


        } else {
                // already cleaned up, simple keep last best_router_node:
                in_rn = avl_find_item(&on->rt_tree, &pb->i.lndev->key);
        }

        if (!in_rn) {

                in_rn = debugMalloc(sizeof (struct router_node), -300222);
                memset( in_rn, 0, sizeof(struct router_node));

                in_rn->key = pb->i.lndev->key;

                reconfigure_metric_record_position(&in_rn->mr, algo, on->ogmSqn_rangeMin, ogm_sqn_max_rcvd, OGM_SQN_BIT_SIZE, YES/*reset_metric*/);

                avl_insert(&on->rt_tree, in_rn, -300223);

                dbgf(DBGL_CHANGES, DBGT_INFO, "new router_node %s to %s (total %d)",
                        ipXAsStr(af_cfg, &in_rn->key.link->link_ip), on->id.name, on->rt_tree.items);
        }

#ifndef NO_DEBUG_ALL
        struct router_node *old_rn = on->curr_rn;
        OGM_SQN_T old_set = in_rn->mr.set;
        UMETRIC_T old_metric =  in_rn->mr.umetric_final;
        OGM_SQN_T old_ogm_sqn_to_be_send = on->ogmSqn_toBeSend;
        UMETRIC_T old_curr_metric = old_rn ? old_rn->mr.umetric_final : 0;
#endif

        update_metric_record(on, &in_rn->mr, algo, on->ogmSqn_rangeSize, on->ogmSqn_rangeMin, in_sqn, &upd_metric);


        if (UXX_GT(OGM_SQN_MASK, ogm_sqn_max_rcvd, in_sqn))
                update_metric_record(on, &in_rn->mr, algo, on->ogmSqn_rangeSize, on->ogmSqn_rangeMin, ogm_sqn_max_rcvd, NULL);

        // the best RN has a better metric and betterOrEqual sqn than all other RNs

        if (!on->best_rn || on->best_rn->mr.umetric_final <= in_rn->mr.umetric_final)
                on->best_rn = in_rn;

        if (on->curr_rn && on->best_rn->mr.umetric_final <= on->curr_rn->mr.umetric_final)
                on->best_rn = on->curr_rn;

        if (UXX_GT(OGM_SQN_MASK, on->best_rn->mr.set, on->ogmSqn_toBeSend) ) {

                if (on->best_rn->mr.umetric_final >= algo->umetric_min) {

                        set_ogmSqn_toBeSend_and_aggregated(on, on->best_rn->mr.set, on->ogmSqn_aggregated);

                        if (on->best_rn != on->curr_rn ) {

                                ASSERTION(-500695, (!(on->curr_rn && !avl_find_item(&link_dev_tree, &on->curr_rn->key))));

                                dbgf(DBGL_CHANGES, DBGT_INFO,
                                        "changing router to %s %s via %s %s metric=%ju (prev %s %s metric=%ju sqn_max=%d sqn_in=%d)",
                                        on->id.name, on->primary_ip_str, ipXAsStr(af_cfg, &on->best_rn->key.link->link_ip),
                                        on->best_rn->key.dev->label_cfg.str, on->best_rn->mr.umetric_final,
                                        ipXAsStr(af_cfg, on->curr_rn ? &on->curr_rn->key.link->link_ip : &ZERO_IP),
                                        on->curr_rn ? on->curr_rn->key.dev->label_cfg.str : "---",
                                        on->curr_rn ? on->curr_rn->mr.umetric_final : 0,
                                        ogm_sqn_max_rcvd, in_sqn);

                                if (on->curr_rn)
                                        cb_route_change_hooks(DEL, on);

                                on->curr_rn = on->best_rn;

                                cb_route_change_hooks(ADD, on);
                        }

                } else {

                        if (on->curr_rn)
                                cb_route_change_hooks(DEL, on);

                        on->curr_rn = NULL;

                }
        }

#ifndef NO_DEBUG_ALL
        dbgf(DBGL_ALL, ((in_sqn != ((in_rn->mr.sqn_bit_mask)&(old_set + 1))) ? DBGT_ERR : DBGT_INFO),
                "orig=%s self=%s dev=%s  oldInSet=%-5d  oldInMax=%-5d  old_to_send=%-5d   oldInMetric=%-10ju "
                "  inSqn=%-5d  newInMax=%-5d  new_to_send=%-5d  metricAlgo( inMetric=%-10ju )=%-10ju"
                "inVia=%s newInSet=%-5d newInMetric=%-10ju   "
                "oldRt=%s oldRtSet=%-5d oldRtMetric=%-10ju  newRT=%s newRtSet=%-5d newRtMetric=%-10ju  bestRt=%s bestRtSet=%-5d bestRtMetric=%-10ju  %s",
                on->id.name, self.id.name, pb->i.iif->label_cfg.str, old_set, on->ogmSqn_maxRcvd, old_ogm_sqn_to_be_send, old_metric,
                in_sqn, ogm_sqn_max_rcvd, on->ogmSqn_toBeSend, *in_umetric, upd_metric,
                pb->i.llip_str, in_rn->mr.set, in_rn->mr.umetric_final,
                ipXAsStr(af_cfg, old_rn ? &old_rn->key.link->link_ip : &ZERO_IP),
                old_rn ? old_rn->mr.set : 0, old_curr_metric,
                ipXAsStr(af_cfg, on->curr_rn ? &on->curr_rn->key.link->link_ip : &ZERO_IP), 
                on->curr_rn ? on->curr_rn->mr.set : 0, on->curr_rn ? on->curr_rn->mr.umetric_final : 0,
                ipXAsStr(af_cfg, &on->best_rn->key.link->link_ip), on->best_rn ? on->best_rn->mr.set : 0, 
                on->best_rn->mr.umetric_final,
                UXX_GT(OGM_SQN_MASK, on->ogmSqn_toBeSend, old_ogm_sqn_to_be_send) ? "TO_BE_SEND" : " ");
#endif
        on->ogmSqn_maxRcvd = ogm_sqn_max_rcvd;

        return SUCCESS;
}




STATIC_FUNC
IDM_T validate_metricalgo(struct host_metricalgo *ma, struct ctrl_node *cn)
{
        TRACE_FUNCTION_CALL;

        if (
                validate_param((ma->exp_offset), MIN_FMETRIC_EXP_OFFSET, MAX_FMETRIC_EXP_OFFSET,ARG_FMETRIC_EXP_OFFSET) ||
                validate_param((ma->fmetric_mantissa_min), MIN_FMETRIC_MANTISSA_MIN, MAX_FMETRIC_MANTISSA_MIN, ARG_FMETRIC_MANTISSA_MIN) ||
                validate_param((ma->fmetric_exp_reduced_min), MIN_FMETRIC_EXPONENT_MIN, MAX_FMETRIC_EXPONENT_MIN, ARG_FMETRIC_EXPONENT_MIN) ||
                validate_param((ma->algo_type), MIN_METRIC_ALGO, MAX_METRIC_ALGO, ARG_PATH_METRIC_ALGO) ||
                validate_param((ma->flags),      MIN_METRIC_FLAGS, MAX_METRIC_FLAGS, ARG_PATH_METRIC_FLAGS) ||
                validate_param((ma->window_size), MIN_PATH_WINDOW, MAX_PATH_WINDOW, ARG_PATH_WINDOW) ||
                validate_param((ma->lounge_size), MIN_PATH_LOUNGE, MAX_PATH_LOUNGE, ARG_PATH_LOUNGE) ||
                validate_param((ma->wavg_slow), MIN_PATH_REGRESSION_SLOW, MAX_PATH_REGRESSION_SLOW, ARG_PATH_REGRESSION_SLOW) ||
                validate_param((ma->wavg_fast), MIN_PATH_REGRESSION_FAST, MAX_PATH_REGRESSION_FAST, ARG_PATH_REGRESSION_FAST) ||
                validate_param((ma->wavg_correction), MIN_PATH_REGRESSION_DIFF, MAX_PATH_REGRESSION_DIFF, ARG_PATH_REGRESSION_DIFF) ||
                validate_param((ma->hystere), MIN_PATH_HYST, MAX_PATH_HYST, ARG_PATH_HYST) ||
                validate_param((ma->hop_penalty), MIN_HOP_PENALTY, MAX_HOP_PENALTY, ARG_HOP_PENALTY) ||
                validate_param((ma->late_penalty), MIN_LATE_PENAL, MAX_LATE_PENAL, ARG_LATE_PENAL) ||
                0) {

                EXITERROR(-500755, (0));

                return FAILURE;
        }


        return SUCCESS;
}


STATIC_FUNC
IDM_T metricalgo_tlv_to_host(struct description_tlv_metricalgo *tlv_algo, struct host_metricalgo *host_algo, uint16_t size)
{
        TRACE_FUNCTION_CALL;
        memset(host_algo, 0, sizeof (struct host_metricalgo));

        if (size < sizeof (struct mandatory_tlv_metricalgo))
                return FAILURE;

	host_algo->exp_offset = tlv_algo->m.exp_offset;
        host_algo->fmetric_mantissa_min = tlv_algo->m.fmetric_mantissa_min;
        host_algo->fmetric_exp_reduced_min = tlv_algo->m.fmetric_exp_reduced_min;
	host_algo->algo_type = ntohs(tlv_algo->m.algo_type);
        host_algo->flags = ntohs(tlv_algo->m.flags);
        host_algo->window_size = tlv_algo->m.path_window_size;
        host_algo->lounge_size = tlv_algo->m.path_lounge_size;
        host_algo->wavg_slow = tlv_algo->m.wavg_slow;
        host_algo->wavg_fast = tlv_algo->m.wavg_fast;
        host_algo->wavg_correction = tlv_algo->m.wavg_correction;
        host_algo->hystere = tlv_algo->m.hystere;
	host_algo->hop_penalty = tlv_algo->m.hop_penalty;
	host_algo->late_penalty = tlv_algo->m.late_penalty;

        if (validate_metricalgo(host_algo, NULL) == FAILURE)
                return FAILURE;


        host_algo->umetric_min = MAX(
                umetric(host_algo->fmetric_mantissa_min, host_algo->fmetric_exp_reduced_min, host_algo->exp_offset),
                umetric(FMETRIC_MANTISSA_ROUTABLE, 0, host_algo->exp_offset));

        return SUCCESS;
}


STATIC_FUNC
int create_description_tlv_metricalgo(struct tx_frame_iterator *it)
{
        TRACE_FUNCTION_CALL;
        struct description_tlv_metricalgo tlv_algo;

        dbgf(DBGL_CHANGES, DBGT_INFO, " size %zu", sizeof (struct description_tlv_metricalgo));

        memset(&tlv_algo, 0, sizeof (struct description_tlv_metricalgo));

        tlv_algo.m.exp_offset = DEF_FMETRIC_EXP_OFFSET;
        tlv_algo.m.fmetric_mantissa_min = DEF_FMETRIC_MANTISSA_MIN;
        tlv_algo.m.fmetric_exp_reduced_min = my_fmetric_exp_min;
        tlv_algo.m.algo_type = htons(my_path_metricalgo); //METRIC_ALGO
        tlv_algo.m.flags = htons(my_path_metric_flags);
        tlv_algo.m.path_window_size = my_path_window;
        tlv_algo.m.path_lounge_size = my_path_lounge;
        tlv_algo.m.wavg_slow = my_path_regression_slow;
        tlv_algo.m.wavg_fast = my_path_regression_fast;
        tlv_algo.m.wavg_correction = my_path_regression_diff;
        tlv_algo.m.hystere = my_path_hystere;
        tlv_algo.m.hop_penalty = my_hop_penalty;
        tlv_algo.m.late_penalty = my_late_penalty;

        if (self.path_metricalgo)
                debugFree(self.path_metricalgo, -300282);

        self.path_metricalgo = debugMalloc(sizeof ( struct host_metricalgo), -300283);


        if (metricalgo_tlv_to_host(&tlv_algo, self.path_metricalgo, sizeof (struct description_tlv_metricalgo)) == FAILURE)
                cleanup_all(-500844);

        if (tx_iterator_cache_data_space(it) < ((int) sizeof (struct description_tlv_metricalgo))) {

                dbgf(DBGL_SYS, DBGT_ERR, "unable to announce metric due to limiting --%s", ARG_UDPD_SIZE);
                return TLV_DATA_FAILURE;
        }

        memcpy(((struct description_tlv_metricalgo *) tx_iterator_cache_msg_ptr(it)), &tlv_algo,
                sizeof (struct description_tlv_metricalgo));

        return sizeof (struct description_tlv_metricalgo);
}


STATIC_FUNC
void dbg_metrics_status(struct ctrl_node *cn)
{
        TRACE_FUNCTION_CALL;

        dbg_printf(cn, "%s=%d %s=%d %s=%d %s=%d %s=%d\n",
                ARG_LINK_REGRESSION_SLOW, link_metric_algo[SQR_RTQ].wavg_slow,
                ARG_LINK_REGRESSION_FAST, link_metric_algo[SQR_RTQ].wavg_fast,
                ARG_LINK_REGRESSION_DIFF, link_metric_algo[SQR_RTQ].wavg_correction,
                ARG_LINK_RTQ_LOUNGE, link_metric_algo[SQR_RTQ].lounge_size,
                ARG_LINK_WINDOW, link_metric_algo[SQR_RTQ].window_size);

        process_description_tlvs(NULL, &self, self.desc, TLV_OP_DEBUG, BMX_DSC_TLV_METRIC, cn);

}

STATIC_FUNC
void dbg_metricalgo(struct ctrl_node *cn, struct host_metricalgo *host_algo)
{
        TRACE_FUNCTION_CALL;

        dbg_printf(cn, "%s=%d %s=%d %s=%d %s=%ju %s=%d %s=0x%X %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d %s=%d\n",
                ARG_FMETRIC_EXP_OFFSET, host_algo->exp_offset,
                ARG_FMETRIC_MANTISSA_MIN, host_algo->fmetric_mantissa_min,
                ARG_FMETRIC_EXPONENT_MIN, host_algo->fmetric_exp_reduced_min,
                "umetric_min", host_algo->umetric_min,
                ARG_PATH_METRIC_ALGO, host_algo->algo_type,
                ARG_PATH_METRIC_FLAGS, host_algo->flags,

                ARG_PATH_REGRESSION_SLOW, host_algo->wavg_slow,
                ARG_PATH_REGRESSION_FAST, host_algo->wavg_fast,
                ARG_PATH_REGRESSION_DIFF, host_algo->wavg_correction,
                ARG_PATH_LOUNGE, host_algo->lounge_size,
                ARG_PATH_WINDOW, host_algo->window_size,
                ARG_PATH_HYST, host_algo->hystere,
                ARG_HOP_PENALTY, host_algo->hop_penalty,
                ARG_LATE_PENAL, host_algo->late_penalty);
}

STATIC_FUNC
int process_description_tlv_metricalgo(struct rx_frame_iterator *it )
{
        TRACE_FUNCTION_CALL;
        assertion(-500683, (it->frame_type == BMX_DSC_TLV_METRIC));
        assertion(-500684, (it->on));

        struct orig_node *on = it->on;
        IDM_T op = it->op;

        struct description_tlv_metricalgo *tlv_algo = (struct description_tlv_metricalgo *) (it->frame_data);
        struct host_metricalgo host_algo;

        dbgf_all( DBGT_INFO, "%s ", tlv_op_str[op]);

        if (op == TLV_OP_DEL) {

                if (on->path_metricalgo) {
                        debugFree(on->path_metricalgo, -300285);
                        on->path_metricalgo = NULL;
                }

                if (on->metricSqnMaxArr) {
                        debugFree(on->metricSqnMaxArr, -300307);
                        on->metricSqnMaxArr = NULL;
                }


        } else if (!(op == TLV_OP_TEST || op == TLV_OP_ADD || op == TLV_OP_DEBUG)) {

                return it->frame_data_length;
        }

        if (metricalgo_tlv_to_host(tlv_algo, &host_algo, it->frame_data_length) == FAILURE)
                return FAILURE;


        if (op == TLV_OP_ADD) {

                assertion(-500684, (!on->path_metricalgo));

                on->path_metricalgo = debugMalloc(sizeof (struct host_metricalgo), -300286);

                memcpy(on->path_metricalgo, &host_algo, sizeof (struct host_metricalgo));

                on->metricSqnMaxArr = debugMalloc(((on->path_metricalgo->lounge_size + 1) * sizeof (UMETRIC_T)), -300308);
                memset(on->metricSqnMaxArr, 0, ((on->path_metricalgo->lounge_size + 1) * sizeof (UMETRIC_T)));

                // migrate current router_nodes->mr.clr position to new sqn_range:
                struct router_node *rn;
                struct avl_node *an;

                for (an = NULL; (rn = avl_iterate_item(&on->rt_tree, &an));) {

                        reconfigure_metric_record_position(&rn->mr, on->path_metricalgo, on->ogmSqn_rangeMin, on->ogmSqn_rangeMin, 0, NO);

/*
                        OGM_SQN_T in = ((OGM_SQN_MASK)&(on->ogmSqn_rangeMin + on->path_metricalgo->lounge_size));
                        reconfigure_metric_record(&rn->mr, on->path_metricalgo, on->ogmSqn_rangeMin, in, 0, NO);
*/

                }


        } else if (op == TLV_OP_DEBUG) {

                dbg_metricalgo(it->cn, &host_algo);

        }

        return it->frame_data_length;
}







STATIC_FUNC
int32_t opt_link_metricalgo(uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn)
{
        TRACE_FUNCTION_CALL;

        if (cmd == OPT_CHECK || cmd == OPT_APPLY || cmd == OPT_REGISTER) {

                struct host_metricalgo test_algo[SQR_RANGE];

                memset(test_algo, 0, sizeof (test_algo));

                uint8_t r;
                for (r = 0; r < SQR_RANGE; r++) {
                        //test_algo[r].exp_offset = 0;
                        //test_algo[r].fmetric_to_live = 0;
                        //test_algo[r].algo_type = 0;

                        test_algo[r].flags = (cmd != OPT_REGISTER && !strcmp(opt->long_name, ARG_LINK_METRIC_FLAGS)) ?
                                strtol(patch->p_val, NULL, 10) :my_link_metric_flags;

                        test_algo[r].window_size = (cmd != OPT_REGISTER && !strcmp(opt->long_name, ARG_LINK_WINDOW)) ?
                                strtol(patch->p_val, NULL, 10) : my_link_window;

                        if ( r == SQR_RTQ)
                                test_algo[SQR_RTQ].lounge_size = (cmd != OPT_REGISTER && !strcmp(opt->long_name, ARG_LINK_RTQ_LOUNGE)) ?
                                strtol(patch->p_val, NULL, 10) : my_link_rtq_lounge;
                        else
                                test_algo[SQR_RQ].lounge_size = RQ_LINK_LOUNGE;

                        test_algo[r].wavg_slow = (cmd != OPT_REGISTER && !strcmp(opt->long_name, ARG_LINK_REGRESSION_SLOW))
                                ? strtol(patch->p_val, NULL, 10) : my_link_regression_slow;

                        test_algo[r].wavg_fast = (cmd != OPT_REGISTER && !strcmp(opt->long_name, ARG_LINK_REGRESSION_FAST))
                                ? strtol(patch->p_val, NULL, 10) : my_link_regression_fast;

                        test_algo[r].wavg_correction = (cmd != OPT_REGISTER && !strcmp(opt->long_name, ARG_LINK_REGRESSION_DIFF))
                                ? strtol(patch->p_val, NULL, 10) : my_link_regression_diff;

                        //test_algo[r].hystere = 0;
                        //test_algo[r].hop_penalty = 0;
                        //test_algo[r].late_penalty = 0;

                        if (validate_metricalgo(&(test_algo[r]), cn) == FAILURE)
                                return FAILURE;
                }


                if (cmd == OPT_APPLY || cmd == OPT_REGISTER) {
                        
                        memcpy( &link_metric_algo, &test_algo, sizeof(test_algo));

//                        my_description_changed = YES;
                }

        }

        return SUCCESS;
}



STATIC_FUNC
int32_t opt_path_metricalgo(uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn)
{
        TRACE_FUNCTION_CALL;

        if (cmd == OPT_REGISTER || cmd == OPT_CHECK || cmd == OPT_APPLY) {

                struct host_metricalgo test_algo;
                memset(&test_algo, 0, sizeof (struct host_metricalgo));

                // only options with a non-zero MIN value and those with illegal compinations must be tested
                // other illegal option configurations will be cached by their MIN_... MAX_.. control.c architecture

                //test_algo.algo_type = (cmd != OPT_REGISTER && !strcmp(opt->long_name, ARG_PATH_METRIC_ALGO)) ?
                //        strtol(patch->p_val, NULL, 10) : my_path_metricalgo;

                //test_algo.flags = (cmd != OPT_REGISTER && !strcmp(opt->long_name, ARG_PATH_METRIC_FLAGS)) ?
                //        strtol(patch->p_val, NULL, 10) : my_path_metric_flags;

                test_algo.window_size = (cmd != OPT_REGISTER && !strcmp(opt->long_name, ARG_PATH_WINDOW)) ?
                        strtol(patch->p_val, NULL, 10) : my_path_window;

                test_algo.wavg_slow = (cmd != OPT_REGISTER && !strcmp(opt->long_name, ARG_PATH_REGRESSION_SLOW))
                        ? strtol(patch->p_val, NULL, 10) : my_path_regression_slow;

                test_algo.wavg_fast = (cmd != OPT_REGISTER && !strcmp(opt->long_name, ARG_PATH_REGRESSION_FAST))
                        ? strtol(patch->p_val, NULL, 10) : my_path_regression_fast;

                test_algo.wavg_correction = (cmd != OPT_REGISTER && !strcmp(opt->long_name, ARG_PATH_REGRESSION_DIFF))
                        ? strtol(patch->p_val, NULL, 10) : my_path_regression_diff;


                if (validate_metricalgo(&test_algo, cn) == FAILURE)
                        return FAILURE;

        }

        if (cmd == OPT_APPLY) {

                opt_link_metricalgo(cmd, _save, opt, patch, cn);

                my_description_changed = YES;
        }

	return SUCCESS;
}


#ifdef WITH_UNUSED
int32_t test_value = 0;

STATIC_FUNC
int32_t opt_test(uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn)
{

        if (cmd != OPT_APPLY)
                return SUCCESS;

        // test the bit-shift-uint-casting functions:


        int i;
        uint32_t sqn_u32=50, in_u32=50, dad_u32=100;
        uint16_t sqn_u16=50, in_u16=50, dad_u16=100;
        uint8_t is_u32, is_u16;

        for (i = -2 * U16_MAX; i <= 2 * U16_MAX; i++) {
                //in_u32 = in_u16 = i;
                //sqn_u32 = sqn_u16 = i;
                dad_u32 = dad_u16 = i;
                if ((is_u16 = (((uint16_t) (sqn_u16 - in_u16)) < dad_u16)) !=
                        (is_u32 = (((U32_MAX >> 16)&(sqn_u32 - in_u32)) < dad_u32))) {

                        dbgf_cn(cn, DBGL_SYS, DBGT_ERR, "testing bit-shift-uint-casting i=%d failed", i);

                }
        }

        // test the fmetric_to_umetric() and umetric_to_fmetric() converter functions:

        float error_max = 0;

        UMETRIC_T in_max = umetric_max(DEF_FMETRIC_EXP_OFFSET); // (in_max_a * in_max_b);//((uint64_t) - 1); //(in_max_a * in_max_b);

        UMETRIC_T in_min = 0;//1 << DEF_EXPONENT_OFFSET; //0; // 1 << DEF_EXPONENT_OFFSET;

        UMETRIC_T in = in_min;


        while(1) {


                FMETRIC_T fm = umetric_to_fmetric(DEF_FMETRIC_EXP_OFFSET, in);

                UMETRIC_T out = fmetric_to_umetric(fm);
                float error = (((float) in) - ((float) out)) / ((float) MAX(in,1));

                dbgf_cn(cn, DBGL_SYS, DBGT_INFO,
                        "in: %20ju (mant=%-2u exp_total=%-2u)  out: %20ju %4ju %.2e  error: %8.3f %c",
                        in, fm.val.f.mantissa, fm.val.f.exp_total, out,
                        ((out << 10) / umetric_max(DEF_FMETRIC_EXP_OFFSET)), ((float) out), error * 100, '%');

                error = error < 0 ? -error : error;
                error_max = MAX(error, error_max);


                UMETRIC_T add = ((in * (UMETRIC_T)test_value) / 1024);

                add = (add ? add : 1);

                if (in >= in_max)
                        break;

                if ((in_max - add) > in)
                        in += add;

                else
                        in = in_max;


        }


        dbgf_cn(cn, DBGL_SYS, DBGT_INFO, "in_min=%ju   in_max=%ju %ju/1000 %.3e  steps=%d  error_max=%.3f %c",
                in_min, in_max,
                ((in_max << 10) / umetric_max(DEF_FMETRIC_EXP_OFFSET)),
                ((float) in_max), test_value, 100 * error_max, '%');

        cleanup_all(CLEANUP_SUCCESS);

        return SUCCESS;
}
#endif

STATIC_FUNC
struct opt_type metrics_options[]=
{
//       ord parent long_name             shrt Attributes                            *ival              min                 max                default              *func,*syntax,*help

	{ODI, 0,0,                         0,  0,0,0,0,0,0,                          0,                 0,                  0,                 0,                   0,
			0,		"\nMetric options:"}
,
#ifdef WITH_UNUSED
        {ODI, 0, "test_metric", 0, 0, A_PS1, A_USR, A_INI, A_ARG, A_ANY, &test_value, 1, (((uint32_t)-1)>>1), 0, opt_test,
                ARG_VALUE_FORM, "test metric stuff like floating-point function"}
,
	{ODI, 0, ARG_PATH_HYST,   	   0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&my_path_hystere,MIN_PATH_HYST,	MAX_PATH_HYST,	DEF_PATH_HYST,	opt_path_metricalgo,
			ARG_VALUE_FORM,	"use hysteresis to delay route switching to alternative next-hop neighbors with better path metric"}
        ,
        // there SHOULD! be a minimal lateness_penalty >= 1 ! Otherwise a shorter path with equal path-cost than a longer path will never dominate
	{ODI, 0, ARG_LATE_PENAL,  	   0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&my_late_penalty,MIN_LATE_PENAL,MAX_LATE_PENAL, DEF_LATE_PENAL, opt_path_metricalgo,
			ARG_VALUE_FORM,	"penalize non-first rcvd OGMs "}
        ,

#endif
#ifndef LESS_OPTIONS
        {ODI, 0, ARG_PATH_METRIC_ALGO,     0,  5, A_PS1, A_ADM, A_DYI, A_CFA, A_ANY, &my_path_metricalgo,MIN_METRIC_ALGO,    MAX_METRIC_ALGO,    DEF_METRIC_ALGO,    opt_path_metricalgo,
                ARG_VALUE_FORM, HELP_PATH_METRIC_ALGO}
        ,
        {ODI, 0, ARG_FMETRIC_EXPONENT_MIN, 0,  5, A_PS1, A_ADM, A_DYI, A_CFA, A_ANY, &my_fmetric_exp_min,MIN_FMETRIC_EXPONENT_MIN,MAX_FMETRIC_EXPONENT_MIN,DEF_FMETRIC_EXPONENT_MIN,    opt_path_metricalgo,
                ARG_VALUE_FORM, " "}
        ,
        {ODI, 0, ARG_PATH_WINDOW, 0, 5, A_PS1, A_ADM, A_DYI, A_CFA, A_ANY, &my_path_window, MIN_PATH_WINDOW, MAX_PATH_WINDOW, DEF_PATH_WINDOW, opt_path_metricalgo,
			ARG_VALUE_FORM,	"set path window size (PWS) for end2end path-quality calculation (path metric)"}
        ,
#endif
	{ODI, 0, ARG_PATH_LOUNGE,          0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&my_path_lounge, MIN_PATH_LOUNGE,MAX_PATH_LOUNGE,DEF_PATH_LOUNGE, opt_path_metricalgo,
			ARG_VALUE_FORM, "set default PLS buffer size to artificially delay my OGM processing for ordered path-quality calulation"}
        ,
	{ODI, 0, ARG_PATH_REGRESSION_SLOW,      0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&my_path_regression_slow,MIN_PATH_REGRESSION_SLOW,MAX_PATH_REGRESSION_SLOW,DEF_PATH_REGRESSION_SLOW,opt_path_metricalgo,
			ARG_VALUE_FORM,	"set (slow) path regression "}
        ,
#ifndef LESS_OPTIONS
	{ODI, 0, ARG_HOP_PENALTY,	   0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&my_hop_penalty, MIN_HOP_PENALTY, MAX_HOP_PENALTY, DEF_HOP_PENALTY, opt_path_metricalgo,
			ARG_VALUE_FORM,	"penalize non-first rcvd OGMs in 1/255 (each hop will substract metric*(VALUE/255) from current path-metric)"}
        ,
        {ODI,0,ARG_LINK_WINDOW,       	   0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&my_link_window,	MIN_LINK_WINDOW, 	MAX_LINK_WINDOW,DEF_LINK_WINDOW,    opt_link_metricalgo,
			ARG_VALUE_FORM,	"set link window size (LWS) for link-quality calculation (link metric)"}
        ,
#endif
	{ODI,0,ARG_LINK_REGRESSION_SLOW,        0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&my_link_regression_slow,	MIN_LINK_REGRESSION_SLOW,MAX_LINK_REGRESSION_SLOW,DEF_LINK_REGRESSION_SLOW,opt_link_metricalgo,
			ARG_VALUE_FORM,	"set (slow) link regression"}
        ,
#ifndef LESS_OPTIONS
	{ODI,0,ARG_LINK_REGRESSION_FAST,        0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&my_link_regression_fast,	MIN_LINK_REGRESSION_FAST,MAX_LINK_REGRESSION_FAST,DEF_LINK_REGRESSION_FAST,opt_link_metricalgo,
			ARG_VALUE_FORM,	"set (fast) link regression"}
        ,
	{ODI,0,ARG_LINK_REGRESSION_DIFF,        0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&my_link_regression_diff,	MIN_LINK_REGRESSION_DIFF,MAX_LINK_REGRESSION_DIFF,DEF_LINK_REGRESSION_DIFF,opt_link_metricalgo,
			ARG_VALUE_FORM,	"set (diff) link regression"}
        ,
	{ODI,0,ARG_LINK_RTQ_LOUNGE,  	   0,  5,A_PS1,A_ADM,A_DYI,A_CFA,A_ANY,	&my_link_rtq_lounge,MIN_LINK_RTQ_LOUNGE,MAX_LINK_RTQ_LOUNGE,DEF_LINK_RTQ_LOUNGE, opt_link_metricalgo,
			ARG_VALUE_FORM, "set local LLS buffer size to artificially delay OGM processing for ordered link-quality calulation"}
#endif

};


STATIC_FUNC
int32_t init_metrics( void )
{

        struct frame_handl metric_handl;
        memset( &metric_handl, 0, sizeof(metric_handl));
        metric_handl.fixed_msg_size = 0;
        metric_handl.is_relevant = 1;
        metric_handl.min_msg_size = sizeof (struct mandatory_tlv_metricalgo);
        metric_handl.name = "desc_tlv_metric0";
        metric_handl.tx_frame_handler = create_description_tlv_metricalgo;
        metric_handl.rx_frame_handler = process_description_tlv_metricalgo;
        register_frame_handler(description_tlv_handl, BMX_DSC_TLV_METRIC, &metric_handl);

        
        register_path_metricalgo(BIT_METRIC_ALGO_RQv0, path_metricalgo_RQv0);
        register_path_metricalgo(BIT_METRIC_ALGO_TQv0, path_metricalgo_TQv0);
        register_path_metricalgo(BIT_METRIC_ALGO_RTQv0, path_metricalgo_RTQv0);
        register_path_metricalgo(BIT_METRIC_ALGO_ETXv0, path_metricalgo_ETXv0);
        register_path_metricalgo(BIT_METRIC_ALGO_ETTv0, path_metricalgo_ETTv0);
        register_path_metricalgo(BIT_METRIC_ALGO_MINBANDWIDTH, path_metricalgo_MINBANDWIDTH);


        register_options_array(metrics_options, sizeof (metrics_options));

        return SUCCESS;
}

STATIC_FUNC
void cleanup_metrics( void )
{
        if (self.path_metricalgo) {
                debugFree(self.path_metricalgo, -300281);
                self.path_metricalgo = NULL;
        }
}





struct plugin *metrics_get_plugin( void ) {

	static struct plugin metrics_plugin;
	memset( &metrics_plugin, 0, sizeof ( struct plugin ) );

	metrics_plugin.plugin_name = "bmx6_metric_plugin";
	metrics_plugin.plugin_size = sizeof ( struct plugin );
        metrics_plugin.plugin_code_version = CODE_VERSION;
        metrics_plugin.cb_init = init_metrics;
	metrics_plugin.cb_cleanup = cleanup_metrics;
        metrics_plugin.cb_plugin_handler[PLUGIN_CB_LINKDEV_CREATE] = (void (*) (void*)) linkdev_create;
        metrics_plugin.cb_plugin_handler[PLUGIN_CB_LINK_DESTROY] = (void (*) (void*)) remove_task_purge_rq_metrics;
        metrics_plugin.cb_plugin_handler[PLUGIN_CB_STATUS] = (void (*) (void*)) dbg_metrics_status;

        return &metrics_plugin;
}
