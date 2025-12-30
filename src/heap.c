#include "include/heap.h"

// takes in the head of a heap and arranges it to satisfy the heap priority
// @param heap: pointer to the array of the heap
// @return: 0 on success, -1 on failure
int heapify(heap_node_t *heap) {
  if (heap == NULL) {
    return -1;
  }
  int size = get_heap_size(heap);
  if (size <= 1) {
    return 0;
  }
  int ret_values = 0;
  for (int i = (size / 2) - 1; i >= 0; i--) {
    ret_values += heapify_helper(heap, i, size);
    if (ret_values < 0) {
      return -1;
    }
  }
  return ret_values;
}
int heapify_helper(heap_node_t *heap, int index, int size) {
  int smallest_value = index;
  int left_child = 2 * index + 1;
  int right_child = 2 * index + 2;
  if (left_child < size && heap[left_child].prio < heap[smallest_value].prio) {
    smallest_value = left_child;
  }
  if (right_child < size && heap[right_child].prio < heap[smallest_value].prio) {
    smallest_value = right_child;
  }
  if (smallest_value != index) {
    // swap
    int temp_prio = heap[index].prio;
    tcb_t *temp_obj = heap[index].obj;
    heap[index].prio = heap[smallest_value].prio;
    heap[index].obj = heap[smallest_value].obj;
    heap[smallest_value].prio = temp_prio;
    heap[smallest_value].obj = temp_obj;
  }
  return 0;


}

// inserts an object with a given priority into the heap
// @param heap: pointer to the array of the heap
// @param obj: pointer to the object to be inserted
// @param prio: priority of the object
// @return: 0 on success, -1 on failure
int heap_insert(heap_node_t *heap, tcb_t *obj, uint32 prio) {
  int size = get_heap_size(heap);
  if (size == -1 || size >= MAX_HEAP_SIZE) {
    return -1;
  }
  heap[size].obj = obj;
  heap[size].prio = prio;
  heapify(heap);
  return 0;  
}

// removes a node from the heap
// @param heap: pointer to the array of the heap
// @param node: pointer to the node to be removed
// @return: 0 on success, -1 on failure
int heap_remove(heap_node_t *heap, heap_node_t *node) {
  int size = get_heap_size(heap);
  if (size == -1 || size == 0) {
    return -1;
  }
  int index = -1;
  for (int i = 0; i < size; i++) {
    if (heap[i].obj == node->obj && heap[i].prio == node->prio) {
      index = i;
      break;
    }
  }
  if (index == -1) {
    return -1;
  }
  heap[index].obj = heap[size - 1].obj;
  heap[index].prio = heap[size - 1].prio;
  heap[size - 1].obj = NULL;
  heap[size - 1].prio = 0;
  heapify(heap);
  return 0;
}

// gets the size of the heap
// @param heap: pointer to the array of the heap
// @return: size of the heap
int get_heap_size(heap_node_t *heap) {
  if (heap == NULL) return -1;
  for (int i = 0; i < MAX_HEAP_SIZE; i++) {
    if (heap[i].obj == NULL) {
      return i;
    }
  }
  return 0;
}

