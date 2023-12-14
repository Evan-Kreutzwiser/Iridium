
#ifndef KERNEL_LINKE_LIST_H_
#define KERNEL_LINKE_LIST_H_

#include <types.h>
#include <kernel/spinlock.h>
#include <iridium/types.h>

#include <stdbool.h>
#define __need_size_t
#include <stddef.h>

typedef struct _node {
    struct _node* next;
    void *data;
} _node;

typedef struct linked_list {
    size_t count;
    _node* head; // Begining of the list
    _node* tail; // End of the list
    lock_t lock;
} linked_list;

// A compare-to for sorting and searching the list for values
// Should not modify any values, just compare
// Returns a negative value if `data` is smaller, positive if `target` is larger, or 0 if they are equal
typedef int (*search_function)(void *data, void *target);

void linked_list_init(linked_list *list);

// Append `data` to the end of the linked list
ir_status_t linked_list_add(linked_list *list, void *data);

// Insert `data` into the list using a comparison function to determine it's index
ir_status_t linked_list_add_sorted(linked_list *list, search_function function, void *data);

// Writes a pointer to `out` with the data contained at `index` of the linked list
ir_status_t linked_list_get(linked_list *list, uint index, void **out);

// Removes 'index' from the linked list, placing the removed data in `out` if `out` is not null.
ir_status_t linked_list_remove(linked_list *list, uint index, void **out);

// Find an element in the list matching `target` using the provided search function.
ir_status_t linked_list_find(linked_list *list, void *target, search_function comapre_to, uint *index, void **out);

// Remove the first element in the list matching `target` using the provided search
// function. If a search function is not provided, the list compares raw pointer
// values. The caller is responsible for freeing the removed data passed to `out`.
ir_status_t linked_list_find_and_remove(linked_list *list, void *target, search_function comapre_to, void **out);

#endif // ! KERNEL_LINKE_LIST_H_
