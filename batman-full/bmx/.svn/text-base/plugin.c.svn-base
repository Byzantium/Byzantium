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
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>

#include "bmx.h"
#include "msg.h"
#include "ip.h"
#include "plugin.h"
#include "schedule.h"
#include "tools.h"


static LIST_SIMPEL(plugin_list, struct plugin_node, list, list);


static LIST_SIMPEL(cb_route_change_list, struct cb_route_change_node, list, list);
static LIST_SIMPEL(cb_packet_list, struct cb_packet_node, list, list);
LIST_SIMPEL(cb_fd_list, struct cb_fd_node, list, list);



int32_t plugin_data_registries[PLUGIN_DATA_SIZE];

void cb_plugin_hooks(int32_t cb_id, void* data)
{
        TRACE_FUNCTION_CALL;
	struct list_node *list_pos;
	struct plugin_node *pn, *prev_pn = NULL;
	
	list_for_each( list_pos, &plugin_list ) {
		
		pn = list_entry( list_pos, struct plugin_node, list );
		
		if ( prev_pn  &&  prev_pn->plugin  &&  prev_pn->plugin->cb_plugin_handler[cb_id] )
			(*(prev_pn->plugin->cb_plugin_handler[cb_id])) ( data );
		
		prev_pn = pn;
	}
	
	if ( prev_pn  &&  prev_pn->plugin  &&  prev_pn->plugin->cb_plugin_handler[cb_id] )
		((*(prev_pn->plugin->cb_plugin_handler[cb_id])) (data));
	
}


STATIC_FUNC
void _set_thread_hook(int32_t cb_type, void (*cb_handler) (void), int8_t del, struct list_node *cb_list)
{
	struct list_node *list_pos, *tmp_pos, *prev_pos = cb_list;
	struct cb_node *cbn;
		
	if ( !cb_type  ||  !cb_handler ) {
		cleanup_all( -500143 );
	}
	
	list_for_each_safe( list_pos, tmp_pos, (struct list_head*) cb_list ) {
			
		cbn = list_entry( list_pos, struct cb_node, list );
		
		if ( cb_type == cbn->cb_type  &&  cb_handler == cbn->cb_handler ) {

			if ( del ) {

                                list_del_next(((struct list_head*) cb_list), prev_pos);
				debugFree( cbn, -300069 );
                                return;
				
			} else {
				cleanup_all( -500144 );
				//dbgf( DBGL_SYS, DBGT_ERR, "cb_hook for cb_type %d and cb_handler already registered", cb_type );
			}
			
		} else {
			
			prev_pos = &cbn->list;
		}
	}

        assertion(-500145, (!del));

        cbn = debugMalloc(sizeof ( struct cb_node), -300027);
        memset(cbn, 0, sizeof ( struct cb_node));

        cbn->cb_type = cb_type;
        cbn->cb_handler = cb_handler;
        list_add_tail(((struct list_head*) cb_list), &cbn->list);

}


void set_route_change_hooks(void (*cb_route_change_handler) (uint8_t del, struct orig_node *dest), uint8_t del)
{
        _set_thread_hook(1, (void (*) (void)) cb_route_change_handler, del, (struct list_node*) & cb_route_change_list);
}


// notify interested plugins of a changed route...
// THIS MAY CRASH when one plugin unregisteres two packet_hooks while being called with cb_packet_handler()
void cb_route_change_hooks(uint8_t del, struct orig_node *dest)
{
        TRACE_FUNCTION_CALL;
	struct list_node *list_pos;
	struct cb_route_change_node *con, *prev_con = NULL;

        assertion(-500674, (dest && dest->desc));

	list_for_each( list_pos, &cb_route_change_list ) {

		con = list_entry( list_pos, struct cb_route_change_node, list );

		if ( prev_con )
                        (*(prev_con->cb_route_change_handler)) (del, dest);

                prev_con = con;

        }

        if (prev_con)
                (*(prev_con->cb_route_change_handler)) (del, dest);

}

void set_packet_hook(void (*cb_packet_handler) (struct packet_buff *), int8_t del)
{
        _set_thread_hook(1, (void (*) (void)) cb_packet_handler, del, (struct list_node*) &cb_packet_list);
}

void cb_packet_hooks(struct packet_buff *pb)
{
        TRACE_FUNCTION_CALL;

	struct list_node *list_pos;
	struct cb_packet_node *cpn, *prev_cpn = NULL;

	list_for_each( list_pos, &cb_packet_list ) {

		cpn = list_entry( list_pos, struct cb_packet_node, list );

                if (prev_cpn) {

			(*(prev_cpn->cb_packet_handler)) (pb);

		}

		prev_cpn = cpn;

        }

        if (prev_cpn)
                (*(prev_cpn->cb_packet_handler)) (pb);

}


