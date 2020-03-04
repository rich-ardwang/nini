/* ni_list.h - A generic doubly linked list implementation
 *
 * Copyright (c) 2020-2030, Lei Wang <wanglei_gmgc@163.com>
 * All rights reserved.
 *
 */

#ifndef _NI_LIST_H_
#define _NI_LIST_H_

/* Node, List, and Iterator are the only data structures used currently. */

typedef struct ni_list_node {
    struct ni_list_node     *prev;
    struct ni_list_node     *next;
    void                    *value;
} ni_list_node;

typedef struct ni_list_iter {
    ni_list_node    *next;
    int             direction;
} ni_list_iter;

typedef struct ni_list {
    ni_list_node    *head;
    ni_list_node    *tail;
    void            *(*dup)(void *ptr);
    void            (*free)(void *ptr);
    int             (*match)(void *ptr, void *key);
    unsigned long   len;
} ni_list;

/* Functions implemented as macros */
#define lstLen(l)               ((l)->len)
#define lstFirst(l)             ((l)->head)
#define lstLast(l)              ((l)->tail)
#define lstPrevNode(n)          ((n)->prev)
#define lstNextNode(n)          ((n)->next)
#define lstNodeVal(n)           ((n)->value)
#define lstSetDupMethod(l, m)   ((l)->dup = (m))
#define lstSetFreeMethod(l, m)  ((l)->free = (m))
#define lstSetMatchMethod(l, m) ((l)->match = (m))
#define lstGetDupMethod(l)      ((l)->dup)
#define lstGetFree(l)           ((l)->free)
#define lstGetMatchMethod(l)    ((l)->match)

/* Prototypes */
ni_list *ni_list_create(void);
void ni_list_release(ni_list *lst);
void ni_list_empty(ni_list *lst);
ni_list *ni_list_add_node_head(ni_list *lst, void *val);
ni_list *ni_list_add_node_tail(ni_list *lst, void *val);
ni_list *ni_list_insert_node(ni_list *lst, ni_list_node *old_nd, void *val, int after);
void ni_list_del_node(ni_list *lst, ni_list_node *nd);
ni_list_iter *ni_list_get_iterator(ni_list *lst, int direction);
ni_list_node *ni_list_next(ni_list_iter *iter);
void ni_list_release_iterator(ni_list_iter *iter);
ni_list *ni_list_dup(ni_list *orig);
ni_list_node *ni_list_search_key(ni_list *lst, void *key);
ni_list_node *ni_list_index(ni_list *lst, long idx);
void ni_list_rewind(ni_list *lst, ni_list_iter *iter);
void ni_list_rewind_tail(ni_list *lst, ni_list_iter *iter);
void ni_list_rotate(ni_list *lst);
void ni_list_join(ni_list *l, ni_list *o);

/* Directions for iterators */
#define AL_START_HEAD 0
#define AL_START_TAIL 1

#endif /* _NI_LIST_H_ */
