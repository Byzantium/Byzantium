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



#define BIT_METRIC_ALGO_MIN           0x00
#define BIT_METRIC_ALGO_RQv0          0x00
#define BIT_METRIC_ALGO_TQv0          0x01
#define BIT_METRIC_ALGO_RTQv0         0x02
#define BIT_METRIC_ALGO_MINBANDWIDTH  0x03
#define BIT_METRIC_ALGO_ETXv0         0x04
#define BIT_METRIC_ALGO_ETTv0         0x05
#define BIT_METRIC_ALGO_MAX           0x05
#define BIT_METRIC_ALGO_ARRSZ         ((8*sizeof(ALGO_T)))

#define MIN_METRIC_ALGO               0x00
#define TYP_METRIC_ALGO_RQv0          (0x01 << BIT_METRIC_ALGO_RQv0)         //   1
#define TYP_METRIC_ALGO_TQv0          (0x01 << BIT_METRIC_ALGO_TQv0)         //   2
#define TYP_METRIC_ALGO_RTQv0         (0x01 << BIT_METRIC_ALGO_RTQv0)        //   4
#define TYP_METRIC_ALGO_MINBANDWIDTH  (0x01 << BIT_METRIC_ALGO_MINBANDWIDTH) //   8
#define TYP_METRIC_ALGO_ETXv0         (0x01 << BIT_METRIC_ALGO_ETXv0)        //  16
#define TYP_METRIC_ALGO_ETTv0         (0x01 << BIT_METRIC_ALGO_ETTv0)        //  32
#define MAX_METRIC_ALGO               (0x01 << BIT_METRIC_ALGO_MAX)          // 128
#define MAX_METRIC_ALGO_RESERVED      ((ALGO_T)-1);
#define DEF_METRIC_ALGO    TYP_METRIC_ALGO_ETTv0

#define ARG_PATH_METRIC_ALGO "path_metricalgo"
#define HELP_PATH_METRIC_ALGO "set metric algo for routing towards myself: 1=RQ 2=TQ 4=RTQ 8=HopPenalty 16=MinBW 32=ETX 64=ETTv0"


#define MAX_PATH_WINDOW 250      /* 250 TBD: should not be larger until ogm->ws and neigh_node.packet_count (and related variables) is only 8 bit */
#define MIN_PATH_WINDOW 1
#define DEF_PATH_WINDOW 5        /* NBRF: NeighBor Ranking sequence Frame) sliding packet range of received orginator messages in squence numbers (should be a multiple of our word size) */
#define ARG_PATH_WINDOW "path_window"
//extern int32_t my_path_window; // my path window size used to quantify the end to end path quality between me and other nodes

#define MIN_PATH_LOUNGE 0
#define MAX_PATH_LOUNGE 4
#define DEF_PATH_LOUNGE 2
#define ARG_PATH_LOUNGE "path_lounge"
//extern int32_t my_path_lounge;


#define DEF_PATH_REGRESSION_SLOW 1
#define MIN_PATH_REGRESSION_SLOW 1
#define MAX_PATH_REGRESSION_SLOW 255
#define ARG_PATH_REGRESSION_SLOW "path_regression_slow"

#define DEF_PATH_REGRESSION_FAST 1
#define MIN_PATH_REGRESSION_FAST 1
#define MAX_PATH_REGRESSION_FAST 32
#define ARG_PATH_REGRESSION_FAST "path_regression_fast"

#define DEF_PATH_REGRESSION_DIFF 8
#define MIN_PATH_REGRESSION_DIFF 2
#define MAX_PATH_REGRESSION_DIFF 255
#define ARG_PATH_REGRESSION_DIFF "path_regression_correction"

#define MAX_LINK_WINDOW 100
#define MIN_LINK_WINDOW 1
#define DEF_LINK_WINDOW 40
#define ARG_LINK_WINDOW "link_window"
//extern int32_t my_link_window; // my link window size used to quantify the link qualities to direct neighbors
#define RQ_PURGE_ITERATIONS MAX_LINK_WINDOW


