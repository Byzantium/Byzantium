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


#define MIN_UPTIME 0
#define MAX_UPTIME 2147383 /*(((TP32/1000)/2)-100) /1000 to talk about seconds and not ms, /2 to not render scheduled events outdated, -100 to be save */
#define DEF_UPTIME 0 

#define DBGT_NONE 0
#define	DBGT_INFO 1
#define	DBGT_WARN 2
#define	DBGT_ERR  3


#define DBGL_MIN	0
#define DBGL_SYS	0
//#define DBGL_ROUTES	1
//#define DBGL_GATEWAYS	2
#define DBGL_CHANGES	3
#define DBGL_ALL	4
#define DBGL_PROFILE	5
//#define DBGL_UNUSED	6
//#define DBGL_SERVICES	7
#define DBGL_DETAILS	8
//#define DBGL_HNAS	9
//#define DBGL_LINKS	10
#define DBGL_TEST	11
#define DBGL_DUMP	12
#define DBGL_MAX 	12
#define DBGL_INVALID	13


extern int unix_sock;

extern struct list_head ctrl_list;

#define ARG_PEDANTIC_CMDCHECK "pedantic_cmd_check"


extern int32_t Client_mode;


#define CONNECTION_END_STR "$"
#define CONNECTION_END_CHR '$'


#define LONG_OPT_ARG_DELIMITER_CHAR '/'//'\\'
#define LONG_OPT_ARG_DELIMITER_STR  "/"//"\\"

#define REFERENCE_KEY_WORD	"ref:"

#define EOS_DELIMITER '#'


#define ARG_RESET "-" /* s-string preamble for call_option() to reset opt to default */
#define ARG_RESET_CHAR '-'
#define OPT_CHILD_UNDEFINED -1

enum {
	CTRL_CLOSE_ERROR,
	CTRL_CLOSE_SUCCESS,
	CTRL_CLOSE_STRAIGHT,
	CTRL_CLOSE_DELAY,
	CTRL_CLEANUP,
	CTRL_PURGE_ALL
};

#define CTRL_CLOSING_TIMEOUT	5000


struct ctrl_node
{
	struct list_node list;
	int fd;
	void (*cn_fd_handler) (struct ctrl_node *);
	TIME_T closing_stamp;
	uint8_t authorized;
	int8_t dbgl;
};

extern struct list_head dbgl_clients[DBGL_MAX+1];

struct dbgl_node
{
	struct list_node list;
	struct ctrl_node *cn;
};



// muting does not help if a changing value like time or seqno occurs durig the first DBG_HIST_TEXT_SIZE bytes
#define DBG_HIST_TEXT_SIZE 80 
#define DBG_HIST_SIZE 20
//#define DBG_HIST_EXPIRE 100000

struct dbg_histogram {
	TIME_T print_stamp;
	int32_t expire;
	uint16_t check_len;
	uint16_t catched;
	char text[ DBG_HIST_TEXT_SIZE ];
};


#ifndef TEST_DEBUG

#define DBG_HIST_NEW	0x00
#define DBG_HIST_MUTING 0x01
#define DBG_HIST_MUTED	0x02

#ifdef  NO_DEBUG_ALL
#define dbgf_all(...) {;}
#else
#define dbgf_all( dbgt, ... ); do { if ( __dbgf_all() ) { _dbgf_all( dbgt, __FUNCTION__, __VA_ARGS__ ); } } while (0)
#endif

#define dbgf( dbgl, dbgt, ...)           _dbgf(         dbgl, dbgt, __FUNCTION__, __VA_ARGS__ )
#define dbgf_cn( cn, dbgl, dbgt, ...)    _dbgf_cn( cn,  dbgl, dbgt, __FUNCTION__, __VA_ARGS__ )
#define dbgf_mute( len, dbgl, dbgt, ...) _dbgf_mute( len, dbgl, dbgt, __FUNCTION__, __VA_ARGS__ )

void dbg ( int8_t dbgl, int8_t dbgt, char *last, ... );
void _dbgf(int8_t dbgl, int8_t dbgt, const char *f, char *last, ...);
void dbg_cn(struct ctrl_node *cn, int8_t dbgl, int8_t dbgt, char *last, ...);
void _dbgf_cn(struct ctrl_node *cn, int8_t dbgl, int8_t dbgt, const char *f, char *last, ...);
void dbg_mute(uint32_t check_len, int8_t dbgl, int8_t dbgt, char *last, ...);
void _dbgf_mute(uint32_t check_len, int8_t dbgl, int8_t dbgt, const char *f, char *last, ...);
void _dbgf_all ( int8_t dbgt, const char *f, char *last, ... );
uint8_t __dbgf_all( void );

void dbg_printf( struct ctrl_node *cn, char *last, ...  );

#else

