#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include "assert.h"
#include "egos.h"
#include "queue.h"

/* An element in a queue.
 */
struct element {
	struct element *next;
	void *item;
};

void queue_init(struct queue *q){
	q->first = 0;
	q->last = &q->first;
}

/* Put it on the wrong side of the queue.  I.e., make it the next
 * item to be returned.  Sort of like a stack...
 */
void queue_insert(struct queue *q, void *item){
	struct element *e = calloc(1, sizeof(*e));

	e->item = item;
	if (q->first == 0) {
		q->last = &e->next;
	}
	e->next = q->first;
	q->first = e;
}

#ifdef new_alloc
void queue_append(struct queue *q, void *item, char *file, int line){
	struct element *e = my_alloc(1, sizeof(*e), file, line, True);

	e->item = item;
	e->next = 0;
	*q->last = e;
	q->last = &e->next;
}

#else // !new_alloc

void queue_add(struct queue *q, void *item){
	struct element *e = calloc(1, sizeof(*e));

	e->item = item;
	e->next = 0;
	*q->last = e;
	q->last = &e->next;
}

void queue_add_uint(struct queue *q, unsigned int item){
	queue_add(q, (void *) (address_t) item);
}
#endif // new_alloc

void *queue_get(struct queue *q){
	void *item;
	struct element *e;

	if ((e = q->first) == 0) {
		return 0;
	}
	if ((q->first = e->next) == 0) {
		q->last = &q->first;
	}
	item = e->item;
	free(e);
	return item;
}

bool_t queue_tget(struct queue *q, void **item){
	struct element *e;

	if ((e = q->first) == 0) {
		return False;
	}
	if ((q->first = e->next) == 0) {
		q->last = &q->first;
	}
	*item = e->item;
	free(e);
	return True;
}

bool_t queue_get_uint(struct queue *q, unsigned int *item){
	void *x;
	bool_t r = queue_tget(q, &x);
	if (!r) {
		return False;
	}
	*item = (unsigned int) (address_t) x;
	return True;
}

bool_t queue_empty(struct queue *q){
	return q->first == 0;
}

void queue_release(struct queue *q){
	assert(q->first == 0);
}
