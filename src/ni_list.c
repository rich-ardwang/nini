/* ni_list.c - A generic doubly linked list implementation
 *
 * Copyright (c) 2020-2030, Lei Wang <wanglei_gmgc@163.com>
 * All rights reserved.
 *
 */

#include <stdlib.h>
#include "ni_list.h"
#include "ni_malloc.h"

/* Create a new list. The created list can be freed with
 * AlFreeList(), but private value of every node need to be freed
 * by the user before to call AlFreeList().
 *
 * On error, NULL is returned. Otherwise the pointer to the new list. */
ni_list *ni_list_create(void) {
    struct ni_list *lst;
    if ((lst = ni_malloc(sizeof(*lst))) == NULL)
        return NULL;
    lst->head = lst->tail = NULL;
    lst->len = 0;
    lst->dup = NULL;
    lst->free = NULL;
    lst->match = NULL;
    return lst;
}

/* Remove all the elements from the list without destroying the list itself. */
void ni_list_empty(ni_list *lst) {
    unsigned long   len;
    ni_list_node    *current, *next;
    current = lst->head;
    len = lst->len;
    while(len--) {
        next = current->next;
        if (lst->free)
            lst->free(current->value);
        ni_free(current);
        current = next;
    }
    lst->head = lst->tail = NULL;
    lst->len = 0;
}

/* Free the whole list.
 *
 * This function can't fail. */
void ni_list_release(ni_list *lst) {
    ni_list_empty(lst);
    ni_free(lst);
}

/* Add a new node to the list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
ni_list *ni_list_add_node_head(ni_list *lst, void *val) {
    ni_list_node *node;
    if ((node = ni_malloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = val;
    if (lst->len == 0) {
        lst->head = lst->tail = node;
        node->prev = node->next = NULL;
    } else {
        node->prev = NULL;
        node->next = lst->head;
        lst->head->prev = node;
        lst->head = node;
    }
    lst->len++;
    return lst;
}

/* Add a new node to the list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
ni_list *ni_list_add_node_tail(ni_list *lst, void *val) {
    ni_list_node *node;
    if ((node = ni_malloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = val;
    if (lst->len == 0) {
        lst->head = lst->tail = node;
        node->prev = node->next = NULL;
    } else {
        node->prev = lst->tail;
        node->next = NULL;
        lst->tail->next = node;
        lst->tail = node;
    }
    lst->len++;
    return lst;
}

ni_list *ni_list_insert_node(ni_list *lst, ni_list_node *old_nd, void *val, int after) {
    ni_list_node *node;
    if ((node = ni_malloc(sizeof(*node))) == NULL)
        return NULL;
    node->value = val;
    if (after) {
        node->prev = old_nd;
        node->next = old_nd->next;
        if (lst->tail == old_nd)
            lst->tail = node;
    } else {
        node->next = old_nd;
        node->prev = old_nd->prev;
        if (lst->head == old_nd)
            lst->head = node;
    }
    if (node->prev != NULL) {
        node->prev->next = node;
    }
    if (node->next != NULL) {
        node->next->prev = node;
    }
    lst->len++;
    return lst;
}

/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
void ni_list_del_node(ni_list *lst, ni_list_node *nd) {
    if (nd->prev)
        nd->prev->next = nd->next;
    else
        lst->head = nd->next;
    if (nd->next)
        nd->next->prev = nd->prev;
    else
        lst->tail = nd->prev;
    if (lst->free)
        lst->free(nd->value);
    ni_free(nd);
    lst->len--;
}

/* Returns a list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the list.
 *
 * This function can't fail. */
ni_list_iter *ni_list_get_iterator(ni_list *lst, int direction) {
    ni_list_iter *iter;
    if ((iter = ni_malloc(sizeof(*iter))) == NULL)
        return NULL;
    if (direction == AL_START_HEAD)
        iter->next = lst->head;
    else
        iter->next = lst->tail;
    iter->direction = direction;
    return iter;
}

