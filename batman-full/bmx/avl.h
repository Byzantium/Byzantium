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

/*
 * avl code inspired by:
 * http://eternallyconfuzzled.com/tuts/datastructures/jsw_tut_avl.aspx
 * where Julienne Walker said (web page from 28. 2. 2010 12:55):
 * ...Once again, all of the code in this tutorial is in the public domain.
 * You can do whatever you want with it, but I assume no responsibility
 * for any damages from improper use. ;-)
 */

#ifndef _AVL_H
#define _AVL_H

#include <stdint.h>


#define AVL_MAX_HEIGHT 128


struct avl_node {
        void *item;
        int balance;
	struct avl_node * up;
	struct avl_node * link[2];
};

// obtain key pointer based on avl_node pointer
#define AVL_NODE_KEY( a_tree, a_node ) ( (void*) ( ((char*)(a_node)->item)+((a_tree)->key_offset) ) )

// obtain key pointer based on item pointer
#define AVL_ITEM_KEY( a_tree, a_item ) ( (void*) ( ((char*)(a_item))+((a_tree)->key_offset) ) )

struct avl_tree {
	struct avl_node *root;
	uint16_t key_size;
	uint16_t key_offset;
	uint32_t items;
};


#define AVL_INIT_TREE(tree, element_type, key_field) do { \
                          tree.root = NULL; \
                          tree.key_size = sizeof( (((element_type *) 0)->key_field) ); \
                          tree.key_offset = ((unsigned long) (&((element_type *) 0)->key_field)); \
                          tree.items = 0; \
                      } while (0)

#define AVL_TREE(tree, element_type, key_field) struct avl_tree (tree) =  { \
                   NULL, \
                   (sizeof( (((element_type *) 0)->key_field) )), \
                   ((unsigned long)(&((element_type *)0)->key_field)), \
                   0 }

#define avl_height(p) ((p) == NULL ? -1 : (p)->balance)
#define avl_max(a,b) ((a) > (b) ? (a) : (b))


struct avl_node *avl_find( struct avl_tree *tree, void *key );
void            *avl_find_item( struct avl_tree *tree, void *key );
struct avl_node *avl_next( struct avl_tree *tree, void *key );
void            *avl_next_item(struct avl_tree *tree, void *key);
void            *avl_first_item(struct avl_tree *tree);
struct avl_node *avl_iterate(struct avl_tree *tree, struct avl_node *it );
void            *avl_iterate_item(struct avl_tree *tree, struct avl_node **it );

void             avl_insert(struct avl_tree *tree, void *node, int32_t tag);
void            *avl_remove(struct avl_tree *tree, void *key, int32_t tag);

void             init_avl(void);

#ifdef AVL_DEBUG
struct avl_iterator {
	struct avl_node * up[AVL_MAX_HEIGHT];
	int upd[AVL_MAX_HEIGHT];
	int top;
};

struct avl_node *avl_iter(struct avl_tree *tree, struct avl_iterator *it );
void avl_debug( struct avl_tree *tree );
void avl_test(int m);
#endif


#endif
