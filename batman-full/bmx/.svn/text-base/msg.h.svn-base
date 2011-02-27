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

#include <stdint.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <linux/if.h>




#define MIN_UDPD_SIZE 128 //(6+4+(22+8)+32)+184=72+56=128
#define DEF_UDPD_SIZE 512
#define MAX_UDPD_SIZE (MIN( 1400, MAX_PACKET_SIZE))
#define ARG_UDPD_SIZE "udp_data_size"



#define DEF_TX_TS_TREE_SIZE 150
#define DEF_TX_TS_TREE_PURGE_FK 3

#define DEF_DESC0_CACHE_SIZE 100
#define DEF_DESC0_CACHE_TO   100000


#define MIN_UNSOLICITED_DESC_ADVS 0
#define MAX_UNSOLICITED_DESC_ADVS 1
#define DEF_UNSOLICITED_DESC_ADVS 1
#define ARG_UNSOLICITED_DESC_ADVS "unsolicited_descriptions"

#define MIN_DHS0_REQS_TX_ITERS 1
#define MAX_DHS0_REQS_TX_ITERS 100
#define DEF_DHS0_REQS_TX_ITERS 40
#define ARG_DHS0_REQS_TX_ITERS "hash_req_sends"

#define MIN_DSC0_REQS_TX_ITERS 1
#define MAX_DSC0_REQS_TX_ITERS 100
#define DEF_DSC0_REQS_TX_ITERS 40
#define ARG_DSC0_REQS_TX_ITERS "desc_req_sends"

#define MIN_DHS0_ADVS_TX_ITERS 1
#define MAX_DHS0_ADVS_TX_ITERS 100
#define DEF_DHS0_ADVS_TX_ITERS 1
#define ARG_DHS0_ADVS_TX_ITERS "hash_sends"

#define MIN_DSC0_ADVS_TX_ITERS 1
#define MAX_DSC0_ADVS_TX_ITERS 20
#define DEF_DSC0_ADVS_TX_ITERS 1
#define ARG_DSC0_ADVS_TX_ITERS "desc_sends"

#define MIN_OGM_TX_ITERS 0
#define MAX_OGM_TX_ITERS 20
#define DEF_OGM_TX_ITERS 4
#define ARG_OGM_TX_ITERS "ogm_sends"

#define DEF_OGM_ACK_TX_ITERS 1
#define MIN_OGM_ACK_TX_ITERS 0
#define MAX_OGM_ACK_TX_ITERS 4
#define ARG_OGM_ACK_TX_ITERS "ogm_ack_sends"



//TODO: set REQ_TO to 1 (in a non-packet-loss testenvironment this may be set to 1000 for testing)
#define DEF_TX_DESC0_REQ_TO  ((DEF_TX_INTERVAL*3)/2)
#define DEF_TX_DESC0_ADV_TO  200
#define DEF_TX_DHASH0_REQ_TO ((DEF_TX_INTERVAL*3)/2)
#define DEF_TX_DHASH0_ADV_TO 200

#define MIN_DESC0_REFERRED_TO 10000
#define MAX_DESC0_REFERRED_TO 100000
#define DEF_DESC0_REFERRED_TO 10000

#define DEF_DESC0_REQ_STONE_OLD_TO 40000

#define MAX_PKT_MSG_SIZE (MAX_UDPD_SIZE - sizeof(struct packet_header) - sizeof(struct frame_header_long))

#define MAX_DESC0_TLV_SIZE (MAX_PKT_MSG_SIZE - sizeof(struct msg_description_adv) )



