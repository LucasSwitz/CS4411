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

// Test 2 : preprend to an empty queue- should succeed
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

// Test 3 : Prepend on Non Empty (3 items)- should succeed
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
// Test 4 : Prepend on NULL queue- should fail
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

//Test 5 : prepend null item- should fail
int testPrependNullItem()
{
    queue_t *queue = queue_new();
    int *item1 = NULL;

    queue_prepend(queue, item1);

    // check that nothing changes
    if (queue->item == NULL)
    {
        return 0;
    }
    return -1;
}

// Test 6 : append to an empty queue- should succeed
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

// Test 7 : append on Non Empty (3 items)- should succeed
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
// Test 8: Append on NULL queue- should fail
int testAppendNullQueue()
{
    queue_t *queue = NULL;
    int *item1 = malloc(sizeof(int));

    queue_append(queue, item1);

    // check that nothing changes
    if (queue == NULL)
    {
        return 0;
    }
    return -1;
}

//Test 9: append null item- should fail
int testAppendNullItem()
{
    queue_t *queue = queue_new();
    int *item1 = NULL;

    queue_append(queue, item1);

    // check that nothing changes
    if (queue->item == NULL)
    {
        return 0;
    }
    return -1;
}

// Test 10: dequeue on single element list- should succeed
int testDequeueSingle()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    void **returnVal = malloc(sizeof(int *));

    queue_append(queue, item1);
    queue_dequeue(queue, returnVal);
    if (queue->item == NULL && *(returnVal) == item1)
    {
        return 0;
    }
    return -1;
}
// Test 11: Dequeue on NULL queue- should fail
int testDequeueNullQueue()
{
    queue_t *queue = NULL;
    void **returnVal = malloc(sizeof(int *));

    queue_dequeue(queue, returnVal);

    // check that nothing changes
    if (*(returnVal) == NULL)
    {
        return 0;
    }
    return -1;
}

// Test 12: multiple consecutive dequeue operations- should succeed
int testDequeueOnMultiple()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    int *item2 = malloc(sizeof(int));
    void **returnVal = malloc(sizeof(int *));

    queue_append(queue, item1);
    queue_append(queue, item2);

    queue_dequeue(queue, returnVal); // first call of dequeue
    if (*(returnVal) != item1 || queue->item != item2 || queue->next != NULL)
    {
        return -1;
    }

    queue_dequeue(queue, returnVal);
    if (*(returnVal) != item2 || queue->item != NULL || queue->next != NULL)
    {
        return -1;
    }

    queue_dequeue(queue, returnVal);
    if (*(returnVal) != NULL)
    {
        return -1;
    }
    return 0;
}

// Test 13: delete a single item- should succeed
int testDeleteSingle()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    queue_delete(queue, item1);

    if (queue->item == NULL && queue->next == NULL)
    {
        return 0;
    }
    return -1;
}

// Test 14: multiple deletion of elements- should succeed
int testDeleteMultiple()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    int *item2 = malloc(sizeof(int));
    int *item3 = malloc(sizeof(int));

    queue_append(queue, item1);
    queue_append(queue, item2);
    queue_append(queue, item3);

    queue_delete(queue, item2); // remove from middle

    if (queue->item != item1 || queue->next->item != item3)
    {
        return -1;
    }

    queue_delete(queue, item3); // remove from back
    if (queue->item != item1 || queue->next != NULL)
    {
        return -1;
    }

    queue_delete(queue, item1);

    if (queue->item != NULL || queue->next != NULL)
    {
        return -1;
    }
    return 0;
}

//Test 15: Test that queue "shifts" one over to the right when deleting the first element of a multiple element linked list- should succeed
int testDeleteFirst()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    int *item2 = malloc(sizeof(int));

    queue_append(queue, item1);
    queue_append(queue, item2);
    queue_delete(queue, item1);

    if (queue->item == item2 && queue->next == NULL)
    {
        return 0;
    }
    return -1;
}

// Test 16: delete on NULL queue- should fail
int testDeleteNullQueue()
{
    queue_t *queue = NULL;
    int *item1 = malloc(sizeof(int));

    queue_delete(queue, item1);

    // check that nothing changes
    if (queue == NULL)
    {
        return 0;
    }
    return -1;
}
//Test 17: delete null item- should fail
int testDeleteNullItem()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    int *item2 = NULL;

    queue_append(queue, item1);
    queue_delete(queue, item2);

    // check that item1 wasn't deleted
    if (queue_delete(queue, item2) == -1 && queue->item == item1)
    {
        return 0;
    }
    return -1;
}
//Test 18: delete item not in queue- should fail
int testDeleteItemNotInQueue()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    int *item2 = malloc(sizeof(int));

    queue_append(queue, item1);
    queue_delete(queue, item2);

    // check that item1 wasn't deleted
    if (queue_delete(queue, item2) == -1 && queue->item == item1)
    {
        return 0;
    }
    return -1;
}
// Test 19: length of empty queue- should succeed
int countTestEmpty()
{
    queue_t *queue = queue_new();

    if (queue_length(queue) == 0)
    {
        return 0;
    }

    return -1;
}

