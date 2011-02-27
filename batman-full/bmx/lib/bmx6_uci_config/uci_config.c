/*
 * Copyright (C) 2010 BMX contributors:
 * Axel Neumann
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

#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <uci.h>

#include "../../bmx.h"
#include "../../plugin.h"
#include "../../tools.h"
#include "uci_config.h"

static char conf_path[MAX_PATH_SIZE] = "";
static char *bmx_conf_name = NULL;

static char *uci_err;

static struct uci_context *bmx_ctx = NULL;
static struct uci_ptr bmx_pptr;

static struct uci_context *net_ctx = NULL;

static struct opt_type tmp_conf_opt;


static void signal_hup_handler( int32_t sig ) {
	
	dbgf( DBGL_SYS, DBGT_INFO, "reloading config" );
	
	struct ctrl_node *cn = create_ctrl_node( STDOUT_FILENO, NULL, YES/*we are root*/ );
	
	if ( (apply_stream_opts( ARG_RELOAD_CONFIG, OPT_CHECK, NO/*no cfg by default*/, cn ) == FAILURE)  ||
	     (apply_stream_opts( ARG_RELOAD_CONFIG, OPT_APPLY, NO/*no cfg by default*/, cn ) == FAILURE)  ) 
	{
		close_ctrl_node( CTRL_CLOSE_STRAIGHT, cn );	
		dbg( DBGL_SYS, DBGT_ERR, "reloading config failed! FIX your config NOW!"  );
		return;
	}
	
	close_ctrl_node( CTRL_CLOSE_STRAIGHT, cn );	
	
	respect_opt_order( OPT_APPLY, 0, 99, NULL, NO/*load_cofig*/, OPT_POST, 0/*probably closed*/ );

        cb_plugin_hooks(PLUGIN_CB_CONF, NULL);
	
}



STATIC_FUNC
int8_t uci_reload_package( struct uci_context *ctx, struct uci_ptr *ptr, char* package ) {
	
	uci_unload(ctx, ptr->p);
	
	memset( ptr, 0, sizeof( struct uci_ptr ) );
	ptr->package = package;
	
	if ( uci_lookup_ptr( ctx, ptr, NULL, false) != SUCCESS ) {
		uci_get_errorstr( ctx, &uci_err, "" );
		dbgf( DBGL_SYS, DBGT_ERR, "%s", uci_err );
		return FAILURE;
	}
	
	return SUCCESS;
}



STATIC_FUNC
struct uci_element *uci_lookup( struct uci_context *ctx, struct uci_ptr *ptr, char *name ) {
	
	dbgf_all( DBGT_INFO, "%s", name );
	
	if ( name )
		memset( ptr, 0, sizeof( struct uci_ptr ) );
	
	if ( uci_lookup_ptr( ctx, ptr, name, false) != SUCCESS ) {
		uci_get_errorstr( ctx, &uci_err, "" );
		dbgf( DBGL_SYS, DBGT_ERR, "%s %s", name, uci_err );
		return NULL;
	}
	struct uci_element *e = ptr->last;
	
	if ( !( ptr->flags & UCI_LOOKUP_COMPLETE ) ) {
		dbgf_all( DBGT_INFO, "%s %s %s %s  is not configured", 
		     name, ptr->package, ptr->section, ptr->option );
		return NULL;
	}
	
	return e;
}