/* Release the iterator memory */
void ni_list_release_iterator(ni_list_iter *iter) {
    ni_free(iter);
}

/* Create an iterator in the list private iterator structure */
void ni_list_rewind(ni_list *lst, ni_list_iter *iter) {
    iter->next = lst->head;
    iter->direction = AL_START_HEAD;
}

void ni_list_rewind_tail(ni_list *lst, ni_list_iter *iter) {
    iter->next = lst->tail;
    iter->direction = AL_START_TAIL;
}

/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * listDelNode(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the list,
 * or NULL if there are no more elements, so the classical usage patter
 * is:
 *
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 *
 * */
ni_list_node *ni_list_next(ni_list_iter *iter) {
    ni_list_node *current = iter->next;
    if (current != NULL) {
        if (iter->direction == AL_START_HEAD)
            iter->next = current->next;
        else
            iter->next = current->prev;
    }
    return current;
}

/* Duplicate the whole list. On out of memory NULL is returned.
 * On success a copy of the original list is returned.
 *
 * The 'Dup' method set with listSetDupMethod() function is used
 * to copy the node value. Otherwise the same pointer value of
 * the original node is used as value of the copied node.
 *
 * The original list both on success or error is never modified. */
ni_list *ni_list_dup(ni_list *org) {
    ni_list         *copy;
    ni_list_iter    iter;
    ni_list_node    *node;
    if ((copy = ni_list_create()) == NULL)
        return NULL;
    copy->dup = org->dup;
    copy->free = org->free;
    copy->match = org->match;
    ni_list_rewind(org, &iter);
    while((node = ni_list_next(&iter)) != NULL) {
        void *value;
        if (copy->dup) {
            value = copy->dup(node->value);
            if (value == NULL) {
                ni_list_release(copy);
                return NULL;
            }
        } else
            value = node->value;
        if (ni_list_add_node_tail(copy, value) == NULL) {
            ni_list_release(copy);
            return NULL;
        }
    }
    return copy;
}

/* Search the list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with listSetMatchMethod(). If no 'match' method
 * is set, the 'value' pointer of every node is directly
 * compared with the 'key' pointer.
 *
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists
 * NULL is returned. */
ni_list_node *ni_list_search_key(ni_list *lst, void *key) {
    ni_list_iter iter;
    ni_list_node *node;
    ni_list_rewind(lst, &iter);
    while((node = ni_list_next(&iter)) != NULL) {
        if (lst->match) {
            if (lst->match(node->value, key))
                return node;
        } else {
            if (key == node->value)
                return node;
        }
    }
    return NULL;
}

/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned. */
ni_list_node *ni_list_index(ni_list *lst, long idx) {
    ni_list_node *nd;
    if (idx < 0) {
        idx = (-idx)-1;
        nd = lst->tail;
        while(idx-- && nd)
            nd = nd->prev;
    } else {
        nd = lst->head;
        while(idx-- && nd)
            nd = nd->next;
    }
    return nd;
}

/* Rotate the list removing the tail node and inserting it to the head. */
void ni_list_rotate(ni_list *lst) {
    ni_list_node *tail = lst->tail;
    if (lstLen(lst) <= 1)
        return;
    /* Detach current tail */
    lst->tail = tail->prev;
    lst->tail->next = NULL;
    /* Move it as head */
    lst->head->prev = tail;
    tail->prev = NULL;
    tail->next = lst->head;
    lst->head = tail;
}

/* Add all the elements of the list 'o' at the end of the
 * list 'l'. The list 'other' remains empty but otherwise valid. */
void ni_list_join(ni_list *l, ni_list *o) {
    if (o->head)
        o->head->prev = l->tail;
    if (l->tail)
        l->tail->next = o->head;
    else
        l->head = o->head;
    if (o->tail)
        l->tail = o->tail;
    l->len += o->len;
    /* Setup other as an empty list. */
    o->head = o->tail = NULL;
    o->len = 0;
}
