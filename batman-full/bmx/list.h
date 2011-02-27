/*
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

/*
 * This list implementation is originally based on the
 * double-linked list implementaton from the linux-kernel
 * which can be found in include/linux/list.h
 *
 * The following has been changed to better fit my needs and intuition:
 * - items counter
 * - plist handler (list-node structure with void* item pointer
 * - single-linked list instead of double-linked list to save overhead
 * - some functions for straight item access!
 */


#ifndef _LIST_H
#define _LIST_H



struct list_node {
	struct list_node *next;
};

struct list_head {
	struct list_node *next, *prev;
	uint16_t items;
	uint16_t list_node_offset;
	uint16_t key_node_offset;
	uint16_t key_length;
};

struct plist_node {
	struct list_node list;
	void *item;
};



#define LIST_SIMPEL(lst, element_type, list_field, key_field ) struct list_head lst = { \
          .next = (struct list_node *)&lst, \
          .prev = (struct list_node *) & lst,    \
          .items = 0, \
          .list_node_offset = ((unsigned long)(&((element_type *)0)->list_field)), \
          .key_node_offset = ((unsigned long)(&((element_type *)0)->key_field)), \
          .key_length = sizeof(((element_type *)0)->key_field) }

#define LIST_INIT_HEAD(ptr, element_type, list_field) do { \
	ptr.next = ptr.prev =(struct list_node *)&ptr; \
        ptr.items = 0; \
        ptr.list_node_offset = ((unsigned long)(&((element_type *)0)->list_field)); \
        ptr.key_node_offset = 0; \
        ptr.key_length = 0; \
} while (0)

#define LIST_EMPTY(lst)  ((lst)->next == (struct list_node *)(lst))


#define list_get_first(head) ((void*)((LIST_EMPTY(head)) ? NULL : (((char*) (head)->next) - (head)->list_node_offset) ))
#define list_get_last(head) ((void*)((LIST_EMPTY(head)) ? NULL : (((char*) (head)->prev) - (head)->list_node_offset) ))

void *list_iterate( struct list_head *head, void *node );
void *list_find(struct list_head *head, void* key);

void list_add_head(struct list_head *head, struct list_node *new);
void list_add_tail(struct list_head *head, struct list_node *new );
void list_add_after(struct list_head *head, struct list_node *pos, struct list_node *new);
void list_del_next(struct list_head *head, struct list_node *pos);
void *list_rem_head(struct list_head *head);


#define plist_get_first(head) (LIST_EMPTY(head) ? NULL : \
                              ((struct plist_node*)(((char*) (head)->next) - (head)->list_node_offset))->item )

#define plist_get_last(head) (LIST_EMPTY(head) ? NULL : \
                              ((struct plist_node*)(((char*) (head)->prev) - (head)->list_node_offset))-item )

void * plist_iterate(struct list_head *head, struct plist_node **pln);

void plist_add_head(struct list_head *head, void *item);
void plist_add_tail(struct list_head *head, void *item);
void *plist_rem_head(struct list_head *head);



/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) ( (type *)( (char *)(ptr) - (unsigned long)(&((type *)0)->member) ) )

/**
 * list_for_each	-	iterate over a list
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (struct list_node *)(head); pos = pos->next)

#define list_for_each_item(pos, head, item, type, member) \
	for (pos = (head)->next; \
             pos != (struct list_node *)(head) && \
              (item = ((type *)( (char *)(pos) - (unsigned long)(&((type *)0)->member) ) )); \
             pos = pos->next)

/**
 * list_for_each_safe	-	iterate over a list safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop counter.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (struct list_node *)(head); \
		pos = n, n = pos->next)


#endif
