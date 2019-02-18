/* Intercept all memory allocations.
 */
#define malloc(s)			my_alloc(1, (s), __FILE__, __LINE__, False)
#define calloc(c, s)		my_alloc((c), (s), __FILE__, __LINE__, True)
#define realloc(p, s)		my_realloc((p), (s), __FILE__, __LINE__)
#define free(p)				my_free((p), __FILE__, __LINE__)

/* Usage:
 *
 *	new_alloc(<type>) allocates sizeof(<type>) bytes and returns a typed
 *		pointer to this area.  The area is set to all zeroes.
 *	new_alloc_ext(<type>, ext) allocates sizeof(<type>) + ext bytes and
 *		returns a typed pointer to this area.  The first sizeof(<type>)
 *		bytes are set to zero.
 *	my_print(p) prints information about allocated memory.
 *  my_check() looks for corruptions in the allocated memory.
 *  my_dump() should be called at the very end and looks for memory leaks.
 *		Note that my_dump() is descructive and cannot be called at any time.
 */

#define new_alloc(t)		((t *) my_alloc_ext(sizeof(t), 0, __FILE__, __LINE__))
#define new_alloc_ext(t,x)	((t *) my_alloc_ext(sizeof(t), (x), __FILE__, __LINE__))

void my_check();
void *my_alloc(size_t count, size_t size, char *file, int line, bool_t clr);
void *my_alloc_ext(size_t size, size_t ext, char *file, int line);
void my_free(void *ptr, char *file, int line);
void *my_realloc(void *ptr, size_t size, char *file, int line);
void my_dump(bool_t all);
void my_print(void *ptr);
