/// @file kernel/linked_list.c
/// @brief Functions for manipulating `linked_list` data structures

#include "kernel/linked_list.h"

#include "types.h"
#include "iridium/types.h"
#include "iridium/errors.h"
#include <kernel/spinlock.h>
#include "kernel/heap.h"
#include "kernel/string.h"

#include <stdbool.h>
#include <stddef.h>

#include "arch/debug.h"

struct _node {
    struct _node* next;
    void *data;
};

/// @brief Default linked list searching function
///
/// The search function used when given a null search function.
/// Compares raw pointer values since removing a known value
/// from a list is the most common use case.
/// @param data Element from the linked list
/// @param target Data being searched for or inserted
/// @return A negative value if `target` is larger, positive if `data` is larger, or 0 if they are equal
int default_search(void *data, void *target) {
    return (uintptr_t)data - (uintptr_t)target;
}

/// @brief Append an element to a linked list
/// @param list The list to add the element to
/// @param data A pointer to store in the linked list
/// @return IR_OK on success
ir_status_t linked_list_add(linked_list* list, void *data) {
    spinlock_aquire(list->lock);

    _node *new_node = malloc(sizeof(_node));
    new_node->data = data;
    new_node->next = NULL;

    if (list->count == 0) {
        list->head = new_node;
        list->tail = new_node;
    }
    else {
        list->tail->next = new_node;
        list->tail = new_node;
    }
    list->count++;

    spinlock_release(list->lock);
    return IR_OK;
}

/// @brief Insert `data` into the list using a comparison function to determine it's index
/// @param list The list to insert the element into
/// @param function Used to determine the index in the list to place the data, in ascending order
/// @param data Pointer or value to be inserted into the list
/// @return `IR_ok`
ir_status_t linked_list_add_sorted(linked_list *list, search_function function, void *data) {
    if (!function) function = default_search;
    spinlock_aquire(list->lock);

    // Search for the first element larger than what is being inserted
    _node *last_smaller = NULL;
    _node *first_larger = list->head;
    while (first_larger && function(first_larger->data, data) <= 0) {
        last_smaller = first_larger;
        first_larger = first_larger->next;
    }

    _node *new_node = malloc(sizeof(_node));
    new_node->data = (void*)data;
    new_node->next = first_larger;

    if (!last_smaller) { // Inserting to index 0
        list->head = new_node;
    }
    else { last_smaller->next = new_node; }

    if (!first_larger) { // Inserting at end
        list->tail = new_node;
    }

    list->count++;

    spinlock_release(list->lock);
    return IR_OK;
}


/// @brief Retrieve data from a linked list
/// @param list The list to get the element from
/// @param index The index to retrieve
/// @param out Output parameter where the pointer stored in the list will be placed
/// @return `IR_OK` on success, or `IR_ERROR_INVALID_ARGUMENTS` when index is out of range
ir_status_t linked_list_get(linked_list *list, uint index, void **out) {
    spinlock_aquire(list->lock);

    if (index >= list->count) {
        spinlock_release(list->lock);
        return IR_ERROR_INVALID_ARGUMENTS;
    }

    _node *current = list->head;

    for (uint i = 0; i < index; i++) {
        current = current->next;
    }

    *out = current->data;

    spinlock_release(list->lock);
    return IR_OK;
}

/// @brief Remove an index from a linked list
///
/// Removes 'index' from the linked list, placing the removed data in `out`.
/// The caller is responsible for freeing the data contained at the index.
/// @param list The list to remove an element from
/// @param index The index to remove
/// @param out Output parameter where the removed pointer will be placed.
///            Can be set to `null` to discard the pointer.
/// @return `IR_OK` on success, or `IR_ERROR_INVALID_ARGUMENTS` when index is out of range
ir_status_t linked_list_remove(linked_list *list, uint index, void **out) {

    spinlock_aquire(list->lock);

    if (index >= list->count) {
        spinlock_release(list->lock);
        return IR_ERROR_INVALID_ARGUMENTS;
    }


    _node* previous = NULL;
    _node* node = list->head;
    for (uint i = 1; i < index; i++) {
        previous = node;
        node = node->next;
    }

    // If requested, output the data stored in the node
    if (out) {
        *out = node->data;
    }

    if (!previous /*index == 0*/) {
        list->head = node->next;
    }
    else {
        previous->next = node->next;
    }

    if (node == list->tail) {
        list->tail = previous;
    }

    free(node);

    list->count--;

    spinlock_release(list->lock);
    return IR_OK;
}

// Find an element in the list matching `target` using the provided search function.
// If a search function is not provided, the list compares raw pointer values.
// The index and/or value of the item found can be output to variables
ir_status_t linked_list_find(linked_list *list, void *target, search_function compare_to, uint *index, void **out) {
    if (!compare_to) compare_to = default_search;

    spinlock_aquire(list->lock);

    _node *node = list->head;
    for (uint i = 0; i < list->count; i++) {
        // Check if the node matches the target data
        if (compare_to(node->data, target) == 0) {
            // Return the found data
            if (index) { *index = i; }
            if (out) { *out = node->data; }
            spinlock_release(list->lock);
            return IR_OK;
        }
        node = node->next;
    }

    spinlock_release(list->lock);
    return IR_ERROR_NOT_FOUND;
}

/// @brief Remove `target` from a linked list.
/// If applicable, the caller is responsible for freeing the data passed to `out`.
/// @param list A linked list
/// @param target The data that the search function is looking for
/// @param compare_to Search function used to locate the target data in the list.
///               Can be null to use the default function, which compares raw pointers.
/// @param out Output parameter set to the value removed from the list
/// @return `IR_OK` if the target was removed, otherwise `IR_ERROR_NOT_FOUND`
ir_status_t linked_list_find_and_remove(linked_list *list, void *target, search_function compare_to, void **out) {
    if (!list) {
        return IR_ERROR_INVALID_ARGUMENTS;
    }

    if (!compare_to) compare_to = default_search;

    spinlock_aquire(list->lock);

    _node *previous = NULL;
    _node *node = list->head;
    for (uint i = 0; i < list->count; i++) {
        // Check if the node matches the target data
        if (compare_to(node->data, target) == 0) {
            if (out) *out = node->data;

            // Remove the node from the list
            if (i == 0) {
                list->head = node->next;
            } else {
                previous->next = node->next;
            }
            if (node == list->tail) {
                list->tail = previous;
            }
            free(node);
            list->count--;

            spinlock_release(list->lock);
            return IR_OK;
        }
        previous = node;
        node = node->next;
    }

    spinlock_release(list->lock);

    return IR_ERROR_NOT_FOUND;
}

/// Free a list, discarding all data contained in it. The caller may free the
/// linked_list object after calling.
/// NOTE: Don't use if other threads may attempt to access the list afterwards.
///       It is preferable to remove each element of the list instead.
/// WARNING: Will cause memory leaks if used incorrectly.
void linked_list_destroy(linked_list *list) {

    _node *node = list->head;
    while (node) {
        _node *next = node->next;
        free(node);
        node = next;
    }

    list->count = 0;
    list->head = NULL;
    list->tail = NULL;
}