#define FRAME_TYPE_RSVD0       0
#define FRAME_TYPE_RSVD1       1
#define FRAME_TYPE_PROBLEM_ADV 2  // yet only used to indicate link_id collisions
#define FRAME_TYPE_DSC0_REQS   3
#define FRAME_TYPE_DSC0_ADVS   4  // descriptions are send as individual advertisement frames ???
#define FRAME_TYPE_RSVD5       5
#define FRAME_TYPE_RSVD6       6
#define FRAME_TYPE_DHS0_REQS   7  // Hash-for-description-of-OG-ID requests
#define FRAME_TYPE_DHS0_ADVS   8  // Hash-for-description-of-OG-ID advertisements
#define FRAME_TYPE_RSVD9       9
#define FRAME_TYPE_RSVD10      10
#define FRAME_TYPE_HELLO_ADVS  11 // most-simple BMX-NG hello (nb-discovery) advertisements
#define FRAME_TYPE_HELLO_REPS  12 // most-simple BMX-NG hello (nb-discovery) replies
#define FRAME_TYPE_RSVD13      13
#define FRAME_TYPE_RSVD14      14
#define FRAME_TYPE_OGM_ADVS    15 // most simple BMX-NG (type 0) OGM advertisements
#define FRAME_TYPE_OGM_ACKS    16 // most simple BMX-NG (type 0) OGM advertisements
#define FRAME_TYPE_NOP         17
#define FRAME_TYPE_MAX         (FRAME_TYPE_ARRSZ-1)


struct frame_header_short { // 2 bytes

#if __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned int type : FRAME_TYPE_BIT_SIZE;
	unsigned int is_relevant : FRAME_RELEVANCE_BIT_SIZE;
	unsigned int is_short : FRAME_ISSHORT_BIT_SIZE;

#elif __BYTE_ORDER == __BIG_ENDIAN
	unsigned int is_short : FRAME_ISSHORT_BIT_SIZE;
	unsigned int is_relevant : FRAME_RELEVANCE_BIT_SIZE;
	unsigned int type : FRAME_TYPE_BIT_SIZE;
#else
# error "Please fix <bits/endian.h>"
#endif
	uint8_t length_2B; // lenght of frame in 2-Byte steps, including frame_header and variable data field
//	uint8_t  data[];   // frame-type specific data consisting of 0-1 data headers and 1-n data messages
} __attribute__((packed));


struct frame_header_long { // 4 bytes
#if __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned int type : FRAME_TYPE_BIT_SIZE;
	unsigned int is_relevant : FRAME_RELEVANCE_BIT_SIZE;
	unsigned int is_short : FRAME_ISSHORT_BIT_SIZE;

#elif __BYTE_ORDER == __BIG_ENDIAN
	unsigned int is_short : FRAME_ISSHORT_BIT_SIZE;
	unsigned int is_relevant : FRAME_RELEVANCE_BIT_SIZE;
	unsigned int type : FRAME_TYPE_BIT_SIZE;
#else
# error "Please fix <bits/endian.h>"
#endif
	uint8_t  reserved;
	uint16_t length_1B;  // lenght of frame in 1-Byte steps, including frame_header and variable data field
//	uint8_t  data[];  // frame-type specific data consisting of 0-1 data headers and 1-n data messages
} __attribute__((packed));





#define SHORT_FRAME_DATA_MAX (MIN( 500, ((int)((((sizeof( ((struct frame_header_short*)NULL)->length_2B ))<<8)-1)*2))))


// iterator return codes:
#define TLV_DATA_BLOCKED    (-3) // blocked due to DAD, only returns from rx-iterations
#define TLV_DATA_FULL       (-2) // nothing done! Frame finished or not enough remining data area to write
                                 // only returns from tx-iterations, rx- will return FAILURE
#define TLV_DATA_FAILURE    (-1) // syntax error: exit or blacklist
                                 // only returns from rx-iterations, tx- will fail assertion()
#define TLV_TX_DATA_DONE        (0) // done, nothing more to do
#define TLV_RX_DATA_DONE        (0) // done, nothing more to do
#define TLV_TX_DATA_IGNORED     (1) // unknown, filtered, nothing to send, or ignored due to bad link...
#define TLV_RX_DATA_IGNORED     (1) // unknown, filtered, nothing to send, or ignored due to bad link...
#define TLV_DATA_PROCESSED   (2) // >= means succesfully processed returned amount of data
#define TLV_DATA_STEPS       (2) // legal data-size steps, never returns
                                 // the smalles legal frame must be:
                                 // - a multiple of two
                                 // - have lenght of frame_header_short plus 2 bytes frame_data

