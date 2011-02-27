/*
 * Copyright (C) 2006-2009 B.A.T.M.A.N. contributors:
 *
 * Simon Wunderlich, Marek Lindner
 *
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
 *
 */



#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "os.h"
#include "batman.h"
#include "originator.h"
#include "hna.h"
#include "types.h"

struct neigh_node * create_neighbor(struct orig_node *orig_node, struct orig_node *orig_neigh_node, uint32_t neigh, struct batman_if *if_incoming) {

	struct neigh_node *neigh_node;

	debug_output( 4, "Creating new last-hop neighbour of originator\n" );

	neigh_node = debugMalloc( sizeof(struct neigh_node), 403 );
	memset( neigh_node, 0, sizeof(struct neigh_node) );
	INIT_LIST_HEAD(&neigh_node->list);

	neigh_node->addr = neigh;
	neigh_node->orig_node = orig_neigh_node;
	neigh_node->if_incoming = if_incoming;

	neigh_node->tq_recv = debugMalloc(sizeof(uint16_t) * global_win_size, 406);
	memset(neigh_node->tq_recv, 0, sizeof(uint16_t) * global_win_size);

	neigh_node->real_bits = debugMalloc(sizeof(TYPE_OF_WORD) * num_words, 407);
	memset(neigh_node->real_bits, 0, sizeof(TYPE_OF_WORD) * num_words);

	list_add_tail(&neigh_node->list, &orig_node->neigh_list);

	return neigh_node;

}

/* needed for hash, compares 2 struct orig_node, but only their ip-addresses. assumes that
 * the ip address is the first field in the struct */
int compare_orig( void *data1, void *data2 )
{
	return (memcmp(data1, data2, 4) == 0 ? 1 : 0);
}



