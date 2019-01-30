/*
 * Generic queue implementation.
 *
 */
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

queue_t  *queue_new() {
    // malloc returns NULL on error
    queue_t *new_queue = malloc(sizeof(queue_t));
    return new_queue;
}

// when else do we need -1? maybe if item is null, next is not null? check with TA
int queue_prepend(queue_t *queue, void *item)
{
    // should we throw -1 here? or just create queue and keep going
    if (queue == NULL || item == NULL)
    {
        return -1;
    }
    // empty queue
    if (queue->item == NULL)
    {
        queue->item = item;
        return 0;
    }
    // non-empty queue
    void *temp_item = queue->item;
    void *temp_next = queue->next;
    queue->item = item;
    queue_t *new_queue = queue_new();
    if (new_queue == NULL)
    {
        return -1;
    }
    new_queue->item = temp_item;
    queue->next = new_queue;
    new_queue->next = temp_next;
    return 0;
}

// ATTENTION: CHECK WHILE LOOP LOGIC. MAYBE NEED A TEMP VARIABLE, WILL QUEUE BE SET TO LAST ELEMENT AFTER ITERATION?
int queue_append(queue_t *queue, void *item)
{
    if (queue == NULL || item == NULL)
    {
        return -1;
    }
    // empty queue
    if (queue->item == NULL)
    {
        queue->item = item;
        return 0;
    }
    // non-empty queue
    while (queue->next != NULL)
    {
        queue = queue->next;
    }
    queue_t *new_queue = queue_new();
    if (new_queue == NULL)
    {
        return -1;
    }
    new_queue->item = item;
    queue->next = new_queue;
    return 0;
}

int queue_dequeue(queue_t *queue, void **item)
{
    // do we need to check if item is NULL?
    if (queue == NULL || queue->item)
    {
        *(item) = NULL;
        return -1;
    }
    *(item) = queue->item;

    // DOES THIS ACTUALLY WORK? IF THIS WORKS FIX QUEUE_APPEND- SEE ABOVE COMMENT
    queue = queue->next;
    return 0;
}

// ATTENTION: CHECK WHILE LOOP LOGIC. MAYBE NEED A TEMP VARIABLE, WILL QUEUE BE SET TO LAST ELEMENT AFTER ITERATION?
int queue_iterate(queue_t *queue, func_t f, void *arg)
{
    if (queue == NULL || f == NULL)
    {
        return -1;
    }
    while (queue != NULL)
    {
        // IS THIS RIGHT??? double check with TAs. Do we need to dereference?
        (*f)(queue->item, arg);
        queue = queue->next;
    }
    return 0;
}

int queue_free(queue_t *queue)
{
    if (queue == NULL || queue->item != NULL)
    {
        return -1;
    }
    free(queue);
    return 0;
}

int queue_length(const queue_t *queue)
{
    if (queue == NULL)
    {
        return -1;
    }
    int count = 0;
    while (queue != NULL)
    {
        count++;
        queue = queue->next;
    }
    return count;
}

//RESET QUEUE AFTER UPDA?
int queue_delete(queue_t *queue, void *item)
{
    if (queue == NULL) {
        return -1;
    }
    
    if (queue->item == item) { // case where first item is the item to remove
        queue = queue->next;
        return 0;
    }
    while (queue->next != NULL) {
        if (queue->next->item == item) {
            queue->next = queue->next->next;
            return 0;
        }
    }
    return -1; // item not found in queue
}