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

#define BMX_LIB_CONFIG "bmx6_config.so"

#define ARG_PLUGIN "plugin"


//for config and rareley used hooks:

enum {
	PLUGIN_CB_STATUS,
	PLUGIN_CB_CONF,
	PLUGIN_CB_DEV_EVENT,
	PLUGIN_CB_ORIG_CREATE,
	PLUGIN_CB_ORIG_FLUSH,
	PLUGIN_CB_ORIG_DESTROY,
	PLUGIN_CB_LINK_CREATE,
	PLUGIN_CB_LINK_DESTROY,
	PLUGIN_CB_LINKDEV_CREATE,
	PLUGIN_CB_LINKDEV_DESTROY,
	PLUGIN_CB_TERM,
	PLUGIN_CB_SIZE
};

void cb_plugin_hooks(int32_t cb_id, void* data);




//for registering data hooks (attaching plugin data to bmx data structures)

enum {
 PLUGIN_DATA_ORIG,
 PLUGIN_DATA_DEV,
 PLUGIN_DATA_SIZE
};


extern int32_t plugin_data_registries[PLUGIN_DATA_SIZE];


int32_t get_plugin_data_registry( uint8_t data_type );

void **get_plugin_data( void *data, uint8_t data_type, int32_t registry );






// for registering thread hooks (often and fast-to-be-done hooks)
struct cb_node {
	struct list_node list;
	int32_t cb_type;
	void (*cb_handler) ( void );
};

struct cb_fd_node {
	struct list_node list;
	int32_t fd;
	void (*cb_fd_handler) (int32_t fd);
};

extern struct list_head cb_fd_list;
// cb_fd_handler is called when fd received data
// called function may remove itself
void set_fd_hook( int32_t fd, void (*cb_fd_handler) (int32_t fd), int8_t del );


struct cb_route_change_node {
	struct list_node list;
	int32_t cb_type;
	void (*cb_route_change_handler) (uint8_t del, struct orig_node * dest);
};

void set_route_change_hooks(void (*cb_route_change_handler) (uint8_t del, struct orig_node *dest), uint8_t del);
void cb_route_change_hooks(uint8_t del, struct orig_node *dest);



struct cb_packet_node {
	struct list_node list;
	int32_t packet_type;
	void (*cb_packet_handler) (struct packet_buff *);
};


void set_packet_hook(void (*cb_packet_handler) (struct packet_buff *), int8_t del);
void cb_packet_hooks(struct packet_buff *pb);





// for initializing:

struct plugin {
	uint32_t plugin_code_version;
	uint32_t plugin_size;
	char *plugin_name;
	int32_t (*cb_init) ( void );
	void    (*cb_cleanup) ( void );
	//some more advanced (rarely called) callbacks hooks
	void (*cb_plugin_handler[PLUGIN_CB_SIZE]) (void*);

};


struct plugin_node {
	struct list_node list;
	struct plugin *plugin;
	void *dlhandle;
	char *dlname;
};


int activate_plugin(struct plugin *p, void *dlhandle, const char *dl_name);

IDM_T init_plugin(void);
void cleanup_plugin( void );

