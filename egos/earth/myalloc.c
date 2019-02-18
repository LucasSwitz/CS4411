/* This is a wrapper around the C 'malloc' interface to make it easier to
 * find corruptions and memory leaks.
 */

#include <inttypes.h>
#include <stdio.h>
#include <ucontext.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include "earth.h"
#include "myalloc.h"

#undef malloc
#undef calloc
#undef realloc
#undef free

#define MAGIC_ALLOC		0x12345678
#define MAGIC_FREE		0x87654321
#define MAGIC_TAIL		"0123456789ABCDEF"

/* This is a list of all allocated memory.
 */
static struct my_header *my_list;

/* Header on each chunk of allocated memory.
 */
struct my_header {
	unsigned int magic;
	struct my_header *next, **back;
	size_t size;
	char *file;
	int line;
	char *free_file;
	int free_line;
};

/* Check for corruptions in allocated memory.
 */
void my_check(){
	struct my_header **pmh, *mh;

	for (pmh = &my_list; (mh = *pmh) != 0; pmh = &mh->next) {
		assert(mh->magic == MAGIC_ALLOC);
		assert(mh->free_file == 0);
		assert(mh->free_line == 0);
		assert(mh->back == pmh);

		char *tail = (char *) &mh[1] + mh->size;
		assert(memcmp(tail, MAGIC_TAIL, 16) == 0);
	}
}

/* Allocate total bytes with a header.
 */
static struct my_header *do_alloc(size_t total, char *file, int line){
	struct my_header *mh = malloc(sizeof(*mh) + total + 16);
	mh->magic = MAGIC_ALLOC;
	mh->size = total;
	mh->file = file;
	mh->line = line;
	mh->free_file = 0;
	mh->free_line = 0;

	if ((mh->next = my_list) != 0) {
		my_list->back = &mh->next;
	}
	mh->back = &my_list;
	my_list = mh;

	/* Add a tail.
	 */
	char *tail = (char *) &mh[1] + total;
	memcpy(tail, MAGIC_TAIL, 16);

	return mh;
}

/* Wrapper around malloc/calloc.
 */
void *my_alloc(size_t count, size_t size, char *file, int line, bool_t clr){
	size_t total = count * size;
	struct my_header *mh = do_alloc(total, file, line);
	if (clr) {
		memset(mh + 1, 0, total);
	}
	return mh + 1;
}

/* Allocation function used for new_alloc #defines.
 */
void *my_alloc_ext(size_t size, size_t ext, char *file, int line){
	size_t total = size + ext;
	struct my_header *mh = do_alloc(total, file, line);
	memset(mh + 1, 0, size);
	return mh + 1;
}

/* Print information about allocated area.
 */
void my_print(void *ptr){
	struct my_header *mh = (struct my_header *) ptr - 1;
	assert(mh->magic == MAGIC_ALLOC);
	printf(" loc = %s:%d\n", mh->file, mh->line);
	printf(" size = %lu\n", mh->size);
	char *tail = (char *) ptr + mh->size;
	assert(memcmp(tail, MAGIC_TAIL, 16) == 0);
}

/* Wrapper around free().
 */
void my_free(void *ptr, char *file, int line){
	if (ptr == 0) {
		return;
	}
	struct my_header *mh = (struct my_header *) ptr - 1;

	/* Sanity checks.
	 */
	assert(mh->magic == MAGIC_ALLOC);
	char *tail = (char *) ptr + mh->size;
	assert(memcmp(tail, MAGIC_TAIL, 16) == 0);

	if ((*mh->back = mh->next) != 0) {
		mh->next->back = mh->back;
	}
	mh->magic = MAGIC_FREE;
	mh->free_file = file;
	mh->free_line = line;

	/* Write junk over freed area including tail.
	 */
	char *stuff = (char *) &mh[1];
	unsigned int i;
	for (i = 0; i < mh->size + 16; i++) {
		*stuff++ = (char) i;
	}

	free(mh);
}

/* Wrapper around realloc().
 */
void *my_realloc(void *ptr, size_t size, char *file, int line){
	if (ptr == 0) {
		return my_alloc(1, size, file, line, False);
	}
	struct my_header *mh = (struct my_header *) ptr - 1;
if (mh->magic != MAGIC_ALLOC) printf("ABRT: %s %d %x %s %d %s %d\n", file, line, mh->magic, mh->file, mh->line, mh->free_file, mh->free_line);
	assert(mh->magic == MAGIC_ALLOC);
	struct my_header *mh2 = do_alloc(size, file, line);
	size_t total = mh->size;
	if (total > size) {
		total = size;
	}
	memcpy(mh2 + 1, ptr, total);
	my_free(ptr, file, line);
	return mh2 + 1;
}

/* Call this just before exiting.  It finds memory leaks, but is descructive.
 */
void my_dump(bool_t all){
	struct my_header *mh, *mh2;

	printf("\n\rPotential Memory Leaks:\n\r");
	bool_t found = False;

	for (mh = my_list; mh != 0; mh = mh->next) {
		assert(mh->magic == MAGIC_ALLOC);
		unsigned int count = 1;
		unsigned long total = mh->size;

		/* Find more entries allocated in same place.
		 */
		for (mh2 = mh->next; mh2 != 0; mh2 = mh2->next) {
			if (mh2->magic != MAGIC_ALLOC) {
				printf("--> %x %s:%d %s:%d\n", mh2->magic, mh2->file, mh2->line, mh2->free_file, mh2->free_line);
			}
			assert(mh2->magic == MAGIC_ALLOC);
			if (mh2->line == mh->line && strcmp(mh2->file, mh->file) == 0) {
				count++;
				total += mh2->size;
				if ((*mh2->back = mh2->next) != 0) {
					mh2->next->back = mh2->back;
				}
			}
		}
		if (count > 2 || all) {
			found = True;
			printf("-- %s:%d: count=%u total=%lu\n\r",
								mh->file, mh->line, count, total);
		}
	}
	if (!found) {
		printf("no significant memory leaks found\n\r");
	}
}
