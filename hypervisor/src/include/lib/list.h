#ifndef _HYVEMIND_LIB_LIST_H
#define _HYVEMIND_LIB_LIST_H

#include "hyvstdlib.h"

/* Not-type-safe version */
#define container_of(ptr, type, member) \
    (((char *) ptr) - offsetof(type, member))

struct list_head {
    struct list_head *prev, *next;
};

#define LIST_HEAD_INIT(head_name) { &(head_name), &(head_name) }

static inline void
INIT_LIST_HEAD(struct list_head *head)
{
    head->next = head;
    head->prev = head;
}

static inline void
__list_add(
        struct list_head *new_node,
        struct list_head *before_nn,
        struct list_head *after_nn
) {
    new_node->next = after_nn;
    new_node->prev = before_nn;
    before_nn->next = new_node;
    after_nn->prev = new_node;
}

static inline void
list_add_head(struct list_head *head, struct list_head *new_node)
{
    __list_add(new_node, head, head->next);
}

static inline void
list_add_tail(struct list_head *head, struct list_head *new_node)
{
    __list_add(new_node, head->prev, head);
}

static inline void
__list_remove(struct list_head *prev, struct list_head *next)
{
    prev->next = next;
    next->prev = prev;
}

static inline void
list_remove_entry(struct list_head *rm_entry)
{
    if (!rm_entry)
        return;

    __list_remove(rm_entry->prev, rm_entry->next);
    rm_entry->prev = NULL;
    rm_entry->next = NULL;
}

static inline void
list_remove_head(struct list_head *head)
{
    list_remove_entry(head->next);
}

static inline void
list_remove_tail(struct list_head *head)
{
    list_remove_entry(head->prev);
}

static inline bool
is_list_empty(struct list_head *head)
{
    return head->next == head;
}

#define list_entry(ptr, type, member) \
    ((type *) (container_of(ptr, type, member)))

#define list_first_entry_nullable(head, type, member) \
    (((head)->next == (head)) ? NULL : list_entry((head)->next, type, member))

#define list_last_entry_nullable(head, type, member) \
    (((head)->prev == (head)) ? NULL : list_entry((head)->prev, type, member))

#define list_for_each(pos, head) \
    for(pos = (head)->next; pos != (head); pos = pos->next)

#endif /* _HYVEMIND_LIB_LIST_H */