void set_fd_hook( int32_t fd, void (*cb_fd_handler) (int32_t fd), int8_t del ) {

        _set_thread_hook(fd, (void (*) (void)) cb_fd_handler, del, (struct list_node*) & cb_fd_list);

	change_selects();
}

int32_t get_plugin_data_registry(uint8_t data_type)
{
        TRACE_FUNCTION_CALL;
	
	static int is_plugin_data_initialized = NO;
	
	if ( !is_plugin_data_initialized ) {
		memset( plugin_data_registries, 0, sizeof( plugin_data_registries ) );
		is_plugin_data_initialized=YES;
	}
	
	if ( !initializing || data_type >= PLUGIN_DATA_SIZE )
		return FAILURE;
	
	// do NOT return the incremented value! 
        return (((plugin_data_registries[data_type])++));
}

void **get_plugin_data(void *data, uint8_t data_type, int32_t registry)
{
        TRACE_FUNCTION_CALL;
	
	if ( data_type >= PLUGIN_DATA_SIZE  ||  registry > plugin_data_registries[data_type] ) {
		cleanup_all( -500145 );
		//dbgf( DBGL_SYS, DBGT_ERR, "requested to deliver data for unknown registry !");
		//return NULL;
	}
		
	if ( data_type == PLUGIN_DATA_ORIG )
		return &(((struct orig_node*)data)->plugin_data[registry]);

        if ( data_type == PLUGIN_DATA_DEV )
                return &(((struct dev_node*)data)->plugin_data[registry]);
	
	return NULL;
}






STATIC_FUNC
int is_plugin_active( void *plugin ) {
	
	struct list_node *list_pos;
		
	list_for_each( list_pos, &plugin_list ) {
			
		if ( ((struct plugin_node *) (list_entry( list_pos, struct plugin_node, list )))->plugin == plugin )
			return YES;
	
	}
	
	return NO;
}


int activate_plugin(struct plugin *p, void *dlhandle, const char *dl_name)
{

        if (p == NULL)
		return SUCCESS;
	
	if ( is_plugin_active( p ) )
		return FAILURE;
	

        if (p->plugin_size != sizeof ( struct plugin) || (p->plugin_code_version != CODE_VERSION)) {

                dbgf(DBGL_SYS, DBGT_ERR,
                        "plugin with unexpected size %d != %zu, revision %d != %d",
                        p->plugin_size, sizeof ( struct plugin), p->plugin_code_version, CODE_VERSION);

                return FAILURE;
        }


        if ( p->cb_init == NULL  ||  ((*( p->cb_init )) ()) == FAILURE ) {

                dbg( DBGL_SYS, DBGT_ERR, "could not init plugin");
                return FAILURE;
        }

        struct plugin_node *pn = debugMalloc( sizeof( struct plugin_node), -300028);
        memset( pn, 0, sizeof( struct plugin_node) );

        pn->plugin = p;
        pn->dlhandle = dlhandle;

        list_add_tail(&plugin_list, &pn->list);

        dbgf_all( DBGT_INFO, "%s SUCCESS", pn->plugin->plugin_name );

        if ( dl_name ) {
                pn->dlname = debugMalloc( strlen(dl_name)+1, -300029 );
                strcpy( pn->dlname, dl_name );
        }

        return SUCCESS;
}

STATIC_FUNC
void deactivate_plugin( void *p ) {
	
	if ( !is_plugin_active( p ) ) {
		cleanup_all( -500190 );
	}
	
	struct list_node *list_pos, *tmp_pos, *prev_pos = (struct list_node*)&plugin_list;
		
	list_for_each_safe( list_pos, tmp_pos, &plugin_list ) {
			
		struct plugin_node *pn = list_entry( list_pos, struct plugin_node, list );
			
		if ( pn->plugin == p ) {

                        list_del_next(&plugin_list, prev_pos);
			
			dbg( DBGL_CHANGES, DBGT_INFO, "deactivating plugin %s", pn->plugin->plugin_name );
			
			if ( pn->plugin->cb_cleanup )
				(*( pn->plugin->cb_cleanup )) ();
			
				
			if ( pn->dlname)
				debugFree( pn->dlname, -300070);
			
			debugFree( pn, -300071);
			
		} else {
			
			prev_pos = &pn->list;
			
		}

	}

}

