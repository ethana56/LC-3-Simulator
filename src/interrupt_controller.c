#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "interrupt_controller.h"

#define IMPOSSIBLE_PRIORITY 255

struct interrupt_controller {
    uint8_t interrupts[UINT8_MAX]; /* index is vector. Contains priorities */
    uint8_t queue_heap[UINT8_MAX]; /* heap (binary tree )of vectors based on priorities in interrupts */
    int queue_heap_size;
};

InterruptController *interrupt_controller_new(void) {
    InterruptController *inter_cont;
    int i;
    inter_cont = malloc(sizeof(InterruptController));
    if (inter_cont == NULL) {
        return NULL;
    }
    for (i = 0; i < UINT8_MAX; ++i) {
        inter_cont->interrupts[i] = IMPOSSIBLE_PRIORITY;
    }
    inter_cont->queue_heap_size = 0;
    return inter_cont;
}

void interrupt_controller_free(InterruptController *inter_cont) {
    free(inter_cont);
}

static int parent_index(int index) {
    return (index - 1) / 2;
}

static void interrupt_queue_swap(InterruptController *inter_cont, int index_1, int index_2) {
    uint8_t temp;
    temp = inter_cont->queue_heap[index_1];
    inter_cont->queue_heap[index_1] = inter_cont->queue_heap[index_2];
    inter_cont->queue_heap[index_2] = temp;
}

static int interrupt_queue_should_percolate_up(InterruptController *inter_cont, int child_i, int parent_i) {
    uint8_t *interrupts, *queue_heap;
    interrupts = inter_cont->interrupts;
    queue_heap = inter_cont->queue_heap;
    if (interrupts[queue_heap[child_i]] == interrupts[queue_heap[parent_i]]) {
        return queue_heap[child_i] > queue_heap[parent_i];
    }
    return interrupts[queue_heap[child_i]] > interrupts[queue_heap[parent_i]];
}

static void interrupt_queue_percolate_up(InterruptController *inter_cont, int index) {
    int cur_i;
    cur_i = index;
    while (cur_i != 0) {
        int parent_i;
        parent_i = parent_index(cur_i);
        if (!interrupt_queue_should_percolate_up(inter_cont, cur_i, parent_i)) {
            break;
        }
        interrupt_queue_swap(inter_cont, cur_i, parent_i);
        cur_i = parent_i;
    }
}

static int last_parent_index(int size) {
    return (size / 2) - 1;
}

static int left_child_index(int index) {
    return (2 * index) + 1;
}

static int right_child_index(int index) {
    return 2 * index + 2;
}

static int interrupt_queue_max_index(InterruptController *inter_cont, int index1, int index2) {
    int result;
    uint8_t *interrupts, *queue_heap;
    uint8_t priority1, priority2;
    interrupts = inter_cont->interrupts;
    queue_heap = inter_cont->queue_heap;
    priority1 = interrupts[queue_heap[index1]];
    priority2 = interrupts[queue_heap[index2]];
    if (priority1 == priority2) {
        result = queue_heap[index1] > queue_heap[index2] ? index1 : index2;
    } else {
        result = priority1 > priority2 ? index1 : index2;
    }
    return result;
}

/* index must be a parent index */
static int max_child_index(InterruptController *inter_cont, int index) {
    int right_i, left_i;
    left_i = left_child_index(index);
    right_i = right_child_index(index);
    if (right_i >= inter_cont->queue_heap_size) {
        return left_i;
    }
    return interrupt_queue_max_index(inter_cont, left_i, right_i);
}

static int interrupt_queue_should_percolate_down(InterruptController *inter_cont, int parent_i, int child_i) {
    uint8_t *interrupts, *queue_heap;
    interrupts = inter_cont->interrupts;
    queue_heap = inter_cont->queue_heap;
    if (interrupts[queue_heap[parent_i]] == interrupts[queue_heap[child_i]]) {
        return queue_heap[parent_i] < queue_heap[child_i];
    }
    return interrupts[queue_heap[parent_i]] < interrupts[queue_heap[child_i]];
}

static void interrupt_queue_percolate_down(InterruptController *inter_cont, int parent_i) {
    int cur_parent_i, last_parent_i;
    last_parent_i = last_parent_index(inter_cont->queue_heap_size);
    cur_parent_i = parent_i;
    while (cur_parent_i <= last_parent_i) {
        int largest_child_i;
        largest_child_i = max_child_index(inter_cont, cur_parent_i);
        if (!interrupt_queue_should_percolate_down(inter_cont, cur_parent_i, largest_child_i)) {
            break;
        }
        interrupt_queue_swap(inter_cont, cur_parent_i, largest_child_i);
        cur_parent_i = largest_child_i;
    }
}

static int valid_child(InterruptController *inter_cont, int child_i, int parent_i) {
    if (inter_cont->interrupts[inter_cont->queue_heap[child_i]] == inter_cont->interrupts[inter_cont->queue_heap[parent_i]]) {
        return inter_cont->queue_heap[child_i] <= inter_cont->queue_heap[parent_i];
    }
    return inter_cont->interrupts[inter_cont->queue_heap[child_i]] < inter_cont->interrupts[inter_cont->queue_heap[parent_i]];
}

int check_heap(InterruptController *inter_cont) {
    int i, last_parent_i;
    last_parent_i = last_parent_index(inter_cont->queue_heap_size);
    for (i = 0; i <= last_parent_i; ++i) {
        int left_child, right_child;
        left_child = left_child_index(i);
        right_child = right_child_index(i);
        if (left_child <= inter_cont->queue_heap_size - 1) {
            if (!valid_child(inter_cont, left_child, i)) return 0;
        }
        if (right_child <= inter_cont->queue_heap_size - 1) {
            if (!valid_child(inter_cont, right_child, i)) return 0;
        }
    }
    return 1;
}

static void interrupt_queue_enqueue(InterruptController *inter_cont, uint8_t value) {
    inter_cont->queue_heap[inter_cont->queue_heap_size++] = value;
    interrupt_queue_percolate_up(inter_cont, inter_cont->queue_heap_size - 1);
}

/* Should only be called when inter_cont->queue_heap_size > 0 */
static uint8_t interrupt_queue_dequeue(InterruptController *inter_cont) {
    uint8_t vec;
    vec = inter_cont->queue_heap[0];
    inter_cont->queue_heap[0] = inter_cont->queue_heap[--inter_cont->queue_heap_size];
    interrupt_queue_percolate_down(inter_cont, 0);
    return vec;
}

static uint8_t interrupt_queue_peek(InterruptController *inter_cont) {
    return inter_cont->queue_heap[0];
}

void interrupt_controller_alert(InterruptController *inter_cont, uint8_t vec, uint8_t priority) {
    if (inter_cont->interrupts[vec] == IMPOSSIBLE_PRIORITY) {
        inter_cont->interrupts[vec] = priority;
        interrupt_queue_enqueue(inter_cont, vec);
    }
}

int interrupt_controller_peek(InterruptController *inter_cont, uint8_t *vec, uint8_t *priority) {
    int result;
    result = 0;
    if (inter_cont->queue_heap_size > 0) {
        *vec = interrupt_queue_peek(inter_cont);
        *priority = inter_cont->interrupts[*vec];
        result = 1;
    }
    return result;
}

void interrupt_controller_take(InterruptController *inter_cont) {
    uint8_t vec;
    if (inter_cont->queue_heap_size == 0) {
        return;
    }
    vec = interrupt_queue_dequeue(inter_cont);
    inter_cont->interrupts[vec] = IMPOSSIBLE_PRIORITY;
}