STATIC_FUNC
int uci_save_option( struct uci_context *ctx, char *conf_name, char *sect_name, char *opt_name, char *opt_val, struct ctrl_node *cn ) {

	dbgf_cn( cn, DBGL_CHANGES, DBGT_INFO, "%s.%s.%s=%s",
	      conf_name, sect_name, opt_name, opt_val );
	
	struct uci_ptr ptr;
	memset(&ptr, 0, sizeof(ptr));
	ptr.package = conf_name;
	ptr.section = sect_name;
	
	if ( uci_lookup_ptr( ctx, &ptr, NULL, false ) != SUCCESS ) {
		uci_get_errorstr( ctx, &uci_err, "" );
		dbgf_cn( cn, DBGL_SYS, DBGT_ERR, "uci_lookup_str( %s.%s ): %s", 
		        conf_name, sect_name, uci_err );
		return FAILURE;
	}
	
	ptr.option = opt_name;
	ptr.value = opt_val;
	
	if ( uci_set( ctx, &ptr ) != SUCCESS ) {
		uci_get_errorstr( ctx, &uci_err, "" );
		dbgf_cn( cn, DBGL_SYS, DBGT_ERR, "uci_set( %s.%s.%s=%s ): %s",
		        conf_name, sect_name, opt_name, opt_val, uci_err );
		return FAILURE;
	}
	
	if ( uci_save( ctx, ptr.p ) != SUCCESS ) {
		uci_get_errorstr( ctx, &uci_err, "" );
		dbgf_cn( cn, DBGL_SYS, DBGT_ERR, "uci_save( %s.%s.%s=%s ): %s", 
		        conf_name, sect_name, opt_name, opt_val, uci_err );
		return FAILURE;
	}
	
	return SUCCESS;
}



STATIC_FUNC
int uci_create_section(  struct uci_context *ctx, char *conf_name, char *sect_name, char *opt_name, struct ctrl_node *cn ) {

	
	struct uci_ptr ptr;
	
	memset(&ptr, 0, sizeof(ptr));
	ptr.package = conf_name;
	
	if ( wordlen( sect_name ) )
		ptr.section = sect_name;
	
	
	if ( uci_lookup_ptr( ctx, &ptr, NULL, false ) != SUCCESS ) {
		uci_get_errorstr( ctx, &uci_err, "" );
		dbgf_cn( cn, DBGL_SYS, DBGT_ERR, "%s %s uci_lookup_str(): %s", 
		        conf_name, opt_name, uci_err );
		return FAILURE;
	}

	if ( uci_add_section( ctx, ptr.p, opt_name, &ptr.s ) != SUCCESS ) {
		uci_get_errorstr( ctx, &uci_err, "" );
		dbgf_cn( cn, DBGL_SYS, DBGT_ERR, "%s %s uci_add_section(): %s", 
		        conf_name, opt_name, uci_err );
		return FAILURE;
	}
	
	/*
	if ( uci_save( ctx, ptr.p ) != SUCCESS ) {
		uci_get_errorstr( ctx, &uci_err, "" );
		dbgf_fd( fd, DBGL_SYS, DBGT_ERR, "uci_save(): %s", uci_err );
		return FAILURE;
	}
	*/
	
	if ( !wordlen( sect_name ) )
		strcpy( sect_name, ptr.s->e.name );
	
	dbgf_cn( cn, DBGL_CHANGES, DBGT_INFO, "%s.%s=%s",
	        conf_name, ptr.s->e.name, opt_name );
	
	return SUCCESS;
}



STATIC_FUNC
int uci_remove(  struct uci_context *ctx, char *conf_name, char *sect_name, char *opt_name, struct ctrl_node *cn ) {
	
	dbgf_cn( cn, DBGL_CHANGES, DBGT_INFO, "%s.%s %s", conf_name, sect_name, opt_name );
	
	struct uci_ptr ptr;
	
	memset(&ptr, 0, sizeof(ptr));
	ptr.package = conf_name;
	ptr.section = sect_name;
	
	if ( uci_lookup_ptr( ctx, &ptr, NULL, false ) != SUCCESS ) {
		uci_get_errorstr( ctx, &uci_err, "" );
		dbgf_cn( cn, DBGL_SYS, DBGT_ERR, "uci_lookup_str(): %s", uci_err );
		return FAILURE;
	}
	
	ptr.option = opt_name;
	
	if ( uci_delete( ctx, &ptr ) != SUCCESS ) {
		uci_get_errorstr( ctx, &uci_err, "" );
		dbgf_cn( cn, DBGL_SYS, DBGT_ERR, "uci_delete(): %s", uci_err );
		return FAILURE;
	}
	
	if ( uci_save( ctx, ptr.p ) != SUCCESS ) {
		uci_get_errorstr( ctx, &uci_err, "" );
		dbgf_cn( cn, DBGL_SYS, DBGT_ERR, "uci_save(): %s", uci_err );
		return FAILURE;
	}
	
	// without doing this we get double free or corruption after config-reload or uci_remove()
	uci_reload_package( ctx, &bmx_pptr, conf_name );
	
	return SUCCESS;
	
}