// Test 20: length of queue with single item- should succeed
int countTestSingle()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));

    queue_append(queue, item1);

    if (queue_length(queue) == 1)
    {
        return 0;
    }

    return -1;
}
// Test 21: length of queue with multiple items- should succeed
int countTestMultiple()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    int *item2 = malloc(sizeof(int));

    queue_append(queue, item1);
    queue_append(queue, item2);

    if (queue_length(queue) == 2)
    {
        return 0;
    }

    return -1;
}
// Test 22: length of NULL queue- should fail
int countTestNull()
{
    queue_t *queue = NULL;

    if (queue_length(queue) == -1)
    {
        return 0;
    }

    return -1;
}

// Test 23: free empty queue- should succeed
int testFreeEmpty()
{
    queue_t *queue = queue_new();

    if (queue_free(queue) == 0)
    {
        return 0;
    }

    return -1;
}

// Test 24: free queue with single item- should fail
int testFreeSingle()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));

    queue_append(queue, item1);

    if (queue_free(queue) == -1)
    {
        return 0;
    }

    return -1;
}

// Test 25: free queue with multiple items- should fail
int testFreeMultiple()
{
    queue_t *queue = queue_new();
    int *item1 = malloc(sizeof(int));
    int *item2 = malloc(sizeof(int));

    queue_append(queue, item1);
    queue_append(queue, item2);

    if (queue_free(queue) == -1)
    {
        return 0;
    }

    return -1;
}

// Test 26: free NULL queue- should fail
int testFreeNull()
{
    queue_t *queue = NULL;
    if (queue_free(queue) == -1)
    {
        return 0;
    }

    return -1;
}

// iteration tests use the add function to add 1 to each item

void add(void *a, void *b)
{
    int **item = (int **)a;
    int *operand = (int *)b;
    int *itemVal = *(item);

    int *newVal = malloc(sizeof(int));
    *(newVal) = *(operand) + *(itemVal);
    *(item) = newVal;
}

// Test 27: iterate on queue of single item
int testIterateSingle()
{
    queue_t *queue = queue_new();
    int **item1 = malloc(sizeof(int *));
    int *nestedItem1 = malloc(sizeof(int));
    *(nestedItem1) = 0;
    *(item1) = nestedItem1;

    int *arg = malloc(sizeof(int));
    *(arg) = 1;

    queue_append(queue, item1);

    queue_iterate(queue, &add, arg);

    int **newItem1 = (int **)queue->item;
    if (**newItem1 == 1)
    {
        return 0;
    }
    return -1;
}

// Test 28: iterate on a queue with multiple items
int testIterateMultiple()
{
    queue_t *queue = queue_new();
    int **item1 = malloc(sizeof(int *));
    int *nestedItem1 = malloc(sizeof(int));
    *(nestedItem1) = 0;
    *(item1) = nestedItem1;

    int **item2 = malloc(sizeof(int *));
    int *nestedItem2 = malloc(sizeof(int));
    *(nestedItem2) = 1;
    *(item2) = nestedItem2;

    int *arg = malloc(sizeof(int));
    *(arg) = 1;

    queue_append(queue, item1);
    queue_append(queue, item2);

    queue_iterate(queue, &add, arg);

    int **newItem1 = (int **)queue->item;
    int **newItem2 = (int **)queue->next->item;
    if (**newItem1 == 1 && **newItem2 == 2)
    {
        return 0;
    }
    return -1;
}

// Test 29: iterate on null queue
int testIterateNullQueue()
{
    queue_t *queue = NULL;

    int *arg = malloc(sizeof(int));
    *(arg) = 1;

    if (queue_iterate(queue, &add, arg) == -1)
    {
        return 0;
    }
    return -1;
}

// Test 30: iterate on an empty queue
int testIterateEmpty()
{
    queue_t *queue = queue_new();

    int *arg = malloc(sizeof(int));
    *(arg) = 1;
    queue_iterate(queue, &add, arg);
    // nothing changes
    if (queue->item == NULL)
    {
        return 0;
    }
    return -1;
}

// Test 31: iterate on queue with NULL function
int testIterateNullFunction()
{
    queue_t *queue = queue_new();

    int *arg = malloc(sizeof(int));
    *(arg) = 1;

    if (queue_iterate(queue, NULL, arg) == -1)
    {
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
    assert(testPrependNullItem() == 0);
    assert(testAppendEmpty() == 0);
    assert(testAppendNonEmpty() == 0);
    assert(testAppendNullQueue() == 0);
    assert(testAppendNullItem() == 0);
    assert(testDequeueSingle() == 0); // 10
    assert(testDequeueNullQueue() == 0);
    assert(testDequeueOnMultiple() == 0);
    assert(testDeleteSingle() == 0);
    assert(testDeleteMultiple() == 0);
    assert(testDeleteFirst() == 0); //15
    assert(testDeleteNullQueue() == 0);
    assert(testDeleteNullItem() == 0);
    assert(testDeleteItemNotInQueue() == 0);
    assert(countTestEmpty() == 0);
    assert(countTestSingle() == 0); //20
    assert(countTestMultiple() == 0);
    assert(countTestNull() == 0);
    assert(testFreeEmpty() == 0);
    assert(testFreeSingle() == 0);
    assert(testFreeMultiple() == 0); // 25
    assert(testFreeNull() == 0);
    assert(testIterateSingle() == 0);
    assert(testIterateMultiple() == 0);
    assert(testIterateNullQueue() == 0);
    assert(testIterateEmpty() == 0);
    assert(testIterateNullFunction() == 0);
    printf("\nAll Test Cases Passed\n");
}