#define dbgf( dbgl, dbgt, ...)  		printf( __VA_ARGS__ )
#define dbgf_cn( cn, dbgl, dbgt, ...) 		printf( __VA_ARGS__ )
#define dbgf_cn( cn, dbgl, dbgt, ...)           printf( __VA_ARGS__ )
#define dbg( dbgl, dbgt, ... )  		printf( __VA_ARGS__ )
#define dbg_cn( cn, dbgl, dbgt, ... ) 		printf( __VA_ARGS__ )
#define dbg_mute( check_len, dbgl, dbgt, ... ) 	printf( __VA_ARGS__ )
#define dbgf_mute( check_len, dbgl, dbgt, ... ) printf( __VA_ARGS__ )
#define dbgf_all( dbgt, ... ) 			printf( __VA_ARGS__ )
#define dbgf_ext( dbgt, ... ) 			printf( __VA_ARGS__ )
#define dbg_printf( cn, ...  ) 			printf( __VA_ARGS__ )

#endif

void accept_ctrl_node( void );
void handle_ctrl_node( struct ctrl_node *cn );
void close_ctrl_node( uint8_t cmd, struct ctrl_node *cn );
struct ctrl_node *create_ctrl_node( int fd, void (*cn_fd_handler) (struct ctrl_node *), uint8_t authorized );




#define MAX_UNIX_MSG_SIZE 2000

extern struct list_head opt_list;


/* opt_t types (Parent/Child, Single/Multiple, 0/1/N-arguments) */
#define A_PS0   0x01
#define A_PS1	0x02
#define A_PMN   0x14
#define A_CS1	0x22


/* auth_t types */
#define A_ADM	0x10
#define A_USR	0x00

/* dyn_t types: */
// can only be used on-the-fly
#define A_DYN	0x20 
// can never be used on-the-fly
#define A_INI	0x40
// can be used during init and on-the-fly
#define A_DYI	0x60

/* cfg_t types: */
// can only be set as command-line argument. NOT shows as relevant parameter
#define A_ARG	0x02
// can be set in config file and as command-line argument. Shows as relevant parameter
#define A_CFA	0x03

/* pos_t types: */
// must be given as first argument
#define A_BEG   0x01 NOT IMPLEMENTED 
// may appera anywhere in command stream
#define A_ANY	0x02
// must appear as last argument
#define A_END	0x03
// may appera anywhere in command stream but consumes remaining arguments
#define A_EAT	0x04
// must appear as last argunent and cosumes remaining arguments
#define A_ETE	0x05


extern char *opt_cmd2str[];


struct opt_child {
	
	struct list_node list;
	
	struct opt_type *c_opt; // key,  pointing to related opt_type
	struct opt_parent *parent_instance;
	
	char *c_val;
	
	uint8_t p_diff; //ADD, DEL, NOP
	
	char *c_ref;
};


struct opt_parent {
	
	struct list_node list;
	
	struct list_head childs_instance_list;
	
	char *p_val; //key
	
	uint8_t p_diff; //ADD, DEL, NOP
	
	char *p_ref;
	
};

#define ODI {{0},0,{0,0,0,0,0,0},{0,0,0,0,0,0}}

struct opt_data {
	
	struct list_node list;
	
	struct opt_type *parent_opt; //REMOVE THIS and use casting instead !
	
	struct list_head childs_type_list; //if this opt is a section type, then further sub-opts types can be listed here
	
	struct list_head parents_instance_list; //
//	uint16_t found_parents;
	
};

struct opt_type {
	
	struct opt_data d; //MUST be first structure in opt_type to allow casting between struct opt_data and  struct opt_type
	
//	int8_t old_order;  // enforces an order during the init process,  (0==anytime????), 1..99: in this order. Might become removed
	
	char *parent_name;
	char *long_name;

	char short_name;
	int8_t order;  // enforces an order during the init process,  (0==anytime????), 1..99: in this order. Might become removed
	
	uint8_t opt_t;
	
	uint8_t auth_t;
	uint8_t dyn_t;
	uint8_t cfg_t;
	uint8_t pos_t;
	
	// if != NULL call_option() will be initialize / reset(ARG_RESET) to idef
	int32_t *ival; 
	// if imin != imax call_option() will test for validity
	int32_t imin;
	int32_t imax;  
	int32_t idef;
	
	int32_t (*call_custom_option)( uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn );
	
#define ARG_VALUE_FORM	"<VAL>"
#define ARG_FILE_FORM	"<FILE>"
#define ARG_DIR_FORM	"<DIRECTORY>"
#define ARG_PORT_FORM	"<PORT>"
#define ARG_ADDR_FORM	"<ADDRESS>"
#define ARG_PREFIX_FORM	"<NETADDR>/<PREFIX-LENGTH>"
#define ARG_NETW_FORM	"<NETADDR>"
#define ARG_MASK_FORM	"<NETMASK>"
	
	char	*syntax;
	char	*help;
	
};