STATIC_FUNC
int uci_get_sect_name( uint8_t create, struct ctrl_node *cn, struct uci_context *ctx, 
                       char *conf_name, char *sect_name, char *sect_type,  char *opt_name, char *opt_val ) 
{
	
	struct uci_element *e;
	struct uci_element *se;
	int found=0;
	struct uci_ptr ptr;
	
	dbgf_all( DBGT_INFO, "%s %s %s.*.%s==%s",
	     create?"create":"get", sect_name, conf_name, opt_name, opt_val );
	
	paranoia( -500020, ( !conf_name  ||  !sect_type  ||  !sect_name  ) );
	
	uint8_t named_section = wordlen(sect_name) ? YES : NO;
		
	
	if ( !(e=uci_lookup( ctx, &ptr, conf_name )) )
		return FAILURE;
	
	if ( e->type != UCI_TYPE_PACKAGE )
		return FAILURE;
	
	
	uci_foreach_element( &(ptr.p->sections), se) {
		
		struct uci_section *s = uci_to_section(se);
		struct uci_ptr sptr;
		char name[MAX_ARG_SIZE];
		
		if ( strcmp( sect_type, s->type ) )
			continue;
		
		if ( opt_name ) {
		
			sprintf( name, "%s.%s.%s", conf_name, s->e.name, opt_name );
			
			if ( !(e=uci_lookup( ctx, &sptr, name )) )
				continue;
			
			if ( opt_val  &&  !wordsEqual( sptr.o->v.string, opt_val ) )
				continue;
			
		}
		
		if ( !found  &&  !named_section )
			strcpy( sect_name, s->e.name );
		
		else if ( wordsEqual( sect_name, s->e.name ) )
			return SUCCESS;
			
		found++;
	}
	
	
	if ( found == 0  &&  create ) {
		
		if (  named_section  &&  uci_save_option( ctx, conf_name, sect_name, NULL, sect_type, cn ) != SUCCESS )
			return FAILURE;
		
		if ( !named_section  &&  uci_create_section( ctx, conf_name, sect_name, sect_type, cn ) != SUCCESS )
			return FAILURE;
			
		if ( opt_name  &&  opt_val ) {
			
			if ( uci_save_option( ctx, conf_name, sect_name, opt_name, opt_val, cn ) == SUCCESS )
				return SUCCESS;
			
			return FAILURE;
			
		} else {
			
			return SUCCESS;
		}
			
			
		return FAILURE;
	
	} else if ( found == 1 ) {
		
		return SUCCESS;
		
	} else {
		
		dbgf_cn( cn, DBGL_SYS, DBGT_ERR, 
			"Found %d matching section with %s.*.%s==%s ! FIX your config NOW!",
			found, conf_name, opt_name, opt_val );
	
		return FAILURE;
	}

}


STATIC_FUNC
int bmx_derive_config ( char *reference, char *derivation, struct ctrl_node *cn ) {

	struct uci_ptr sptr;
	char name[MAX_ARG_SIZE];
	struct uci_element *e;
	
	wordCopy( name, reference + strlen( REFERENCE_KEY_WORD ) );
	
	dbgf_all(  DBGT_INFO, "going to lookup %s", name );
	
	if ( !(e=uci_lookup( net_ctx, &sptr, name )) )
		return FAILURE;	
		
	if ( sptr.o  &&  wordlen( sptr.o->v.string ) &&  wordlen( sptr.o->v.string ) < MAX_ARG_SIZE )
		wordCopy( derivation, sptr.o->v.string );
	else 
		return FAILURE;
	
	uci_unload(net_ctx, sptr.p);
	
	return SUCCESS;
}


