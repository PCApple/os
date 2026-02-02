#ifndef HEAP_H
#define HEAP_H
#include <stdint.h>
#include "tcb.h"
#include "utils.h"
#define MAX_HEAP_SIZE 1024
typedef struct heap_node { // heap is going to be stored as a binary tree in an array, L = 2i + 1, R = 2i + 2
    tcb_t* obj;
    uint32_t prio;
} heap_node_t;
int heapify(heap_node_t *heap);
int heapify_helper(heap_node_t *heap, int index, int size);
int heap_insert(heap_node_t *heap, tcb_t *obj, uint32_t prio);
int heap_remove(heap_node_t *heap, heap_node_t *node);
int get_heap_size(heap_node_t *heap);

#endif // HEAP_H
