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



#include "hna.h"
#include "os.h"
#include "hash.h"

#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>


unsigned char *hna_buff_local = NULL;
uint8_t num_hna_local = 0;

struct list_head_first hna_list;
struct list_head_first hna_chg_list;

static pthread_mutex_t hna_chg_list_mutex;
static struct hashtable_t *hna_global_hash = NULL;

int compare_hna(void *data1, void *data2)
{
	return (memcmp(data1, data2, 5) == 0 ? 1 : 0);
}

int choose_hna(void *data, int32_t size)
{
	unsigned char *key= data;
	uint32_t hash = 0;
	size_t i;

	for (i = 0; i < 5; i++) {
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return (hash % size);
}

void hna_init(void)
{
	/* hna local */
	INIT_LIST_HEAD_FIRST(hna_list);
	INIT_LIST_HEAD_FIRST(hna_chg_list);

	pthread_mutex_init(&hna_chg_list_mutex, NULL);

	/* hna global */
	hna_global_hash = hash_new(128, compare_hna, choose_hna);

	if (hna_global_hash == NULL) {
		printf("Error - Could not create hna_global_hash (out of memory?)\n");
		exit(EXIT_FAILURE);
	}
}

/* this function can be called when the daemon starts or at runtime */
void hna_local_task_add_ip(uint32_t ip_addr, uint16_t netmask, uint8_t route_action)
{
	struct hna_task *hna_task;

	hna_task = debugMalloc(sizeof(struct hna_task), 701);
	memset(hna_task, 0, sizeof(struct hna_task));
	INIT_LIST_HEAD(&hna_task->list);

	hna_task->addr = ip_addr;
	hna_task->netmask = netmask;
	hna_task->route_action = route_action;

	if (pthread_mutex_lock(&hna_chg_list_mutex) != 0)
		debug_output(0, "Error - could not lock hna_chg_list mutex in %s(): %s \n", __func__, strerror(errno));

	list_add_tail(&hna_task->list, &hna_chg_list);

	if (pthread_mutex_unlock(&hna_chg_list_mutex) != 0)
		debug_output(0, "Error - could not unlock hna_chg_list mutex in %s(): %s \n", __func__, strerror(errno));
}

/* this function can be called when the daemon starts or at runtime */
void hna_local_task_add_str(char *hna_string, uint8_t route_action, uint8_t runtime)
{
	struct in_addr tmp_ip_holder;
	uint16_t netmask;
	char *slash_ptr;

	if ((slash_ptr = strchr(hna_string, '/')) == NULL) {

		if (runtime) {
			debug_output(3, "Invalid announced network (netmask is missing): %s\n", hna_string);
			return;
		}

		printf("Invalid announced network (netmask is missing): %s\n", hna_string);
		exit(EXIT_FAILURE);
	}

	*slash_ptr = '\0';

	if (inet_pton(AF_INET, hna_string, &tmp_ip_holder) < 1) {

		*slash_ptr = '/';

		if (runtime) {
			debug_output(3, "Invalid announced network (IP is invalid): %s\n", hna_string);
			return;
		}

		printf("Invalid announced network (IP is invalid): %s\n", hna_string);
		exit(EXIT_FAILURE);
	}

	errno = 0;
	netmask = strtol(slash_ptr + 1, NULL, 10);

	if ((errno == ERANGE) || (errno != 0 && netmask == 0)) {

		*slash_ptr = '/';

		if (runtime)
			return;

		perror("strtol");
		exit(EXIT_FAILURE);
	}

	if (netmask < 1 || netmask > 32) {

		*slash_ptr = '/';

		if (runtime) {
			debug_output(3, "Invalid announced network (netmask is invalid): %s\n", hna_string);
			return;
		}

		printf("Invalid announced network (netmask is invalid): %s\n", hna_string);
		exit(EXIT_FAILURE);
	}

	*slash_ptr = '/';

	tmp_ip_holder.s_addr = (tmp_ip_holder.s_addr & htonl(0xFFFFFFFF << (32 - netmask)));
	hna_local_task_add_ip(tmp_ip_holder.s_addr, netmask, route_action);
}

static void hna_local_buffer_fill(void)
{
	struct hna_local_entry *hna_local_entry;

	if (hna_buff_local != NULL)
		debugFree(hna_buff_local, 1701);

	num_hna_local = 0;
	hna_buff_local = NULL;

	if (list_empty(&hna_list))
		return;

	list_for_each_entry(hna_local_entry, &hna_list, list) {
		hna_buff_local = debugRealloc(hna_buff_local, (num_hna_local + 1) * 5 * sizeof(unsigned char), 15);

		memmove(&hna_buff_local[num_hna_local * 5], (unsigned char *)&hna_local_entry->addr, 4);
		hna_buff_local[(num_hna_local * 5) + 4] = (unsigned char)hna_local_entry->netmask;
		num_hna_local++;
	}
}

void hna_local_task_exec(void)
{
	struct list_head *list_pos, *list_pos_tmp, *prev_list_head;
	struct list_head *hna_pos, *hna_pos_tmp;
	struct hna_task *hna_task;
	struct hna_local_entry *hna_local_entry;
	char hna_addr_str[ADDR_STR_LEN];

	if (pthread_mutex_trylock(&hna_chg_list_mutex) != 0)
		return;

	if (list_empty(&hna_chg_list))
		goto unlock_chg_list;

	list_for_each_safe(list_pos, list_pos_tmp, &hna_chg_list) {

		hna_task = list_entry(list_pos, struct hna_task, list);
		addr_to_string(hna_task->addr, hna_addr_str, sizeof(hna_addr_str));

		hna_local_entry = NULL;
		prev_list_head = (struct list_head *)&hna_list;

		list_for_each_safe(hna_pos, hna_pos_tmp, &hna_list) {

			hna_local_entry = list_entry(hna_pos, struct hna_local_entry, list);

			if ((hna_task->addr == hna_local_entry->addr) && (hna_task->netmask == hna_local_entry->netmask)) {

				if (hna_task->route_action == ROUTE_DEL) {
					debug_output(3, "Deleting HNA from announce network list: %s/%i\n", hna_addr_str, hna_task->netmask);

					hna_local_update_routes(hna_local_entry, ROUTE_DEL);
					list_del(prev_list_head, hna_pos, &hna_list);
					debugFree(hna_local_entry, 1702);
				} else {
					debug_output(3, "Can't add HNA - already announcing network: %s/%i\n", hna_addr_str, hna_task->netmask);
				}

				break;

			}

			prev_list_head = &hna_local_entry->list;
			hna_local_entry = NULL;

		}

		if (hna_local_entry == NULL) {

			if (hna_task->route_action == ROUTE_ADD) {
				debug_output(3, "Adding HNA to announce network list: %s/%i\n", hna_addr_str, hna_task->netmask);

				/* add node */
				hna_local_entry = debugMalloc(sizeof(struct hna_local_entry), 702);
				memset(hna_local_entry, 0, sizeof(struct hna_local_entry));
				INIT_LIST_HEAD(&hna_local_entry->list);

				hna_local_entry->addr = hna_task->addr;
				hna_local_entry->netmask = hna_task->netmask;

				hna_local_update_routes(hna_local_entry, ROUTE_ADD);
				list_add_tail(&hna_local_entry->list, &hna_list);
			} else {
				debug_output(3, "Can't delete HNA - network is not announced: %s/%i\n", hna_addr_str, hna_task->netmask);
			}

		}

		list_del((struct list_head *)&hna_chg_list, list_pos, &hna_chg_list);
		debugFree(hna_task, 1703);

	}

	/* rewrite local buffer */
	hna_local_buffer_fill();

unlock_chg_list:
	if (pthread_mutex_unlock(&hna_chg_list_mutex) != 0)
		debug_output(0, "Error - could not unlock hna_chg_list mutex in %s(): %s \n", __func__, strerror(errno));
}

unsigned char *hna_local_update_vis_packet(unsigned char *vis_packet, uint16_t *vis_packet_size)
{
	struct hna_local_entry *hna_local_entry;
	struct vis_data *vis_data;

	if (num_hna_local < 1)
		return vis_packet;

	list_for_each_entry(hna_local_entry, &hna_list, list) {
		*vis_packet_size += sizeof(struct vis_data);

		vis_packet = debugRealloc(vis_packet, *vis_packet_size, 107);
		vis_data = (struct vis_data *)(vis_packet + *vis_packet_size - sizeof(struct vis_data));

		memcpy(&vis_data->ip, (unsigned char *)&hna_local_entry->addr, 4);

		vis_data->data = hna_local_entry->netmask;
		vis_data->type = DATA_TYPE_HNA;
	}

	return vis_packet;
}

void hna_local_update_routes(struct hna_local_entry *hna_local_entry, int8_t route_action)
{
	/* add / delete throw routing entries for own hna */
	add_del_route(hna_local_entry->addr, hna_local_entry->netmask, 0, 0, 0, "unknown", BATMAN_RT_TABLE_NETWORKS, ROUTE_TYPE_THROW, route_action);
	add_del_route(hna_local_entry->addr, hna_local_entry->netmask, 0, 0, 0, "unknown", BATMAN_RT_TABLE_HOSTS, ROUTE_TYPE_THROW, route_action);
	add_del_route(hna_local_entry->addr, hna_local_entry->netmask, 0, 0, 0, "unknown", BATMAN_RT_TABLE_UNREACH, ROUTE_TYPE_THROW, route_action);
	add_del_route(hna_local_entry->addr, hna_local_entry->netmask, 0, 0, 0, "unknown", BATMAN_RT_TABLE_TUNNEL, ROUTE_TYPE_THROW, route_action);

	/* do not NAT HNA networks automatically */
	hna_local_update_nat(hna_local_entry->addr, hna_local_entry->netmask, route_action);
}

static void _hna_global_add(struct orig_node *orig_node, struct hna_element *hna_element)
{
	struct hna_global_entry *hna_global_entry;
	struct hna_orig_ptr *hna_orig_ptr = NULL;
	struct orig_node *old_orig_node = NULL;
	struct hashtable_t *swaphash;

	hna_global_entry = ((struct hna_global_entry *)hash_find(hna_global_hash, hna_element));

	/* add the hna node if it does not exist */
	if (!hna_global_entry) {
		hna_global_entry = debugMalloc(sizeof(struct hna_global_entry), 703);

		if (!hna_global_entry)
			return;

		hna_global_entry->addr = hna_element->addr;
		hna_global_entry->netmask = hna_element->netmask;
		hna_global_entry->curr_orig_node = NULL;
		INIT_LIST_HEAD_FIRST(hna_global_entry->orig_list);

		hash_add(hna_global_hash, hna_global_entry);

		if (hna_global_hash->elements * 4 > hna_global_hash->size) {
			swaphash = hash_resize(hna_global_hash, hna_global_hash->size * 2);

			if (swaphash == NULL)
				debug_output(0, "Couldn't resize global hna hash table \n");
			else
				hna_global_hash = swaphash;
		}
	}

	/* the given orig_node already is the current orig node for this HNA */
	if (hna_global_entry->curr_orig_node == orig_node)
		return;

	list_for_each_entry(hna_orig_ptr, &hna_global_entry->orig_list, list) {
		if (hna_orig_ptr->orig_node == orig_node)
			break;

		hna_orig_ptr = NULL;
	}

	/* append the given orig node to the list */
	if (!hna_orig_ptr) {
		hna_orig_ptr = debugMalloc(sizeof(struct hna_orig_ptr), 704);

		if (!hna_orig_ptr)
			return;

		hna_orig_ptr->orig_node = orig_node;
		INIT_LIST_HEAD(&hna_orig_ptr->list);

		list_add_tail(&hna_orig_ptr->list, &hna_global_entry->orig_list);
	}

	/* our TQ value towards the HNA is better */
	if ((!hna_global_entry->curr_orig_node) ||
		(orig_node->router->tq_avg > hna_global_entry->curr_orig_node->router->tq_avg)) {

		old_orig_node = hna_global_entry->curr_orig_node;
		hna_global_entry->curr_orig_node = orig_node;

		/**
		 * if we change the orig node towards the HNA we may still route via the same next hop
		 * which does not require any routing table changes
		 */
		if ((old_orig_node) &&
			(hna_global_entry->curr_orig_node->router->addr == old_orig_node->router->addr))
			return;

		add_del_route(hna_element->addr, hna_element->netmask, orig_node->router->addr,
					orig_node->router->if_incoming->addr.sin_addr.s_addr,
					orig_node->router->if_incoming->if_index,
					orig_node->router->if_incoming->dev,
					BATMAN_RT_TABLE_NETWORKS, ROUTE_TYPE_UNICAST, ROUTE_ADD);
	}

	/* delete previous route */
	if (old_orig_node) {
		add_del_route(hna_element->addr, hna_element->netmask, old_orig_node->router->addr,
					old_orig_node->router->if_incoming->addr.sin_addr.s_addr,
					old_orig_node->router->if_incoming->if_index,
					old_orig_node->router->if_incoming->dev,
					BATMAN_RT_TABLE_NETWORKS, ROUTE_TYPE_UNICAST, ROUTE_DEL);
	}
}

static void _hna_global_del(struct orig_node *orig_node, struct hna_element *hna_element)
{
	struct list_head *list_pos, *list_pos_tmp, *prev_list_head;
	struct hna_global_entry *hna_global_entry;
	struct hna_orig_ptr *hna_orig_ptr = NULL;

	hna_global_entry = ((struct hna_global_entry *)hash_find(hna_global_hash, hna_element));

	if (!hna_global_entry)
		return;

	hna_global_entry->curr_orig_node = NULL;

	prev_list_head = (struct list_head *)&hna_global_entry->orig_list;
	list_for_each_safe(list_pos, list_pos_tmp, &hna_global_entry->orig_list) {
		hna_orig_ptr = list_entry(list_pos, struct hna_orig_ptr, list);

		/* delete old entry in orig list */
		if (hna_orig_ptr->orig_node == orig_node) {
			list_del(prev_list_head, list_pos, &hna_global_entry->orig_list);
			debugFree(hna_orig_ptr, 1707);
			continue;
		}

		/* find best alternative route */
		if ((!hna_global_entry->curr_orig_node) ||
			(hna_orig_ptr->orig_node->router->tq_avg > hna_global_entry->curr_orig_node->router->tq_avg))
			hna_global_entry->curr_orig_node = hna_orig_ptr->orig_node;

		prev_list_head = &hna_orig_ptr->list;
	}

	/* set new route if available */
	if (hna_global_entry->curr_orig_node) {
		/**
		 * if we delete one orig node towards the HNA but we switch to an alternative
		 * which is reachable via the same next hop no routing table changes are necessary
		 */
		if (hna_global_entry->curr_orig_node->router->addr == orig_node->router->addr)
			return;

		add_del_route(hna_element->addr, hna_element->netmask, hna_global_entry->curr_orig_node->router->addr,
					hna_global_entry->curr_orig_node->router->if_incoming->addr.sin_addr.s_addr,
					hna_global_entry->curr_orig_node->router->if_incoming->if_index,
					hna_global_entry->curr_orig_node->router->if_incoming->dev,
					BATMAN_RT_TABLE_NETWORKS, ROUTE_TYPE_UNICAST, ROUTE_ADD);
	}

	add_del_route(hna_element->addr, hna_element->netmask, orig_node->router->addr,
				orig_node->router->if_incoming->addr.sin_addr.s_addr,
				orig_node->router->if_incoming->if_index,
				orig_node->router->if_incoming->dev,
				BATMAN_RT_TABLE_NETWORKS, ROUTE_TYPE_UNICAST, ROUTE_DEL);

	/* if no alternative route is available remove the HNA entry completely */
	if (!hna_global_entry->curr_orig_node) {
		hash_remove(hna_global_hash, hna_element);
		debugFree(hna_global_entry, 1708);
	}
}

/* hna_buff_delete searches in buf if element e is found.
 *
 * if found, delete it from the buf and return 1.
 * if not found, return 0.
 */
static int hna_buff_delete(struct hna_element *buf, int *buf_len, struct hna_element *e)
{
	int i;
	int num_elements;

	if (buf == NULL)
		return 0;

	/* align to multiple of sizeof(struct hna_element) */
	num_elements = *buf_len / sizeof(struct hna_element);

	for (i = 0; i < num_elements; i++) {

		if (memcmp(&buf[i], e, sizeof(struct hna_element)) == 0) {

			/* move last element forward */
			memmove(&buf[i], &buf[num_elements - 1], sizeof(struct hna_element));
			*buf_len -= sizeof(struct hna_element);

			return 1;
		}
	}
	return 0;
}

void hna_global_add(struct orig_node *orig_node, unsigned char *new_hna, int16_t new_hna_len)
{
	struct hna_element *e, *buff;
	int i, num_elements;
	char hna_str[ADDR_STR_LEN];

	if ((new_hna == NULL) || (new_hna_len == 0)) {
		orig_node->hna_buff = NULL;
		orig_node->hna_buff_len = 0;
		return;
	}

	orig_node->hna_buff = debugMalloc(new_hna_len, 705);
	orig_node->hna_buff_len = new_hna_len;
	memcpy(orig_node->hna_buff, new_hna, new_hna_len);

	/* add new routes */
	num_elements = orig_node->hna_buff_len / sizeof(struct hna_element);
	buff = (struct hna_element *)orig_node->hna_buff;

	debug_output(4, "HNA information received (%i HNA network%s): \n", num_elements, (num_elements > 1 ? "s": ""));

	for (i = 0; i < num_elements; i++) {
		e = &buff[i];
		addr_to_string(e->addr, hna_str, sizeof(hna_str));

		if ((e->netmask > 0 ) && (e->netmask < 33))
			debug_output(4, "hna: %s/%i\n", hna_str, e->netmask);
		else
			debug_output(4, "hna: %s/%i -> ignoring (invalid netmask) \n", hna_str, e->netmask);

		if ((e->netmask > 0) && (e->netmask <= 32))
			_hna_global_add(orig_node, e);
	}
}

/**
 * hna_global_update() replaces the old add_del_hna function. This function
 * updates the new hna buffer for the supplied orig node and
 * adds/deletes/updates the announced routes.
 *
 * Instead of first deleting and then adding, we try to add new routes
 * before delting the old ones so that the kernel will not experience
 * a situation where no route is present.
 */
void hna_global_update(struct orig_node *orig_node, unsigned char *new_hna,
				int16_t new_hna_len, struct neigh_node *old_router)
{
	struct hna_element *e, *buff;
	struct hna_global_entry *hna_global_entry;
	int i, num_elements, old_hna_len;
	unsigned char *old_hna;

	/* orig node stopped announcing any networks */
	if ((orig_node->hna_buff) && ((new_hna == NULL) || (new_hna_len == 0))) {
		hna_global_del(orig_node);
		return;
	}

	/* orig node started to announce networks */
	if ((!orig_node->hna_buff) && ((new_hna != NULL) || (new_hna_len != 0))) {
		hna_global_add(orig_node, new_hna, new_hna_len);
		return;
	}

	/**
	 * next hop router changed - no need to change the global hna hash
	 * we just have to make sure that the best orig node is still in place
	 * NOTE: we may miss a changed HNA here which we will update with the next packet
	 */
	if (old_router != orig_node->router) {
		num_elements = orig_node->hna_buff_len / sizeof(struct hna_element);
		buff = (struct hna_element *)orig_node->hna_buff;

		for (i = 0; i < num_elements; i++) {
			e = &buff[i];

			if ((e->netmask < 1) || (e->netmask > 32))
				continue;

			hna_global_entry = ((struct hna_global_entry *)hash_find(hna_global_hash, e));

			if (!hna_global_entry)
				continue;

			/* if the given orig node is not in use no routes need to change */
			if (hna_global_entry->curr_orig_node != orig_node)
				continue;

			add_del_route(e->addr, e->netmask, orig_node->router->addr,
					orig_node->router->if_incoming->addr.sin_addr.s_addr,
					orig_node->router->if_incoming->if_index,
					orig_node->router->if_incoming->dev,
					BATMAN_RT_TABLE_NETWORKS, ROUTE_TYPE_UNICAST, ROUTE_ADD);

			add_del_route(e->addr, e->netmask, old_router->addr,
				old_router->if_incoming->addr.sin_addr.s_addr,
				old_router->if_incoming->if_index,
				old_router->if_incoming->dev,
				BATMAN_RT_TABLE_NETWORKS, ROUTE_TYPE_UNICAST, ROUTE_DEL);
		}

		return;
	}

	/**
	 * check if the buffers even changed. if its still the same, there is no need to
	 * update the routes. if the router changed, then we have to update all the routes
	 * NOTE: no NULL pointer checking here because memcmp() just returns if n == 0
	 */
	if ((orig_node->hna_buff_len == new_hna_len) && (memcmp(orig_node->hna_buff, new_hna, new_hna_len) == 0))
		return;	/* nothing to do */

	/* changed HNA */
	old_hna = orig_node->hna_buff;
	old_hna_len = orig_node->hna_buff_len;

	orig_node->hna_buff = debugMalloc(new_hna_len, 706);
	orig_node->hna_buff_len = new_hna_len;
	memcpy(orig_node->hna_buff, new_hna, new_hna_len);

	/* add new routes and keep old routes */
	num_elements = orig_node->hna_buff_len / sizeof(struct hna_element);
	buff = (struct hna_element *)orig_node->hna_buff;

	for (i = 0; i < num_elements; i++) {
		e = &buff[i];

		/**
		 * if the router is the same, and the announcement was already in the old
		 * buffer, we can keep the route.
		 */
		if (hna_buff_delete((struct hna_element *)old_hna, &old_hna_len, e) == 0) {
			/* not found / deleted, need to add this new route */
			if ((e->netmask > 0) && (e->netmask <= 32))
				_hna_global_add(orig_node, e);
		}
	}

	/* old routes which are not to be kept are deleted now. */
	num_elements = old_hna_len / sizeof(struct hna_element);
	buff = (struct hna_element *)old_hna;

	for (i = 0; i < num_elements; i++) {
		e = &buff[i];

		if ((e->netmask > 0) && (e->netmask <= 32))
			_hna_global_del(orig_node, e);
	}

	/* dispose old hna buffer now. */
	if (old_hna != NULL)
		debugFree(old_hna, 1704);
}

void hna_global_check_tq(struct orig_node *orig_node)
{
	struct hna_element *e, *buff;
	struct hna_global_entry *hna_global_entry;
	int i, num_elements;

	if ((orig_node->hna_buff == NULL) || (orig_node->hna_buff_len == 0))
		return;

	num_elements = orig_node->hna_buff_len / sizeof(struct hna_element);
	buff = (struct hna_element *)orig_node->hna_buff;

	for (i = 0; i < num_elements; i++) {
		e = &buff[i];

		if ((e->netmask < 1) || (e->netmask > 32))
			continue;

		hna_global_entry = ((struct hna_global_entry *)hash_find(hna_global_hash, e));

		if (!hna_global_entry)
			continue;

		/* if the given orig node is not in use no routes need to change */
		if (hna_global_entry->curr_orig_node == orig_node)
			continue;

		/* the TQ value has to better than the currently selected orig node */
		if (hna_global_entry->curr_orig_node->router->tq_avg > orig_node->router->tq_avg)
			continue;

		/**
		 * if we change the orig node towards the HNA we may still route via the same next hop
		 * which does not require any routing table changes
		 */
		if (hna_global_entry->curr_orig_node->router->addr == orig_node->router->addr)
			goto set_orig_node;

		add_del_route(e->addr, e->netmask, orig_node->router->addr,
				orig_node->router->if_incoming->addr.sin_addr.s_addr,
				orig_node->router->if_incoming->if_index,
				orig_node->router->if_incoming->dev,
				BATMAN_RT_TABLE_NETWORKS, ROUTE_TYPE_UNICAST, ROUTE_ADD);

		add_del_route(e->addr, e->netmask, hna_global_entry->curr_orig_node->router->addr,
			hna_global_entry->curr_orig_node->router->if_incoming->addr.sin_addr.s_addr,
			hna_global_entry->curr_orig_node->router->if_incoming->if_index,
			hna_global_entry->curr_orig_node->router->if_incoming->dev,
			BATMAN_RT_TABLE_NETWORKS, ROUTE_TYPE_UNICAST, ROUTE_DEL);

set_orig_node:
		hna_global_entry->curr_orig_node = orig_node;
	}
}

void hna_global_del(struct orig_node *orig_node)
{
	struct hna_element *e, *buff;
	int i, num_elements;

	if ((orig_node->hna_buff == NULL) || (orig_node->hna_buff_len == 0))
		return;

	/* delete routes */
	num_elements = orig_node->hna_buff_len / sizeof(struct hna_element);
	buff = (struct hna_element *)orig_node->hna_buff;

	for (i = 0; i < num_elements; i++) {
		e = &buff[i];

		/* not found / deleted, need to add this new route */
		if ((e->netmask > 0) && (e->netmask <= 32))
			_hna_global_del(orig_node, e);
	}

	debugFree(orig_node->hna_buff, 1709);
	orig_node->hna_buff = NULL;
	orig_node->hna_buff_len = 0;
}

static void _hna_global_hash_del(void *data)
{
	struct hna_global_entry *hna_global_entry = data;
	struct hna_orig_ptr *hna_orig_ptr = NULL;
	struct list_head *list_pos, *list_pos_tmp;

	list_for_each_safe(list_pos, list_pos_tmp, &hna_global_entry->orig_list) {

		hna_orig_ptr = list_entry(list_pos, struct hna_orig_ptr, list);

		list_del((struct list_head *)&hna_global_entry->orig_list, list_pos, &hna_global_entry->orig_list);
		debugFree(hna_orig_ptr, 1710);
	}

	debugFree(hna_global_entry, 1711);
}

void hna_free(void)
{
	struct list_head *list_pos, *list_pos_tmp;
	struct hna_local_entry *hna_local_entry;

	/* hna local */
	list_for_each_safe(list_pos, list_pos_tmp, &hna_list) {
		hna_local_entry = list_entry(list_pos, struct hna_local_entry, list);
		hna_local_update_routes(hna_local_entry, ROUTE_DEL);

		debugFree(hna_local_entry, 1705);
	}

	if (hna_buff_local != NULL)
		debugFree(hna_buff_local, 1706);

	num_hna_local = 0;
	hna_buff_local = NULL;

	/* hna global */
	if (hna_global_hash != NULL)
		hash_delete(hna_global_hash, _hna_global_hash_del);
}