STATIC_FUNC
int bmx_save_config ( uint8_t del, struct opt_type *opt, char *p_val, char *c_val, struct ctrl_node *cn ) {
	
	dbgf( DBGL_CHANGES, DBGT_INFO, "%s p:%s c:%s", opt->long_name, p_val, c_val );
	
	char sect_name[MAX_ARG_SIZE]="";
	
	paranoia( -500102, !opt );
	
	if ( !bmx_ctx  ||  !bmx_conf_name  ||  opt->cfg_t == A_ARG  )
		return SUCCESS;
	
	if ( opt->opt_t == A_PS1 ) {
		
		// for all general options like ogm_interval, dad_timeout, ...
		
		if ( del ) {
			
			return uci_remove( bmx_ctx, bmx_conf_name, DEF_SECT_NAME, opt->long_name, cn );
		
		} else {
			
			uci_get_sect_name( YES/*create*/, cn, bmx_ctx, bmx_conf_name, DEF_SECT_NAME, DEF_SECT_TYPE, NULL, NULL );
			return uci_save_option( bmx_ctx, bmx_conf_name, DEF_SECT_NAME, opt->long_name, c_val, cn );
		}
		
	} else if ( opt->opt_t == A_PMN ) {
		
		// all A_PMN-options are saved as sections 
		// some with only one argument like HNAs, throw-rule, plugin, service
		// section->options are processed in the following block 
		
		if ( uci_get_sect_name( ( del ? NO : YES/*create*/ ),
		                        cn, bmx_ctx, 
		                        bmx_conf_name, sect_name, opt->long_name, 
		                        opt->long_name, p_val ) == FAILURE )
		{
			
			dbgf_cn( cn, DBGL_SYS, DBGT_ERR, "unknown sect_name %s for %s %s",
			        sect_name, opt->long_name, p_val );
			
			if ( del )
				return SUCCESS;
			
			return FAILURE;
		}
		
		if ( del )
			return uci_remove( bmx_ctx, bmx_conf_name, sect_name, NULL, cn );
		
		else
			return uci_save_option( bmx_ctx, bmx_conf_name, sect_name, opt->long_name, c_val, cn );
		
		
	} else if ( opt->opt_t == A_CS1  &&  p_val ) {
	
		// all A_CS1-child options  like \ttl=20  from --dev eth0
		
		if ( uci_get_sect_name( NO/*create*/, cn, bmx_ctx, 
		                        bmx_conf_name, sect_name, opt->d.parent_opt->long_name, 
		                        opt->d.parent_opt->long_name, p_val ) == FAILURE ) 
		{
			
			dbgf_cn( cn, DBGL_SYS, DBGT_ERR, 
			        "unknown A_1 sect_name %s  sn %s %s  on %s %s",
			        sect_name, opt->d.parent_opt->long_name, p_val, opt->long_name, c_val );
			
			if ( del )
				return SUCCESS;
			
			sect_name[0]=0;
			if ( uci_get_sect_name( YES/*create*/, cn, bmx_ctx,
			                        bmx_conf_name, sect_name, opt->d.parent_opt->long_name,
			                        opt->d.parent_opt->long_name, p_val ) == FAILURE )
				return FAILURE;
			
		}
		
		if ( del )
			return uci_remove( bmx_ctx, bmx_conf_name, sect_name, opt->long_name, cn );
		
		else
			return uci_save_option( bmx_ctx, bmx_conf_name, sect_name, opt->long_name, c_val, cn );
		
		
	} else {
		
		dbgf_cn( cn, DBGL_SYS, DBGT_ERR, "%20s %20s -- %20s %s",
		        opt->d.parent_opt?opt->d.parent_opt->long_name:"--", p_val, opt->long_name, c_val  );
		
		cleanup_all( -501004 );
	}
	
	return SUCCESS;
}