// rx_frame_iterator operation codes:
enum {
	TLV_OP_DEL = 0,
	TLV_OP_TEST = 1,
	TLV_OP_ADD = 2,
//	TLV_OP_DONE = 3,
	TLV_OP_DEBUG = 4
};
/*
 * this iterator is given the beginning of a frame area (e.g. the end of the packet_header)
 * then it iterates over the frames in that area */
struct rx_frame_iterator {
	// MUST be initialized:
	// remains unchanged:
	const char         *caller;
	struct packet_buff *pb;
	struct orig_node   *on;
	struct ctrl_node   *cn;
	uint8_t            *frames_in;
	struct frame_handl *handls;
	IDM_T               op;
	uint8_t             process_filter;
	uint8_t             handl_max;
	int32_t             frames_length;

	// updated by rx..iterate():
	int32_t             frames_pos;

	// set by rx..iterate(), and consumed by handl[].rx_tlv_handler
	uint8_t             is_short_header;
	uint8_t             frame_type;
	int32_t             frame_data_length;
	uint8_t            *frame_data;
	uint8_t            *msg;
};


/*
 * this iterator is given a fr_type and a set of handlers,
 * then the handlers are supposed to figure out what needs to be done.
 * finally the iterator writes ready-to-send frame_header and frame data to *fs_data */
struct tx_frame_iterator {
	// MUST be initialized:
	// remains unchanged:
	const char          *caller;
	struct list_head    *tx_task_list;
	struct tx_task_node *ttn;

	uint8_t             *cache_data_array;
	uint8_t             *frames_out;
	struct frame_handl  *handls;
	uint8_t              handl_max;
	int32_t              frames_out_max;

        // updated by fs_caller():
	uint8_t              frame_type;

	// updated by tx..iterate():
	int32_t              frames_out_pos;
	int32_t              cache_msgs_size;

//#define tx_iterator_cache_data_space( it ) (((it)->frames_out_max) - ((it)->frames_out_pos + (it)->cache_msg_pos + ((int)(sizeof (struct frame_header_long)))))
//#define tx_iterator_cache_hdr_ptr( it ) ((it)->cache_data_array)
//#define tx_iterator_cache_msg_ptr( it ) ((it)->cache_data_array + (it)->cache_msg_pos)
};



struct frame_handl {
        uint8_t is_advertisement;              // NO link information required for tx_frame_...(), dev is enough
	uint8_t is_destination_specific_frame; // particularly: is NO advertisement
	uint8_t is_relevant; // if set to ONE specifies: frame MUST BE processed or in case of unknown frame type, the
	                     // whole super_frame MUST be dropped. If set to ZERO the frame can be ignored.
	                     // if frame->is_relevant==1 and unknown and super_frame->is_relevant==1, then
	                     // the whole super_frame MUST BE dropped as well.
	                     // If irrelevant and unknown frames are rebroadcasted depends on the super_frame logic.
	                     // i.e.: * unknown packet_frames MUST BE dropped.
	                     //       * unknown and irrelevant description_tlv_frames MUST BE propagated
	uint8_t rx_requires_described_neigh_dhn;
	uint8_t rx_requires_known_neighIID4me;
        uint16_t data_header_size;
        uint16_t min_msg_size;
        uint16_t fixed_msg_size;
        uint16_t min_tx_interval;
        int32_t *tx_iterations;
        UMETRIC_T *umetric_rq_min;
        char *name;

	int32_t (*rx_frame_handler) (struct rx_frame_iterator *);
	int32_t (*rx_msg_handler)   (struct rx_frame_iterator *);
	int32_t (*tx_frame_handler) (struct tx_frame_iterator *);
	int32_t (*tx_msg_handler)   (struct tx_frame_iterator *);
};


static inline uint8_t * tx_iterator_cache_hdr_ptr(struct tx_frame_iterator *it)
{
	return it->cache_data_array;
}