#define DEF_LINK_REGRESSION_SLOW 16  // = ~ lg2( DEF_LINK_WINDOW )
#define MIN_LINK_REGRESSION_SLOW 1
#define MAX_LINK_REGRESSION_SLOW 255
#define ARG_LINK_REGRESSION_SLOW "link_regression_slow"

#define DEF_LINK_REGRESSION_FAST 2  // = ~ lg2( DEF_LINK_WINDOW )
#define MIN_LINK_REGRESSION_FAST 1
#define MAX_LINK_REGRESSION_FAST 32
#define ARG_LINK_REGRESSION_FAST "link_regression_fast"

#define DEF_LINK_REGRESSION_DIFF 8  // = ~ lg2( DEF_LINK_WINDOW )
#define MIN_LINK_REGRESSION_DIFF 1
#define MAX_LINK_REGRESSION_DIFF 255
#define ARG_LINK_REGRESSION_DIFF "link_regression_correction"

// the default link_lounge_size of 1o2 is good to compensate for ogi ~ but <= aggreg_interval
#define MIN_LINK_RTQ_LOUNGE 0
#define MAX_LINK_RTQ_LOUNGE 10
#define DEF_LINK_RTQ_LOUNGE 1 //2
#define ARG_LINK_RTQ_LOUNGE "link_lounge"
//extern int32_t my_link_rtq_lounge;

#define RQ_LINK_LOUNGE 0  /* may also be rtq_link_lounge */

// this deactivates OGM-Acks on the link:
#define MIN_LINK_IGNORE_MIN  0
#define MAX_LINK_IGNORE_MIN  100
#define DEF_LINK_IGNORE_MIN  50
#define ARG_LINK_IGNORE_MIN "link_ignore_min"
//extern int32_t link_ignore_min;

// this activates OGM-Acks on the link:
#define MIN_LINK_IGNORE_MAX  0
#define MAX_LINK_IGNORE_MAX  255
#define DEF_LINK_IGNORE_MAX  100
#define ARG_LINK_IGNORE_MAX "link_ignore_max"
//extern int32_t link_ignore_max;



#define MIN_PATH_HYST	0
#define MAX_PATH_HYST	255
#define DEF_PATH_HYST	0
#define ARG_PATH_HYST   "path_hysteresis"
//extern int32_t my_path_hystere;


#define MIN_LATE_PENAL 0
#define MAX_LATE_PENAL 100
#define DEF_LATE_PENAL 1
#define ARG_LATE_PENAL "lateness_penalty"
//extern int32_t my_late_penalty;


#define DEF_HOP_PENALTY 0 //(U8_MAX/20) <=>  5% penalty on metric per hop
#define MIN_HOP_PENALTY 0 // smaller values than 4 do not show effect
#define MAX_HOP_PENALTY U8_MAX
#define ARG_HOP_PENALTY "hop_penalty"
#define MAX_HOP_PENALTY_PRECISION_EXP 8
//extern int32_t my_hop_penalty;





#define FMETRIC_UNDEFINED    0
#define FMETRIC_MANTISSA_BLOCKED  0
#define FMETRIC_MANTISSA_ZERO     1
#define FMETRIC_MANTISSA_ROUTABLE 2


#define TMP_MANTISSA_BIT_SHIFT OGM_MANTISSA_BIT_SIZE
#define OGM_EXPONENT_MAX ((1<<OGM_EXPONENT_BIT_SIZE)-1)
#define OGM_MANTISSA_MAX ((1<<OGM_MANTISSA_BIT_SIZE)-1)


#define DEF_FMETRIC_EXP_OFFSET 0
#define MIN_FMETRIC_EXP_OFFSET 0
#define MAX_FMETRIC_EXP_OFFSET ( (8*sizeof(UMETRIC_T)) - OGM_MANTISSA_BIT_SIZE - (1<<OGM_EXPONENT_BIT_SIZE) ) // konservative?
#define ARG_FMETRIC_EXP_OFFSET "metric_exponent_offset"
//extern int32_t my_fmetric_exp_offset;