STATIC_FUNC
int bmx_load_config ( uint8_t cmd, struct opt_type *opt, struct ctrl_node *cn ) {
	
	char name[MAX_PATH_SIZE]="";
	struct uci_ptr sptr, optr;
	
	if ( !bmx_ctx  ||  !bmx_conf_name )
		return SUCCESS;
		
	if ( !opt->long_name  ||  opt->cfg_t == A_ARG  )
		return SUCCESS;
	
	paranoia( -500138, ( cmd != OPT_CHECK  &&  cmd != OPT_APPLY ) );
	
	
	if ( opt->opt_t == A_PS1 ) {
		
		sprintf( name, "%s.%s.%s", bmx_conf_name, DEF_SECT_NAME, opt->long_name );
		
		dbgf_all( DBGT_INFO, "loading A_PS1-option: %s", name );
		
		if ( !( uci_lookup( bmx_ctx, &optr, name ) ) ) {
			
			if ( !initializing  &&  //no need to reset a configuration during init
			     check_apply_parent_option( DEL, cmd, NO/*save*/, opt, 0, cn ) == FAILURE ) 
			{
				dbgf_cn( cn, DBGL_SYS, DBGT_ERR, 
				     "resetting A_PS1 %s.%s.%s to defaults failed", 
				        bmx_conf_name, DEF_SECT_NAME, opt->long_name );
				
				return FAILURE;
			}
		
		} else if ( !optr.o || !optr.o->v.string ) {
			
			return FAILURE;
		
		} else if ( check_apply_parent_option( ADD, cmd, NO/*save*/, opt, optr.o->v.string, cn ) == FAILURE ) {
		
			dbgf_cn( cn, DBGL_SYS, DBGT_ERR, "loading A_PS1 %s.%s.%s=%s failed", 
			        bmx_conf_name, DEF_SECT_NAME, opt->long_name, optr.o->v.string );
			
			return FAILURE;
		}
		
		
	} else if ( opt->opt_t == A_PMN ) {
		
		// For A_M (multiple) and A_N (N-suboptons) we use a section)
		
		dbgf_all( DBGT_INFO, "loading A_PMN-option: %s", opt->long_name );
		
		struct uci_element *e;
		struct uci_element *se;
		struct opt_parent *p_tmp;
		
		struct list_node *pos;
		
		if ( !(e=uci_lookup( bmx_ctx, &sptr, bmx_conf_name )) )
			return SUCCESS;
		
		if ( e->type != UCI_TYPE_PACKAGE )
			return SUCCESS;
		
		// temporary cache all currently configured parents/sections
		// so that we can later reset all of them which were not reloaded
		del_opt_parent( &tmp_conf_opt, NULL );
		list_for_each( pos, &(opt->d.parents_instance_list) ) {
			p_tmp = list_entry( pos, struct opt_parent, list );
			
			struct opt_parent *p_dup = add_opt_parent(&tmp_conf_opt);
			set_opt_parent_val ( p_dup, p_tmp->p_val );
			set_opt_parent_ref ( p_dup, p_tmp->p_ref );
		}
		
		uci_foreach_element( &(sptr.p->sections), se) {
			
			struct uci_section *s = uci_to_section(se);
			
			if ( strcmp( opt->long_name, s->type ) )
				continue;
			
			sprintf( name, "%s.%s.%s", bmx_conf_name, s->e.name, opt->long_name );
			
			dbgf_all( DBGT_INFO, "looking up: %s", name );
			
			
			if ( !(e=uci_lookup( bmx_ctx, &optr, name )) ) {
				
				if ( cmd == OPT_APPLY ) {
					dbgf_cn( cn, DBGL_SYS, DBGT_WARN, 
					        "looking up %s.%s.%s failed", 
					        bmx_conf_name, s->e.name, opt->long_name );
				}
				
				continue;
			}
			
			char config_sect_val[MAX_ARG_SIZE];
			strcpy( config_sect_val, optr.o->v.string );
			
			
			struct opt_parent *patch = add_opt_parent( &Patch_opt );
			
			if ( call_option( ADD, OPT_PATCH, NO/*save*/, opt, patch, config_sect_val, cn ) == FAILURE ) {
				
				dbgf_cn( cn, DBGL_SYS, DBGT_ERR, "setting sect %s.%s.%s=%s failed", 
				        bmx_conf_name, s->e.name, opt->long_name, config_sect_val );
				
				del_opt_parent( &Patch_opt, patch );
				return FAILURE;
			}
			
			
			list_for_each( pos, &opt->d.childs_type_list ) {
				
				struct opt_type *opt_arg = (struct opt_type*)list_entry( pos, struct opt_data, list );
				
				sprintf( name, "%s.%s.%s", bmx_conf_name, s->e.name, opt_arg->long_name );
				dbgf_all( DBGT_INFO, "looking up: %s", name );
				
				uint8_t del;
				char config_sect_opt_val[MAX_ARG_SIZE];
				
				if ( (e=uci_lookup( bmx_ctx, &optr, name )) ) {
					
					strcpy( config_sect_opt_val, optr.o->v.string );
					del = ADD;
					
				} else {
					
					if ( initializing )
						continue; //no need to reset a configuration during init
					
					config_sect_opt_val[0] = 0;
					del = DEL;
				}
				
				if ( call_option( del, OPT_PATCH, NO/*save*/, opt_arg, patch, config_sect_opt_val, cn ) == FAILURE ) {
					
					dbgf_cn( cn, DBGL_SYS, DBGT_ERR, 
					        "setting opt %s %s %s.%s.%s=%s failed", 
					        opt->long_name, config_sect_val,
					        bmx_conf_name, s->e.name, opt_arg->long_name, config_sect_opt_val );
					
					del_opt_parent( &Patch_opt, patch );
					return FAILURE;
					
				} else {
					
					dbgf_all( DBGT_INFO, 
					        "patched opt %s %s %s.%s.%s=%s", 
					        opt->long_name, config_sect_val,
					        bmx_conf_name, s->e.name, opt_arg->long_name, config_sect_opt_val );
				}
				
			}
			
			if ( call_option( ADD, OPT_ADJUST, NO/*save*/, opt, patch, config_sect_val, cn ) == FAILURE ||
			     call_option( ADD, cmd,        NO/*save*/, opt, patch, config_sect_val, cn ) == FAILURE ) {
				
				dbgf_cn( cn, DBGL_SYS, DBGT_ERR,
					"configuring section %s.%s=%s failed",
					bmx_conf_name, s->e.name, opt->long_name );
				
				     del_opt_parent( &Patch_opt, patch );
				     return FAILURE;
			     }

			// remove all (re)loaded opts from the cached list. They dont have to be resetted later on
                        if ((p_tmp = get_opt_parent_ref(&tmp_conf_opt, config_sect_val)) ||
                                (p_tmp = get_opt_parent_val(&tmp_conf_opt, patch->p_val)))
                                del_opt_parent(&tmp_conf_opt, p_tmp);

			
			del_opt_parent( &Patch_opt, patch );
		}
		
		// finally we have to reset all options which were configured previously  but not reloaded
		list_for_each( pos, &tmp_conf_opt.d.parents_instance_list ) {
			
			p_tmp = list_entry( pos, struct opt_parent, list );
			
			if ( wordsEqual( p_tmp->p_val, BMX_LIB_CONFIG ) ) {
				
				dbg_mute( 40, DBGL_SYS, DBGT_WARN, "missing section %s with option %s %s in %s",
				          ARG_PLUGIN, ARG_PLUGIN, BMX_LIB_CONFIG, bmx_conf_name );
				
			} else if ( check_apply_parent_option( DEL, cmd, NO, opt, p_tmp->p_val, cn ) == FAILURE ) {
				
				dbgf_cn( cn, DBGL_SYS, DBGT_ERR, "calling %s %s failed", opt->long_name, p_tmp->p_val );
				
				return FAILURE;
			}
		}
		
	} else {

                dbgf_cn(cn, DBGL_SYS, DBGT_ERR, "opt: %s illegal implementation!", opt->long_name);
		
		cleanup_all( -500137 );
		
	}
	
	return SUCCESS;
}



