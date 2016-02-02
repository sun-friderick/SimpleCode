/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 Nokia Corporation
   Original Author: Dmitry Kasatkin <dmitry.kasatkin@nokia.com>

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/* 
   $Id: btlist.h,v 1.6 2003/03/12 08:57:22 kds Exp $

   BTLIST - List implementaion for Bluetooth drivers family
   Host Controller Interface

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
                Imre Deak <ext-imre.deak@nokia.com>
*/

#ifndef _BTLIST_H
#define _BTLIST_H

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>

typedef struct btlist_head_t {
	struct list_head	list;
	__u32			len;
	rwlock_t		lock;
} btlist_head_t;


static inline void btl_head_init(btlist_head_t *head)
{
	INIT_LIST_HEAD(&head->list);
	head->len = 0;
	rwlock_init(&head->lock);
}

static inline void btl_init(void *q)
{
	//INIT_LIST_HEAD((struct list_head*)q);
	memset(q, 0, sizeof(struct list_head));
}

static inline void btl_read_lock(btlist_head_t *head)
{
	read_lock_bh(&head->lock);
}

static inline void btl_write_lock(btlist_head_t *head)
{
	write_lock_bh(&head->lock);
}

static inline void btl_read_unlock(btlist_head_t *head)
{
	read_unlock_bh(&head->lock);
}

static inline void btl_write_unlock(btlist_head_t *head)
{
	write_unlock_bh(&head->lock);
}

static inline int btl_empty(btlist_head_t *head)
{
	return list_empty(&head->list);
}

static inline int btl_linked(void *q)
{
	//return list_empty((struct list_head*)q);
	return ((struct list_head*)q)->next != NULL;
}

static inline int btl_len(btlist_head_t *head)
{
	return head->len;
}

static inline void *btl_peek(btlist_head_t *head)
{
	struct list_head	*q = head->list.next;

	if (q == &head->list)
		return NULL;
	return q;
}


static inline void __btl_add_head(btlist_head_t *head, void *q)
{
	list_add((struct list_head*)q, &head->list);
	head->len++;
}

static inline void btl_add_head(btlist_head_t *head, void *q)
{
	btl_write_lock(head);
	__btl_add_head(head, q);
	btl_write_unlock(head);
}

static inline void __btl_add_tail(btlist_head_t *head, void *q)
{
	list_add_tail((struct list_head*)q, &head->list);
	head->len++;
}

static inline void btl_add_tail(btlist_head_t *head, void *q)
{
	btl_write_lock(head);
	__btl_add_tail(head, q);
	btl_write_unlock(head);
}

static inline void *__btl_dequeue_head(btlist_head_t *head)
{
	struct list_head	*result;

	if (btl_empty(head))
		return NULL;

	result = head->list.next;
	//list_del_init(result);
	list_del(result);
	btl_init(result);
	head->len--;

	return result;
}

static inline void * btl_dequeue_head(btlist_head_t *head)
{
	struct list_head	*result;

	btl_write_lock(head);
	result = __btl_dequeue_head(head);
	btl_write_unlock(head);

	return result;
}

static inline void * btl_dequeue(btlist_head_t *head)
{
	return btl_dequeue_head(head);
}

static inline void * __btl_dequeue(btlist_head_t *head)
{
	return __btl_dequeue_head(head);
}

static inline void *__btl_dequeue_tail(btlist_head_t *head)
{
	struct list_head	*result;

	if (btl_empty(head))
		return NULL;

	result = head->list.prev;
	//list_del_init(result);
	list_del(result);
	btl_init(result);
	head->len--;

	return result;
}

static inline void *btl_dequeue_tail(btlist_head_t *head)
{
	struct list_head	*result;

	btl_write_lock(head);
	result = __btl_dequeue_tail(head);
	btl_write_unlock(head);

	return result;
}

static inline void btl_purge(btlist_head_t *head)
{
	struct list_head	*q;
	while ((q = btl_dequeue_head(head)) != NULL)
		kfree(q);
}

static inline void __btl_append(btlist_head_t *head, btlist_head_t *list)
{
	struct list_head	*first = list->list.next;

	if (first != &list->list) {
		struct list_head	*last = list->list.prev, *at = head->list.prev;

		first->prev = at;
		at->next = first;
		last->next = &head->list;
		head->list.prev = last;

		head->len += list->len;
		
		INIT_LIST_HEAD(&list->list);
		list->len = 0;
	}
}

static inline void btl_append(btlist_head_t *head, btlist_head_t *list)
{
	btl_write_lock(head);
	__btl_append(head, list);
	btl_write_unlock(head);
}

static inline void __btl_unlink(btlist_head_t *head, void *q)
{
	if (btl_linked((struct list_head*)q)) {
		//list_del_init((struct list_head*)q);
		list_del((struct list_head*)q);
		btl_init(q);
		head->len--;
	}
}

static inline void btl_unlink(btlist_head_t *head, void *q)
{
	if (!btl_linked((struct list_head*)q))
		return;
	btl_write_lock(head);
	__btl_unlink(head, q);
	btl_write_unlock(head);
}


#define __btl_next(ptr)	((void*)(((struct list_head *)(ptr))->next))
#define __btl_first(head)	((__btl_next(&(head))==(void*)&(head)) ? NULL : __btl_next(&(head)))

#define btl_for_each(pos, head) \
	for (pos = __btl_next(&(head)); (pos != (void*)&(head)) || (pos = NULL); pos = __btl_next(pos))

#define btl_for_each_cur(pos, head) \
	for (pos = __btl_next(pos); (pos != (void*)&(head)) || (pos = NULL); pos = __btl_next(pos))

#define btl_for_each_safe(pos, head, next) \
	for (pos = __btl_next(&head), next = __btl_next(pos); (pos != (void*)&head) || (pos = NULL); \
		pos = next, next = __btl_next(pos))

#endif