#define MIN_FMETRIC_MANTISSA_MIN (0)
#define MAX_FMETRIC_MANTISSA_MIN ((1<<(8-OGM_EXPONENT_BIT_SIZE))-1)
#define ARG_FMETRIC_MANTISSA_MIN "min_metric_mantissa"
#define DEF_FMETRIC_MANTISSA_MIN FMETRIC_MANTISSA_ROUTABLE

#define MIN_FMETRIC_EXPONENT_MIN (0)
#define MAX_FMETRIC_EXPONENT_MIN ((1<<OGM_EXPONENT_BIT_SIZE)-1)
#define ARG_FMETRIC_EXPONENT_MIN "min_metric_exponent"
#define DEF_FMETRIC_EXPONENT_MIN MIN_FMETRIC_EXPONENT_MIN



//#define TYP_METRIC_FLAG_STRAIGHT (0x1<<0)

#define MIN_METRIC_FLAGS          (0x0)
#define MAX_METRIC_FLAGS          (0x1)

#define DEF_PATH_METRIC_FLAGS     (0x0)
#define ARG_PATH_METRIC_FLAGS     "path_metric_flags"

#define DEF_LINK_METRIC_FLAGS     (0x0)
#define ARG_LINK_METRIC_FLAGS     "link_metric_flags"




struct mandatory_tlv_metricalgo {
	uint8_t exp_offset;

#if __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned int fmetric_mantissa_min : (8 - OGM_EXPONENT_BIT_SIZE);
	unsigned int fmetric_exp_reduced_min : OGM_EXPONENT_BIT_SIZE;
#elif __BYTE_ORDER == __BIG_ENDIAN
	unsigned int fmetric_exp_reduced_min : OGM_EXPONENT_BIT_SIZE;
	unsigned int fmetric_mantissa_min : (8 - OGM_EXPONENT_BIT_SIZE);
#else
# error "Please fix <bits/endian.h>"
#endif

	uint8_t reserved_for_fmetric_mantissa_size; //???? does this make sense??
	uint8_t reserved;

	ALGO_T algo_type;
	uint16_t flags;

	uint8_t path_window_size;
	uint8_t path_lounge_size;
	uint8_t wavg_slow;
	uint8_t wavg_fast;
	uint8_t wavg_correction;
	uint8_t hystere;
	uint8_t hop_penalty;
	uint8_t late_penalty;

} __attribute__((packed));


struct description_tlv_metricalgo {
	struct mandatory_tlv_metricalgo m;
	uint8_t optional[];
} __attribute__((packed));



extern struct host_metricalgo link_metric_algo[SQR_RANGE];


extern UMETRIC_T UMETRIC_RQ_NBDISC_MIN;
extern UMETRIC_T UMETRIC_RQ_OGM_ACK_MIN;


// some tools:


FMETRIC_T fmetric(uint8_t mantissa, uint8_t exp_reduced, uint8_t exp_offset);

UMETRIC_T umetric_max(uint8_t exp_offset);

UMETRIC_T umetric(uint8_t mantissa, uint8_t exp_reduced, uint8_t exp_offset);

UMETRIC_T fmetric_to_umetric(FMETRIC_T fm);
FMETRIC_T umetric_to_fmetric(uint8_t exp_offset, UMETRIC_T val);

IDM_T is_fmetric_valid(FMETRIC_T fm);

IDM_T fmetric_cmp(FMETRIC_T a, unsigned char cmp, FMETRIC_T b);


// some core hooks:
void apply_metric_algo(UMETRIC_T *out, struct link_dev_node *link, UMETRIC_T *path, struct host_metricalgo *algo);

IDM_T update_metric_record(struct orig_node *on, struct metric_record *rec, struct host_metricalgo *alg, SQN_T range, SQN_T min, SQN_T in, UMETRIC_T *probe);

void update_link_metrics(struct link_dev_node *lndev, IID_T transmittersIID, HELLO_FLAGS_SQN_T sqn, HELLO_FLAGS_SQN_T valid_max, uint8_t sqr, UMETRIC_T *probe);

IDM_T update_path_metrics(struct packet_buff *pb, struct orig_node *on, OGM_SQN_T in_sqn, UMETRIC_T *in_umetric);



// plugin hooks:

struct plugin *metrics_get_plugin( void );
