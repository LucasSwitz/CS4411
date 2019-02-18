/* Simple queue interface.
 */

#ifdef new_alloc
#define queue_add(q, e)			queue_append((q), e, __FILE__, __LINE__)
#define queue_add_uint(q, e)	queue_append((q), (void *) (address_t) (e), __FILE__, __LINE__)
#endif

struct queue {
	struct element *first, **last;
	int nelts;
};

void queue_init(struct queue *q);
void queue_insert(struct queue *q, void *item);
void queue_append(struct queue *q, void *, char *file, int line);

#ifndef new_alloc
void queue_add(struct queue *q, void *);
void queue_add_uint(struct queue *q, unsigned int);
#endif

void *queue_get(struct queue *q);
bool_t queue_tget(struct queue *q, void **item);
bool_t queue_get_uint(struct queue *q, unsigned int *);
bool_t queue_empty(struct queue *q);
void queue_release(struct queue *q);
