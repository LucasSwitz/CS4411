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
int testPrependEmpty()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    queue_prepend(queue, item1);

    if (queue->item == item1)
    {
        return 0;
    }

    return -1;
}

// Test 3 : Prepend on Non Empty (3 items)
int testPrependNonEmpty()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    int *item2 = malloc(sizeof(int));
    int *item3 = malloc(sizeof(int));

    queue_prepend(queue, item3);
    queue_prepend(queue, item2);
    queue_prepend(queue, item1);

    if (queue->item == item1 && queue->next->item == item2 && queue->next->next->item == item3)
    {
        return 0;
    }

    return -1;
}
// Test 4: Prepend on NULL queue
int testPrependNullQueue()
{
    queue_t *queue = NULL;
    int *item1 = malloc(sizeof(int));

    queue_prepend(queue, item1);

    // check that nothing changes
    if (queue == NULL)
    {
        return 0;
    }
    return -1;
}

// Test 5 : append to an empty queue
int testAppendEmpty()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    queue_append(queue, item1);

    if (queue->item == item1)
    {
        return 0;
    }

    return -1;
}

// Test 6 : append on Non Empty (3 items)
int testAppendNonEmpty()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    int *item2 = malloc(sizeof(int));
    int *item3 = malloc(sizeof(int));

    queue_append(queue, item1);
    queue_append(queue, item2);
    queue_append(queue, item3);
    
    if (queue->item == item1 && queue->next->item == item2 && queue->next->next->item == item3)
    {
        return 0;
    }

    return -1;
}

// Test dequeue on single element list
int testDequeueSingle() {
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    void **returnVal = malloc(sizeof(int*));

    queue_append(queue, item1);
    queue_dequeue(queue, returnVal);
    if (queue->item == NULL && *(returnVal) == item1) {
        return 0;
    }
    return -1;
}

// Test multiple consecutive dequeue operations
int testDequeueOnMultiple() {
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    int *item2 = malloc(sizeof(int));
    void **returnVal = malloc(sizeof(int*));
    
    queue_append(queue, item1);
    queue_append(queue,item2);

    queue_dequeue(queue, returnVal); // first call of dequeue
    if (*(returnVal) != item1 || queue->item != item2 || queue->next != NULL) {
        return -1;
    }

    queue_dequeue(queue, returnVal);
    if (*(returnVal) != item2 || queue->item != NULL || queue->next != NULL) {
        return -1;
    }

    queue_dequeue(queue, returnVal);
    if (*(returnVal) != NULL) {
        return -1;
    }
    return 0;
}

// Test delete a single item
int testDeleteSingle() {
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    queue_delete(queue, item1);

    if (queue->item == NULL && queue->next == NULL) {
        return 0;
    }
    return -1;
}

// Test multiple deletion of elements
int testDeleteMultiple() {
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    int *item2 = malloc(sizeof(int));
    int *item3 = malloc(sizeof(int));

    queue_append(queue,item1);
    queue_append(queue,item2);
    queue_append(queue,item3);

    queue_delete(queue, item2); // remove from middle

    if (queue->item != item1 || queue->next->item != item3) {
        return -1;
    }

    queue_delete(queue, item3); // remove from back
    if (queue->item != item1 || queue->next != NULL) {
        return -1;
    }

    queue_delete(queue, item1);

    if (queue->item != NULL || queue->next != NULL) {
        return -1;
    }
    return 0;
}

// Test that queue "shifts" one over to the right when deleting the first element of a multiple element linked list
int testDeleteFirst() {
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    int *item2 = malloc(sizeof(int));

    queue_append(queue, item1);
    queue_append(queue,item2);
    queue_delete(queue,item1);

    if (queue->item == item2 && queue->next == NULL) {
        return 0;
    }
    return -1;
}

int countTestMultiple() {
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    int *item2 = malloc(sizeof(int));

    queue_append(queue,item1);
    queue_append(queue,item2);


    if (queue_length(queue) == 2) {
        return 0;
    }

    return -1;
}
int main(void)
{
    assert(test1() == 0);
    assert(testPrependEmpty() == 0);
    assert(testPrependNonEmpty() == 0);
    assert(testPrependNullQueue() == 0);
    assert(testAppendEmpty() == 0);
    assert(testAppendNonEmpty() == 0);
    assert(testDequeueSingle() == 0);
    assert(testDequeueOnMultiple() == 0);
    assert(testDeleteSingle()==0);
    assert(testDeleteMultiple() == 0);
    assert(testDeleteFirst() == 0);
    assert(countTestMultiple() == 0);
    printf("\nAll Test Cases Passed\n");
}