enum opt_cmd {
	// option handlers are registered, configured, and unregistered using primitives.
	// for registration and unregistration the primitives OPT_REGISTER and OPT_UNREGISTER are used
	// for configuration the primitives OPT_PATCH, OPT_ADJUST, OPT_CHECK, OPT_APPLY, OPT_SET_POST, OPT_POST are used


	// called once for each option after registration
	// Returns FAILURE or SUCCESS
	OPT_REGISTER,
		
		
	OPT_PATCH,
	// opt values are configured by creating, extending, adjusting and finally testing and applying a patch
	// option handler usually dont care about the creation of the patch.
	// they get a complete patch which includes 
	// the option type-value pair and optional child type-valued pairs.
		
		
	OPT_ADJUST,
	// patched type-values pairs can be adjusted to a unified format before being checked or applied 
	// this has the following advantages:
	// tracked and applied values are equal (different value notations can be adjusted to a unified format)
	//	-> track knows about already configured values (even when given with different notation)
	//	-> can prevent/warn reconfiguration of already configured values
	//	-> can reject resetting of non-configured values
	
		
	OPT_CHECK,
	// to test a given patch (type-value pair) !
	// Returns FAILURE or n>=0 of processed bytes-1
		
		
	OPT_APPLY,
	// to apply a previously created and adjusted patch
	// Returns FAILURE or n>=0 of processed bytes-1
		
		
	OPT_SET_POST,
	// called whenever any option is changed and
	// called ordered for each option and before next higher-order option   
	// Returns FAILURE or SUCCESS
		
		
	OPT_POST,
	// called whenever any option is changed  and
	// called ordered for each option after all options were (re-)set
	// Returns FAILURE or SUCCESS
		
	
	OPT_UNREGISTER
	// called once before an option is unregistered
	// Returns FAILURE or SUCCESS
		
};

// evaluates global variable: "on_the_fly"
// fd may be set (>0) or not (=0)
// cmd==OPT_SET_POST / OPT_POST: 
//	s MBZ, return value is SUCCESS or FAILURE
int32_t call_option( uint8_t del, uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *parent, char *in, struct ctrl_node *cn );

int32_t check_apply_parent_option( uint8_t del, uint8_t cmd, uint8_t _save, struct opt_type *opt, char *in, struct ctrl_node *cn );

struct opt_type *get_option( struct opt_type *parent_opt, uint8_t short_opt, char *s );



void 			set_opt_child_val ( struct opt_child *c, char *val );
struct opt_child *	get_opt_child ( struct opt_type *opt, struct opt_parent *p );


void 			del_opt_parent( struct opt_type *opt, struct opt_parent *parent );
struct opt_parent *	add_opt_parent( struct opt_type *opt );

void 			set_opt_parent_val ( struct opt_parent *p, char *val );
struct opt_parent *	get_opt_parent_val ( struct opt_type *opt, char *val );
void 			set_opt_parent_ref ( struct opt_parent *p, char *ref );
struct opt_parent *	get_opt_parent_ref ( struct opt_type *opt, char *ref );



int8_t func_for_each_opt( struct ctrl_node *cn, void *data, char* func_name, 
                          int8_t (*func) ( struct ctrl_node *cn, void *data, struct opt_type *opt, struct opt_parent *p, struct opt_child *c ) );

int respect_opt_order( uint8_t test, int8_t last, int8_t next, struct opt_type *on, uint8_t load_config, uint8_t cmd, struct ctrl_node *cn );

int8_t apply_stream_opts( char *s, uint8_t dryrun, uint8_t load_cfg, struct ctrl_node *cn );


extern int (*load_config_cb) ( uint8_t test, struct opt_type *opt, struct ctrl_node *cn );

extern int (*save_config_cb) ( uint8_t del, struct opt_type *opt, char *parent, char *val, struct ctrl_node *cn );

extern int (*derive_config) ( char *reference, char *derivation, struct ctrl_node *cn );


void register_option( struct opt_type *opt );
//void remove_option( struct opt_type *opt );
void register_options_array ( struct opt_type *fixed_options, int size );

extern int32_t Load_config;


#ifdef ADJ_PATCHED_NETW
int32_t get_tracked_network( struct opt_type *opt, struct opt_parent *patch, char *out, uint32_t *ip, int32_t *mask, struct ctrl_node *cn );
int32_t adj_patched_network( struct opt_type *opt, struct opt_parent *patch, char *out, uint32_t *ip, int32_t *mask, struct ctrl_node *cn );
#endif

void apply_init_args ( int argc, char *argv[] );

extern struct opt_type Patch_opt;


void init_control( void );
void cleanup_control( void );

void cleanup_config( void );


#ifdef EXPORT_UNUSED
char *debugWordDup( char* word, int32_t tag );
static void strchange( char *s, char i, char o );
char* nextword( char *s );
#endif