STATIC_FUNC
int32_t opt_conf_reload ( uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn ) {
	
	if ( cmd == OPT_CHECK  || cmd == OPT_APPLY ) {
		
		Load_config = 1;
		
		if ( cmd == OPT_CHECK  &&  bmx_ctx ) {
			
			// without doing this we get double free or corruption after config-reload or uci_remove()
			uci_reload_package( bmx_ctx, &bmx_pptr, bmx_conf_name );
		}
	}
	
	return SUCCESS;
}



STATIC_FUNC
int32_t opt_conf_file ( uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn ) {

	char tmp_path[MAX_PATH_SIZE] = "";
	
	if ( !initializing )
		return SUCCESS;
	
	
	if ( cmd == OPT_REGISTER ) {
		
		sprintf( conf_path, "%s/%s", UCI_CONFDIR, DEF_CONF_NAME );
		bmx_conf_name = strrchr( conf_path, '/') + 1;
		*(strrchr( conf_path, '/'))=0;
		
		
	} else if ( cmd == OPT_CHECK  ||  cmd == OPT_APPLY ) {
	
		if ( patch->p_diff == DEL )
			return FAILURE;
		
		char *f = patch->p_val;
		
		if ( wordlen(f)+1 +strlen(UCI_CONFDIR)+1 >= MAX_PATH_SIZE )
			return FAILURE;
		
		if ( wordsEqual( f, ARG_NO_CONFIG_FILE ) ) {
			
			if ( cmd == OPT_APPLY )
				bmx_conf_name = NULL;
			
			return SUCCESS;
		
		} else if ( f[0] == '/' ) {
			
			wordCopy( tmp_path, f );
			
			char *tmp_name = strrchr( tmp_path, '/');
			
			if ( !tmp_name  ||  tmp_name == tmp_path  ||  
			     check_file( tmp_path, YES/*writable*/, NO/*executable*/ ) == FAILURE )
				return FAILURE;
			
		} else if ( strchr( f, '/') == NULL  ||  strchr( f, '/') >= f+wordlen(f) ) {
			
			snprintf( tmp_path, strlen(UCI_CONFDIR)+1+wordlen(f)+1, "%s/%s", UCI_CONFDIR, f );
			
			if ( check_file( tmp_path, YES/*writable*/, NO/*executable*/ ) == FAILURE )
				return FAILURE;
				
			
		} else {
			
			return FAILURE;
		}
		
		if ( cmd == OPT_APPLY ) {
			
			strcpy( conf_path, tmp_path );
			bmx_conf_name = strrchr( conf_path, '/') + 1;
			*(strrchr( conf_path, '/'))=0;
			
		}
		
		return SUCCESS;
		
		
	} else if ( cmd == OPT_SET_POST  &&  bmx_conf_name ) {
		
		sprintf( tmp_path, "%s/%s", conf_path, bmx_conf_name );
		
		if ( check_file( tmp_path, YES/*writable*/, NO/*executable*/ ) == FAILURE )
			return SUCCESS; //no config file used

		bmx_ctx = uci_alloc_context();
		uci_set_confdir( bmx_ctx, conf_path );
		
		dbg( DBGL_CHANGES, DBGT_INFO, 
		     "loading uci bmx6 backend: file://%s/%s succeeded", conf_path, bmx_conf_name );
		
		//initially lookup the bmx package so that we can save future changes
		memset(&bmx_pptr, 0, sizeof(bmx_pptr));
		bmx_pptr.package = bmx_conf_name;
		uci_lookup_ptr( bmx_ctx, &bmx_pptr, NULL, false);
		
		net_ctx = uci_alloc_context();
		uci_set_confdir( net_ctx, conf_path );
		
		load_config_cb = bmx_load_config;
		save_config_cb = bmx_save_config;
		derive_config = bmx_derive_config;
		
		// we are already at OPT_SET_POST order>1 but 
		// we have nothing OPT_TESTed nor OPT_SET order=0 options, so load it now!
		// order > 1 will be OPT_TEST and OPT_SET automatically via load_config_cb = bmx_load_config function
		struct list_node *list_pos;
		
		int8_t test = 1;
		while ( test >= 0 && test <= 1 ) {
		
			list_for_each( list_pos, &opt_list ) {
				
				struct opt_type *on = (struct opt_type*)list_entry( list_pos, struct opt_data, list );
				
				if ( (test && on->order != 1 ) || (!test && on->order == 0) ) {
					
					if ( bmx_load_config( test?OPT_CHECK:OPT_APPLY, on, cn ) != SUCCESS ) {
						
						dbgf_all(  DBGT_ERR, 
							"bmx_load_config() %s %s failed", 
						        test?"OPT_TEST":"OPT_SET",on->long_name );
						
						return FAILURE;
					}
				}
			}
			test--;
		}
	}
	
	signal( SIGHUP, signal_hup_handler );

	return SUCCESS;
}