#ifndef NO_DYN_PLUGIN
STATIC_FUNC
int8_t activate_dyn_plugin( const char* name ) {
	
	struct plugin* (*get_plugin) ( void ) = NULL;
	
	void *dlhandle;
	struct plugin *pv1;
	char dl_path[1000];
	
	char *My_libs = getenv(BMX_ENV_LIB_PATH);
	
	if ( !name )
		return FAILURE;
	
	// dl_open sigfaults on some systems without reason.
	// removing the dl files from BMX_DEF_LIB_PATH is a way to prevent calling dl_open.
	// Therefore we restrict dl search to BMX_DEF_LIB_PATH and BMX_ENV_LIB_PATH and ensure that dl_open 
	// is only called if a file with the requested dl name could be found.
	
	if ( My_libs )
		sprintf( dl_path, "%s/%s", My_libs, name );
	else
		sprintf( dl_path, "%s/%s", BMX_DEF_LIB_PATH, name );
	
	
	dbgf_all( DBGT_INFO, "trying to load dl %s", dl_path );
	
	int dl_tried = 0;

	if ( check_file( dl_path, NO, YES ) == SUCCESS  &&
	     (dl_tried = 1)  &&  (dlhandle = dlopen( dl_path, RTLD_NOW )) )
	{
		
		dbgf_all( DBGT_INFO, "succesfully loaded dynamic library %s", dl_path );
		
	} else {
		
		dbg( dl_tried ? DBGL_SYS : DBGL_CHANGES, dl_tried ? DBGT_ERR : DBGT_WARN,
		     "failed loading dl %s %s (maybe incompatible binary/lib versions?)", 
		     dl_path, dl_tried?dlerror():"" );
		
		return FAILURE;
		
	}
	
	dbgf_all( DBGT_INFO, "survived dlopen()!" );


        typedef struct plugin* (*sdl_init_function_type) ( void );

        union {
                sdl_init_function_type func;
                void * obj;
        } alias;

        alias.obj = dlsym( dlhandle, "get_plugin");

	if ( !( get_plugin = alias.func )  ) {
		dbgf( DBGL_SYS, DBGT_ERR, "dlsym( %s ) failed: %s", name, dlerror() );
		return FAILURE;
	}

	
	if ( !(pv1 = get_plugin()) ) {

		dbgf( DBGL_SYS, DBGT_ERR, "get_plugin( %s ) failed", name );
		return FAILURE;
		
	}
	
	if ( is_plugin_active( pv1 ) )
		return SUCCESS;
	

        if (activate_plugin(pv1, dlhandle, name) == FAILURE) {

                dbgf(DBGL_SYS, DBGT_ERR, "activate_plugin( %s ) failed", dl_path);
                return FAILURE;
		
	}
	
	dbg( DBGL_CHANGES, DBGT_INFO, 
	     "loading and activating %s dl %s succeeded",
	     My_libs ? "customized" : "default",   dl_path );
	
	return SUCCESS;
}


STATIC_FUNC
int32_t opt_plugin ( uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn ) {
	
	dbgf_all( DBGT_INFO, "%s %d", opt_cmd2str[cmd], _save );
	
	char tmp_name[MAX_PATH_SIZE] = "";
	
	
	if ( cmd == OPT_CHECK ) {
		
		dbgf_all( DBGT_INFO, "about to load dl %s", patch->p_val );
		
		if ( wordlen(patch->p_val)+1 >= MAX_PATH_SIZE  ||  patch->p_val[0] == '/' )
			return FAILURE;
		
		wordCopy( tmp_name, patch->p_val );
		
		if ( get_opt_parent_val( opt, tmp_name ) )
			return SUCCESS;
		
		if ( activate_dyn_plugin( tmp_name ) == FAILURE )
			return FAILURE;
		
	}
	
	return SUCCESS;
}

static struct opt_type plugin_options[]= 
{
//        ord parent long_name          shrt Attributes				*ival		min		max		default		*func,*syntax,*help
	
	//order> config-file order to be loaded by config file, order < ARG_CONNECT oder to appera first in help text
	{ODI,0,ARG_PLUGIN,		0,  2,A_PMN,A_ADM,A_INI,A_CFA,A_ANY,	0,		0, 		0,		0, 		opt_plugin,
			ARG_FILE_FORM,	"load plugin. "ARG_FILE_FORM" must be in LD_LIBRARY_PATH or " BMX_ENV_LIB_PATH 
			"\n	path (e.g. --plugin bmx6_howto_plugin.so )\n"}
};
#endif

IDM_T init_plugin(void)
{

	
//	set_snd_ext_hook( 0, NULL, YES ); //ensure correct initialization of extension hooks
//	reg_plugin_data( PLUGIN_DATA_SIZE );// ensure correct initialization of plugin_data
	
	struct plugin *p;
	
	p=NULL
                ;
#ifndef NO_DYN_PLUGIN
	// first try loading config plugin, if succesfull, continue loading optinal plugins depending on config
	activate_dyn_plugin( BMX_LIB_CONFIG );

	register_options_array( plugin_options, sizeof( plugin_options ) );
#endif

        return SUCCESS;
}


void cleanup_plugin( void ) {

	while ( !LIST_EMPTY( &plugin_list ) )
		deactivate_plugin( ((struct plugin_node*)(list_entry( (&plugin_list)->next, struct plugin_node, list)))->plugin );

}