static inline uint8_t * tx_iterator_cache_msg_ptr(struct tx_frame_iterator *it)
{
	return it->cache_data_array + it->handls[it->frame_type].data_header_size + it->cache_msgs_size;
}

static inline int32_t tx_iterator_cache_data_space(struct tx_frame_iterator *it)
{
	return it->frames_out_max - (
		it->frames_out_pos +
		it->handls[it->frame_type].data_header_size +
		it->cache_msgs_size +
		(int) sizeof(struct frame_header_long));
}




#define FRAME_TYPE_PROBLEM_CODE_MIN          0x01
#define FRAME_TYPE_PROBLEM_CODE_DUP_LINK_ID  0x01
#define FRAME_TYPE_PROBLEM_CODE_MAX          0x01


struct msg_problem_adv { // 4 bytes
	uint8_t code;
	uint8_t reserved;
	LINK_ID_T link_id;
} __attribute__((packed));






struct msg_hello_adv { // 2 bytes
	HELLO_FLAGS_SQN_T hello_flags_sqn;
} __attribute__((packed));

struct msg_hello_reply { // 6 bytes
	HELLO_FLAGS_SQN_T hello_flags_sqn;
	IID_T transmitterIID4x;
} __attribute__((packed));



struct msg_dhash_request { // 4 bytes
	IID_T receiverIID4x; // IID_RSVD_4YOU to ask for neighbours' dhash0
} __attribute__((packed));

struct hdr_dhash_request {
	LINK_ID_T destination_link_id;
	struct msg_dhash_request msg[];
} __attribute__((packed));


struct msg_dhash_adv { // 2 + X bytes
	IID_T transmitterIID4x;
	struct description_hash dhash;
} __attribute__((packed));


#define msg_description_request msg_dhash_request
#define hdr_description_request hdr_dhash_request



struct description {
	struct description_id id;

	uint16_t code_version;
	uint16_t dsc_tlvs_len;

	DESC_SQN_T dsc_sqn;
	uint16_t capabilies;

	OGM_SQN_T ogm_sqn_min;
	OGM_SQN_T ogm_sqn_range;

	uint16_t tx_interval;

	uint8_t ttl_max;
	uint8_t reserved1;

	uint32_t reserved[4]; //ensure traditional message size

//	uint8_t tlv_frames[];
} __attribute__((packed));


#define MSG_DESCRIPTION0_ADV_UNHASHED_SIZE  2
#define MSG_DESCRIPTION0_ADV_HASHED_SIZE   (sizeof( struct description_id) + (8 * sizeof(uint32_t)))
#define MSG_DESCRIPTION0_ADV_SIZE  (MSG_DESCRIPTION0_ADV_UNHASHED_SIZE + MSG_DESCRIPTION0_ADV_HASHED_SIZE)

struct msg_description_adv {
	
	// the unhashed part:
	IID_T    transmitterIID4x; // orig_sid

	// the hashed pard:
	struct description desc;

} __attribute__((packed));





#define OGM_JUMPS_PER_AGGREGATION 10

#define OGMS_PER_AGGREG_MAX                                                                                         \
              ( (pref_udpd_size -                                                                                   \
                  (sizeof(struct packet_header) + sizeof(struct frame_header_long) + sizeof(struct hdr_ogm_adv) +   \
                    (OGM_JUMPS_PER_AGGREGATION * sizeof(struct msg_ogm_adv)) ) ) /                              \
                (sizeof(struct msg_ogm_adv)) )

#define OGMS_PER_AGGREG_PREF ( OGMS_PER_AGGREG_MAX  / 2 )




#define OGM_IID_RSVD_JUMP  (OGM_IIDOFFST_MASK) // 63 //255 // resulting from ((2^transmitterIIDoffset_bit_range)-1)