static uint8_t show_conf_general = YES;

STATIC_FUNC
int8_t show_conf ( struct ctrl_node *cn, void *data, struct opt_type *opt, struct opt_parent *p, struct opt_child *c ) {
	
	if ( show_conf_general  &&  !c  &&  opt->opt_t == A_PS1 ) {
		
		dbg_printf( cn, "\toption '%s' '%s'\n", opt->long_name, (p->p_ref ? p->p_ref : p->p_val) );
		
	} else if ( !show_conf_general  &&  !c  &&  opt->opt_t == A_PMN ) {
		
		dbg_printf( cn, "\nconfig '%s'\n", opt->long_name );
		dbg_printf( cn, "\toption '%s' '%s'\n", opt->long_name, (p->p_ref ? p->p_ref : p->p_val) );
		
	} else if ( !show_conf_general  &&  c  &&  c->c_opt->opt_t == A_CS1 ) {
		
		dbg_printf( cn, "\toption '%s' '%s'\n", c->c_opt->long_name, (c->c_ref ? c->c_ref : c->c_val) );
		
	}
	
	return SUCCESS;
}



STATIC_FUNC
int32_t opt_show_conf ( uint8_t cmd, uint8_t _save, struct opt_type *opt, struct opt_parent *patch, struct ctrl_node *cn ) {
	
	if ( cmd == OPT_APPLY ) {
		
		dbg_printf( cn, "config '%s' '%s'\n", DEF_SECT_TYPE, DEF_SECT_NAME );
		
		show_conf_general = YES;
		func_for_each_opt( cn, NULL, "show_conf()", show_conf );
		show_conf_general = NO;
		func_for_each_opt( cn, NULL, "show_conf()", show_conf );
		
		dbg_printf( cn, "\n" );
		
	}	
	
	return SUCCESS;
}