/* hashfunction to choose an entry in a hash table of given size */
/* hash algorithm from http://en.wikipedia.org/wiki/Hash_table */
int choose_orig( void *data, int32_t size ) {

	unsigned char *key= data;
	uint32_t hash = 0;
	size_t i;

	for (i = 0; i < 4; i++) {
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return (hash%size);

}



/* this function finds or creates an originator entry for the given address if it does not exits */
struct orig_node *get_orig_node( uint32_t addr ) {

	struct orig_node *orig_node;
	struct hashtable_t *swaphash;
	char orig_str[ADDR_STR_LEN];
	prof_start( PROF_get_orig_node );


	orig_node = ((struct orig_node *)hash_find( orig_hash, &addr ));

	if ( orig_node != NULL ) {

		prof_stop( PROF_get_orig_node );
		return orig_node;

	}


	addr_to_string( addr, orig_str, ADDR_STR_LEN );
	debug_output( 4, "Creating new originator: %s \n", orig_str );

	orig_node = debugMalloc( sizeof(struct orig_node), 401 );
	memset(orig_node, 0, sizeof(struct orig_node));
	INIT_LIST_HEAD_FIRST( orig_node->neigh_list );

	orig_node->orig = addr;
	orig_node->router = NULL;
	orig_node->batman_if = NULL;

	orig_node->bcast_own = debugMalloc( found_ifs * sizeof(TYPE_OF_WORD) * num_words, 404 );
	memset( orig_node->bcast_own, 0, found_ifs * sizeof(TYPE_OF_WORD) * num_words );

	orig_node->bcast_own_sum = debugMalloc( found_ifs * sizeof(uint8_t), 405 );
	memset( orig_node->bcast_own_sum, 0, found_ifs * sizeof(uint8_t) );

	hash_add( orig_hash, orig_node );

	if ( orig_hash->elements * 4 > orig_hash->size ) {

		swaphash = hash_resize( orig_hash, orig_hash->size * 2 );

		if ( swaphash == NULL ) {

			debug_output( 0, "Couldn't resize hash table \n" );
			restore_and_exit(0);

		}

		orig_hash = swaphash;

	}

	prof_stop( PROF_get_orig_node );
	return orig_node;

}



void update_orig(struct orig_node *orig_node, struct bat_packet *in, uint32_t neigh, struct batman_if *if_incoming, unsigned char *hna_recv_buff, int16_t hna_buff_len, uint8_t is_duplicate, uint32_t curr_time)
{
	struct list_head *list_pos;
	struct gw_node *gw_node;
	struct neigh_node *neigh_node = NULL, *tmp_neigh_node = NULL;
	prof_start(PROF_update_originator);

	debug_output(4, "update_originator(): Searching and updating originator entry of received packet,  \n");

	list_for_each(list_pos, &orig_node->neigh_list) {
		tmp_neigh_node = list_entry(list_pos, struct neigh_node, list);

		if ((tmp_neigh_node->addr == neigh ) && (tmp_neigh_node->if_incoming == if_incoming)) {
			neigh_node = tmp_neigh_node;
			continue;
		}

		if (is_duplicate)
			continue;

		ring_buffer_set(tmp_neigh_node->tq_recv, &tmp_neigh_node->tq_index, 0);
		tmp_neigh_node->tq_avg = ring_buffer_avg(tmp_neigh_node->tq_recv);
	}

	if (!neigh_node)
		neigh_node = create_neighbor(orig_node, get_orig_node(neigh), neigh, if_incoming);
	else
		debug_output(4, "Updating existing last-hop neighbour of originator\n");

	neigh_node->last_valid = curr_time;

	ring_buffer_set(neigh_node->tq_recv, &neigh_node->tq_index, in->tq);
	neigh_node->tq_avg = ring_buffer_avg(neigh_node->tq_recv);

	if (!is_duplicate) {
		orig_node->last_ttl = in->ttl;
		neigh_node->last_ttl = in->ttl;
	}

	/**
	 * if we got have a better tq value via this neighbour or
	 * same tq value but the link is more symetric change the next hop
	 * router
	 */
	if ((orig_node->router != neigh_node) && ((!orig_node->router) ||
	    (neigh_node->tq_avg > orig_node->router->tq_avg) ||
	    ((neigh_node->tq_avg == orig_node->router->tq_avg) &&
	     (neigh_node->orig_node->bcast_own_sum[if_incoming->if_num] > orig_node->router->orig_node->bcast_own_sum[if_incoming->if_num]))))
		update_routes(orig_node, neigh_node, hna_recv_buff, hna_buff_len);
	else
		update_routes(orig_node, orig_node->router, hna_recv_buff, hna_buff_len);

	if (orig_node->gwflags != in->gwflags)
		update_gw_list(orig_node, in->gwflags, in->gwport);

	orig_node->gwflags = in->gwflags;
	hna_global_check_tq(orig_node);

	/* restart gateway selection if we have more packets and fast or late switching enabled */
	if ((routing_class > 2) && (orig_node->gwflags != 0) && (curr_gateway != NULL)) {

		/* if the node is not our current gateway and
		   we have preferred gateray disabled and a better tq value or we found our preferred gateway */
		if ((curr_gateway->orig_node != orig_node) &&
				   (((pref_gateway == 0) && (orig_node->router->tq_avg > curr_gateway->orig_node->router->tq_avg)) || (pref_gateway == orig_node->orig))) {

			/* it is our preferred gateway or we have fast switching or the tq is $routing_class better than our old tq */
			if ((pref_gateway == orig_node->orig) || (routing_class == 3) || (orig_node->router->tq_avg - curr_gateway->orig_node->router->tq_avg >= routing_class)) {

				gw_node = NULL;

				list_for_each(list_pos, &gw_list) {
					gw_node = list_entry(list_pos, struct gw_node, list);

					if (gw_node->orig_node == orig_node)
						break;

					gw_node = NULL;
				}

				/* if this gateway had not a gateway failure within the last 30 seconds */
				if ((gw_node != NULL) && ((int)(curr_time - (gw_node->last_failure + 30000)) > 0)) {

					debug_output(3, "Gateway client - restart gateway selection: better gateway found (tq curr: %i, tq new: %i) \n", curr_gateway->orig_node->router->tq_avg, orig_node->router->tq_avg);

					del_default_route();

				}

			}

		}

	}

	prof_stop(PROF_update_originator);
}



void purge_orig(uint32_t curr_time)
{
	struct hash_it_t *hashit = NULL;
	struct list_head *neigh_pos, *neigh_temp, *prev_list_head;
	struct list_head *gw_pos, *gw_pos_tmp;
	struct orig_node *orig_node;
	struct neigh_node *neigh_node, *best_neigh_node;
	struct gw_node *gw_node;
	uint8_t gw_purged = 0, neigh_purged, max_tq;
	char orig_str[ADDR_STR_LEN], neigh_str[ADDR_STR_LEN];
	prof_start( PROF_purge_originator );


	/* for all origins... */
	while ( NULL != ( hashit = hash_iterate( orig_hash, hashit ) ) ) {

		orig_node = hashit->bucket->data;

		if ((int)(curr_time - (orig_node->last_valid + (2 * purge_timeout))) > 0) {

			addr_to_string( orig_node->orig, orig_str, ADDR_STR_LEN );
			debug_output( 4, "Originator timeout: originator %s, last_valid %u \n", orig_str, orig_node->last_valid );

			hash_remove_bucket( orig_hash, hashit );

			/* for all neighbours towards this originator ... */
			list_for_each_safe( neigh_pos, neigh_temp, &orig_node->neigh_list ) {

				neigh_node = list_entry(neigh_pos, struct neigh_node, list);

				list_del((struct list_head *)&orig_node->neigh_list, neigh_pos, &orig_node->neigh_list);
				debugFree(neigh_node->tq_recv, 1407);
				debugFree(neigh_node->real_bits, 1409);
				debugFree(neigh_node, 1401);

			}

			list_for_each( gw_pos, &gw_list ) {

				gw_node = list_entry( gw_pos, struct gw_node, list );

				if ( gw_node->deleted )
					continue;

				if ( gw_node->orig_node == orig_node ) {

					addr_to_string( gw_node->orig_node->orig, orig_str, ADDR_STR_LEN );
					debug_output( 3, "Removing gateway %s from gateway list \n", orig_str );

					gw_node->deleted = get_time_msec();
					gw_purged = 1;

					break;

				}

			}

			update_routes( orig_node, NULL, NULL, 0 );

			debugFree( orig_node->bcast_own, 1403 );
			debugFree( orig_node->bcast_own_sum, 1404 );
			debugFree( orig_node, 1405 );

		} else {

			best_neigh_node = NULL;
			max_tq = neigh_purged = 0;
			prev_list_head = (struct list_head *)&orig_node->neigh_list;

			/* for all neighbours towards this originator ... */
			list_for_each_safe( neigh_pos, neigh_temp, &orig_node->neigh_list ) {

				neigh_node = list_entry( neigh_pos, struct neigh_node, list );

				if ((int)(curr_time - (neigh_node->last_valid + purge_timeout)) > 0) {

					addr_to_string( orig_node->orig, orig_str, ADDR_STR_LEN );
					addr_to_string( neigh_node->addr, neigh_str, ADDR_STR_LEN );
					debug_output( 4, "Neighbour timeout: originator %s, neighbour: %s, last_valid %u \n", orig_str, neigh_str, neigh_node->last_valid );

					if (orig_node->router == neigh_node) {

						/* we have to delete the route towards this node before it gets purged */
						debug_output( 4, "Deleting previous route \n" );

						/* remove old announced network(s) */
						hna_global_del(orig_node);

						add_del_route(orig_node->orig, 32, orig_node->router->addr, 0, orig_node->batman_if->if_index, orig_node->batman_if->dev, BATMAN_RT_TABLE_HOSTS, ROUTE_TYPE_UNICAST, ROUTE_DEL);

						/* if the neighbour is the route towards our gateway */
						if ((curr_gateway != NULL) && (curr_gateway->orig_node == orig_node))
							del_default_route();

						orig_node->router = NULL;

					}

					neigh_purged = 1;
					list_del(prev_list_head, neigh_pos, &orig_node->neigh_list);
					debugFree(neigh_node->tq_recv, 1408);
					debugFree(neigh_node->real_bits, 1410);
					debugFree(neigh_node, 1406);

				} else {

					if ((best_neigh_node == NULL) || (neigh_node->tq_avg > max_tq)) {
						best_neigh_node = neigh_node;
						max_tq = neigh_node->tq_avg;
					}

					prev_list_head = &neigh_node->list;

				}

			}

			if ((neigh_purged) && ((best_neigh_node == NULL) || (orig_node->router == NULL) || (max_tq > orig_node->router->tq_avg)))
				update_routes( orig_node, best_neigh_node, orig_node->hna_buff, orig_node->hna_buff_len );

		}

	}


	prev_list_head = (struct list_head *)&gw_list;

	list_for_each_safe( gw_pos, gw_pos_tmp, &gw_list ) {

		gw_node = list_entry(gw_pos, struct gw_node, list);

		if ((gw_node->deleted) && ((int)(curr_time - (gw_node->deleted + (2 * purge_timeout))) > 0)) {

			list_del( prev_list_head, gw_pos, &gw_list );
			debugFree( gw_pos, 1406 );

		} else {

			prev_list_head = &gw_node->list;

		}

	}

	prof_stop( PROF_purge_originator );

	if ( gw_purged )
		choose_gw();

}



void debug_orig(void) {

	struct hash_it_t *hashit = NULL;
	struct list_head *forw_pos, *orig_pos, *neigh_pos;
	struct forw_node *forw_node;
	struct orig_node *orig_node;
	struct neigh_node *neigh_node;
	struct gw_node *gw_node;
	uint16_t batman_count = 0;
	uint64_t uptime_sec;
	int download_speed, upload_speed, debug_out_size;
	char str[ADDR_STR_LEN], str2[ADDR_STR_LEN], orig_str[ADDR_STR_LEN], debug_out_str[1001];


	if ( debug_clients.clients_num[1] > 0 ) {

		addr_to_string( ((struct batman_if *)if_list.next)->addr.sin_addr.s_addr, orig_str, sizeof(orig_str) );
		uptime_sec = (uint64_t)(get_time_msec64() / 1000);

		debug_output(2, "BOD\n");
		debug_output(2, "%''12s     (%s/%i) %''15s [%10s], gw_class ... [B.A.T.M.A.N. %s%s, MainIF/IP: %s/%s, UT: %id%2ih%2im] \n", "Gateway", "#", TQ_MAX_VALUE, "Nexthop", "outgoingIF", SOURCE_VERSION, (strlen(REVISION_VERSION) > 3 ? REVISION_VERSION : ""), ((struct batman_if *)if_list.next)->dev, orig_str, (uint32_t)(uptime_sec/86400), (uint32_t)((uptime_sec%86400)/3600), (uint32_t)((uptime_sec%3600)/60));

		if ( list_empty( &gw_list ) ) {

			debug_output( 2, "No gateways in range ... \n" );

		} else {

			list_for_each( orig_pos, &gw_list ) {

				gw_node = list_entry( orig_pos, struct gw_node, list );

				if ( gw_node->deleted )
					continue;

				if (gw_node->orig_node->router == NULL)
					continue;

				addr_to_string( gw_node->orig_node->orig, str, sizeof (str) );
				addr_to_string( gw_node->orig_node->router->addr, str2, sizeof (str2) );

				get_gw_speeds( gw_node->orig_node->gwflags, &download_speed, &upload_speed );

				debug_output(2, "%s %-15s (%3i) %''15s [%10s], gw_class %3i - %i%s/%i%s, gateway failures: %i \n", ( curr_gateway == gw_node ? "=>" : "  " ), str, gw_node->orig_node->router->tq_avg, str2, gw_node->orig_node->router->if_incoming->dev, gw_node->orig_node->gwflags, (download_speed > 2048 ? download_speed / 1024 : download_speed), (download_speed > 2048 ? "MBit" : "KBit"), (upload_speed > 2048 ? upload_speed / 1024 : upload_speed), (upload_speed > 2048 ? "MBit" : "KBit"), gw_node->gw_failure);

				batman_count++;

			}

			if ( batman_count == 0 )
				debug_output( 2, "No gateways in range ... \n" );

		}

		debug_output( 2, "EOD\n" );

	}

	if ( ( debug_clients.clients_num[0] > 0 ) || ( debug_clients.clients_num[3] > 0 ) ) {

		addr_to_string( ((struct batman_if *)if_list.next)->addr.sin_addr.s_addr, orig_str, sizeof(orig_str) );
		uptime_sec = (uint64_t)(get_time_msec64() / 1000);

		debug_output(1, "BOD \n");
		debug_output(1, "  %-11s (%s/%i) %''15s [%10s]: %''20s ... [B.A.T.M.A.N. %s%s, MainIF/IP: %s/%s, UT: %id%2ih%2im] \n", "Originator", "#", TQ_MAX_VALUE, "Nexthop", "outgoingIF", "Potential nexthops", SOURCE_VERSION, (strlen(REVISION_VERSION) > 3 ? REVISION_VERSION : ""), ((struct batman_if *)if_list.next)->dev, orig_str, (uint32_t)(uptime_sec/86400), (uint32_t)((uptime_sec%86400)/3600), (uint32_t)((uptime_sec%3600)/60));

		if ( debug_clients.clients_num[3] > 0 ) {

			debug_output( 4, "------------------ DEBUG ------------------ \n" );
			debug_output( 4, "Forward list \n" );

			list_for_each( forw_pos, &forw_list ) {
				forw_node = list_entry( forw_pos, struct forw_node, list );
				addr_to_string( ((struct bat_packet *)forw_node->pack_buff)->orig, str, sizeof(str) );
				debug_output( 4, "    %s at %u \n", str, forw_node->send_time );
			}

			debug_output( 4, "Originator list \n" );
			debug_output( 4, "  %-11s (%s/%i) %''15s [%10s]: %''20s\n", "Originator", "#", TQ_MAX_VALUE, "Nexthop", "outgoingIF", "Potential nexthops" );

		}

		while ( NULL != ( hashit = hash_iterate( orig_hash, hashit ) ) ) {

			orig_node = hashit->bucket->data;

			if ( orig_node->router == NULL )
				continue;

			batman_count++;

			addr_to_string( orig_node->orig, str, sizeof (str) );
			addr_to_string( orig_node->router->addr, str2, sizeof (str2) );

			debug_output( 1, "%-15s (%3i) %''15s [%10s]:", str, orig_node->router->tq_avg, str2, orig_node->router->if_incoming->dev );
			debug_output( 4, "%''15s (%3i) %''15s [%10s], last_valid: %u: \n", str, orig_node->router->tq_avg, str2, orig_node->router->if_incoming->dev, orig_node->last_valid );

			debug_out_size = 0;

			list_for_each( neigh_pos, &orig_node->neigh_list ) {

				neigh_node = list_entry( neigh_pos, struct neigh_node, list );

				addr_to_string( neigh_node->addr, str, sizeof (str) );

				debug_out_size = debug_out_size + snprintf( ( debug_out_str + debug_out_size ), ( sizeof(debug_out_str) - 1 - debug_out_size ), " %15s (%3i)", str, neigh_node->tq_avg);

				if ( (unsigned int)(debug_out_size + 30) > sizeof(debug_out_str) - 1 ) {

					debug_output( 1, "%s \n", debug_out_str );
					debug_output( 4, "%s \n", debug_out_str );

					debug_out_size = 0;

				}

			}

			if (debug_out_size > 0) {
				debug_output( 1, "%s \n", debug_out_str );
				debug_output( 4, "%s \n", debug_out_str );
			}

		}

		if ( batman_count == 0 ) {

			debug_output( 1, "No batman nodes in range ... \n" );
			debug_output( 4, "No batman nodes in range ... \n" );

		}

		debug_output( 1, "EOD\n" );
		debug_output( 4, "---------------------------------------------- END DEBUG \n" );

	}

}