struct msg_ogm_adv // 4 bytes
{
	OGM_MIX_T mix; //uint16_t mix of transmitterIIDoffset, metric_exp, and metric_mant

//#if __BYTE_ORDER == __LITTLE_ENDIAN
//	unsigned int metric_mant : OGM_MANTISSA_BIT_SIZE;
//	unsigned int metric_exp : OGM_EXPONENT_BIT_SIZE;
//	unsigned int transmitterIIDoffset : IIDOFFSET_SIZE;
//
//#elif __BYTE_ORDER == __BIG_ENDIAN
//	unsigned int transmitterIIDoffset : IIDOFFSET_SIZE;
//	unsigned int metric_exp : OGM_EXPONENT_BIT_SIZE;
//	unsigned int metric_mant : OGM_MANTISSA_BIT_SIZE;
//#else
//# error "Please fix <bits/endian.h>"
//#endif

	union {
		OGM_SQN_T ogm_sqn;
		IID_T transmitterIIDabsolute;
	} u;

} __attribute__((packed));


struct hdr_ogm_adv { // 2 bytes
	AGGREG_SQN_T aggregation_sqn;
	struct msg_ogm_adv msg[];
} __attribute__((packed));

/*
 * reception triggers:
 * - (if link <-> neigh <-... is known and orig_sid is NOT known) msg_dhash0_request[ ... orig_did = orig_sid ]
 * - else update_orig(orig_sid, orig_sqn)
 */

struct msg_ogm_ack {
	IID_T transmitterIID4x;
	AGGREG_SQN_T aggregation_sqn;
} __attribute__((packed));
/*
 * reception triggers:
 * - (if link <-> neigh <-... is known and orig_sid is NOT known) msg_dhash0_request[ ... orig_did = orig_sid ]
 * - else update_orig(orig_sid, orig_sqn)
 */


#define BMX_DSC_TLV_METRIC      0x00
#define BMX_DSC_TLV_UHNA4       0x01
#define BMX_DSC_TLV_UHNA6       0x02
#define BMX_DSC_TLV_MAX        (FRAME_TYPE_ARRSZ-1)
#define BMX_DSC_TLV_ARRSZ       FRAME_TYPE_ARRSZ


#define DEF_DESCRIPTION_GREP         0
#define MIN_DESCRIPTION_GREP         0
#define MAX_DESCRIPTION_GREP         FRAME_TYPE_PROCESS_ALL
#define ARG_DESCRIPTION_GREP         "description"

#define ARG_DESCRIPTION_NAME    "name"
#define ARG_DESCRIPTION_IP      "ip"



struct description_cache_node {
	struct description_hash dhash;
        TIME_T timestamp;
        struct description *description;
};

extern uint32_t ogm_aggreg_pending;
extern IID_T myIID4me;
extern TIME_T myIID4me_timestamp;

extern struct frame_handl packet_frame_handler[FRAME_TYPE_ARRSZ];
extern struct frame_handl description_tlv_handl[BMX_DSC_TLV_ARRSZ];

extern char *tlv_op_str[];


/***********************************************************
  The core frame/message structures and handlers
************************************************************/


OGM_SQN_T set_ogmSqn_toBeSend_and_aggregated(struct orig_node *on, OGM_SQN_T to_be_send, OGM_SQN_T aggregated);
void update_my_description_adv( void );
struct dhash_node * process_description(struct packet_buff *pb, struct description *desc, struct description_hash *dhash);
IDM_T process_description_tlvs(struct packet_buff *pb, struct orig_node *on, struct description *desc, IDM_T op, uint8_t filter, struct ctrl_node *cn);
void purge_tx_timestamp_tree(struct dev_node *dev, IDM_T purge_all);
void purge_tx_task_list(struct list_head *tx_tasks_list, struct link_node *only_link, struct dev_node *only_dev);

void tx_packets( void *unused );
IDM_T rx_frames(struct packet_buff *pb);
int32_t rx_frame_iterate(struct rx_frame_iterator* it);

void schedule_tx_task(struct link_dev_node *lndev_out,
	uint16_t type, uint16_t msgs_len, uint16_t u16, uint32_t u32, IID_T myIID4x, IID_T neighIID4x);

void register_frame_handler(struct frame_handl *array, int pos, struct frame_handl *handl);

struct plugin *msg_get_plugin( void );
