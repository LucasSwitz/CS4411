#include "queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/* 
    This is your test file
    Write good tests!

*/

// Test 1 is given to you
int test1()
{
    queue_t *new = queue_new();
    return 0;
}

// Test 2 : preprend to an empty queue
int testPrependEmpty() {
    queue_t * queue = queue_new();
    int *item1 = malloc(sizeof(int));
    queue_prepend(queue,item1);

    if (queue->item == item1) {
        return 0;
    }

    return -1;
}

// Test Prepend on Non Empty (3 items)
int testPrependNonEmpty() {
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    int *item2 = malloc(sizeof(int));
    int *item3 = malloc(sizeof(int));

    queue_prepend(queue, item3);
    queue_prepend(queue, item2);
    queue_prepend(queue, item1);

    if (queue->item == item1 && queue->next->item == item2 && queue->next->next->item == item3) {
        return 0;
    }

    return -1;
}

int main(void) {
    assert(test1() == 0);
    assert(testPrependEmpty() == 0);
    assert(testPrependNonEmpty() == 0);
    printf("\nAll Test Cases Passed\n");
}