static struct opt_type config_options[]= {
//        ord parent long_name          shrt Attributes				*ival		min		max		default		*func,*syntax,*help
	
	{ODI,0,0,			0,  5,0,0,0,0,0,			0,		0,		0,		0,		0,
			0,		"\nUCI config options:"},
		
		
	{ODI,0,ARG_CONFIG_FILE,	        'f',1,A_PS1,A_ADM,A_INI,A_ARG,A_ANY,	0,		0, 		0,		0, 		opt_conf_file,
			ARG_FILE_FORM,	"use non-default config file. If defined, this must be the first given option.\n"
                        "	use --" ARG_CONFIG_FILE "=" ARG_NO_CONFIG_FILE " or -f" ARG_NO_CONFIG_FILE " to disable"},
	
	{ODI,0,ARG_RELOAD_CONFIG,	0,  1,A_PS0,A_ADM,A_DYN,A_ARG,A_ANY,	0,		0, 		0,		0, 		opt_conf_reload,
			0,		"dynamically reload config file"},
	
	{ODI,0,ARG_SHOW_CONFIG,	        0,  5,A_PS0,A_ADM,A_DYN,A_ARG,A_ANY,	0,		0, 		0,		0, 		opt_show_conf,
			0,		"show current config as it could be saved to " ARG_CONFIG_FILE }
};



STATIC_FUNC
void cleanup_conf( void ) {
	
	del_opt_parent( &tmp_conf_opt, NULL );
	
	load_config_cb = NULL;
	save_config_cb = NULL;
	
	if ( bmx_ctx )
		uci_free_context( bmx_ctx );

	if ( net_ctx )
		uci_free_context( net_ctx );
	
}

STATIC_FUNC
int32_t init_conf( void ) {

	memset( &tmp_conf_opt, 0, sizeof( struct opt_type ) );
	LIST_INIT_HEAD( tmp_conf_opt.d.childs_type_list, struct opt_data, list );
	LIST_INIT_HEAD( tmp_conf_opt.d.parents_instance_list, struct opt_parent, list );
	
	register_options_array( config_options, sizeof( config_options ) );
	
	return SUCCESS;
}



struct plugin* get_plugin( void ) {
	
	static struct plugin conf_plugin;
        memset( &conf_plugin, 0, sizeof( conf_plugin));
	
	conf_plugin.plugin_name = "bmx6_uci_config_plugin";
	conf_plugin.plugin_size = sizeof ( struct plugin );
        conf_plugin.plugin_code_version = CODE_VERSION;
	conf_plugin.cb_init = init_conf;
	conf_plugin.cb_cleanup = cleanup_conf;

	return &conf_plugin;
}

