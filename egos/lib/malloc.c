#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>
#include "egos.h"

// #define UNIT_TEST	// define for testing only */

static char *base;

#ifdef UNIT_TEST

static char *brk;

#define SETBRK(b)		do { brk = (b); } while (0)
#define GETBRK()		brk

#else /* !UNIT_TEST */

#define SETBRK(b)		/* none */

extern int _end;

#endif /* UNIT_TEST */

enum m_status { M_LAST, M_FREE, M_INUSE };

/* The heap is organized as follows.  It is a sequence of alternating
 * blocks and headers, although a block can be of size 0 and thus
 * headers can be next to one another.  The sequence starts and ends
 * with a header.  The last header has status M_LAST; all others are
 * either M_FREE or M_INUSE.  An M_FREE or M_INUSE header describes
 * the block that follows it.  There cannot be two consecutive free
 * blocks.  A header specifies both the size of block (always a
 * multiple of 16), and the size of the block that precedes it.
 */
struct m_hdr {
	unsigned int size;		// size of this block
	unsigned int prev;		// size of preceding block
	unsigned int status;	// really enum m_status
	unsigned int align;		// to make header a multiple of 16 bytes
};
static bool_t initialized;

void *m_alloc(int size){
	/* Debugging */
	if (size <= 0) { * (int *) 1 = 1; }
	if (size > 10000000) { * (int *) 5 = 5; }

	struct m_hdr *mh, *next, *other;

	/* The first time, set up an initial free block.
	 */
	if (!initialized) {
		extern int *end_get(void);

		base = (char *) (end_get() + 1);

		/* Align to 16 bytes.
		 */
		base = (char *) (((address_t) base + 15) & ~ (address_t) 15);

		mh = (struct m_hdr *) base;
		SETBRK((char *) &mh[1] + PAGESIZE);
		mh->size = PAGESIZE - sizeof(*mh);
		mh->prev = 0;
		mh->status = M_FREE;
		next = (struct m_hdr *) ((char *) mh + PAGESIZE);
		next->size = 0;
		next->prev = PAGESIZE - sizeof(*mh);
		next->status = M_LAST;
		initialized = True;
	}

	mh = (struct m_hdr *) base;

	/* The header plus the size should be a multiple of 16 bytes to deal
	 * with 128-bit floating point numbers.
	 */
	size = (size + 15) & ~15;

	for (;;) {
		switch (mh->status) {
		case M_LAST:
			/* If the last block was free, skip back one.
			 */
			if (mh != (struct m_hdr *) base) {
				other = ((struct m_hdr *) ((char *) mh - mh->prev)) - 1;
				if (other->status == M_FREE) {
					mh = other;
				}
			}

			/* Now see if any other block came before.
			 */
			if (mh == (struct m_hdr *) base) {
				other = 0;
			}
			else {
				other = ((struct m_hdr *) ((char *) mh - mh->prev)) - 1;
			}

			/* Allocate enough and a bit more.
			 */
			SETBRK((char *) &mh[1] + size + PAGESIZE);
			mh->size = size + PAGESIZE - sizeof(*mh);
			mh->prev = other == 0 ? 0 : other->size;
			mh->status = M_FREE;
			next = (struct m_hdr *) ((char *) &mh[1] + mh->size);
			next->size = 0;
			next->prev = mh->size;
			next->status = M_LAST;
			/* FALL THROUGH */

		case M_FREE:
			/* If it fits exactly, just grab it.
			 */
			if (mh->size == size) {
				mh->status = M_INUSE;
				return mh + 1;
			}

			/* If it fits plus a new header, split it.
			 */
			if (mh->size >= size + sizeof(*mh)) {
				/* Keep track of the next header.
				 */
				next = (struct m_hdr *) ((char *) &mh[1] + mh->size);

				/* Insert a new header.
				 */
				other = (struct m_hdr *) ((char *) &mh[1] + size);
				other->size = mh->size - size - sizeof(*mh);
				other->prev = size;
				other->status = M_FREE;

				/* Update the following header.
				 */
				next->prev = other->size;

				/* Update the current header and return.
				 */
				mh->size = size;
				mh->status = M_INUSE;
				return mh + 1;
			}
			/* FALL THROUGH */

			/* If in use, or free and not big enough, skip to next.
			 */
		case M_INUSE:
			mh = (struct m_hdr *) ((char *) &mh[1] + mh->size);
			break;

		default:
			* (int *) 3 = 3;		// for debugging
		}
	}
}

void *m_calloc(int nitems, int size){
	size *= nitems;

	char *p = m_alloc(size);
	char *q = p;
	while (size-- > 0) {
		*q++ = 0;
	}
	return p;
}

void m_free(void *ptr){
	if (ptr == 0) {
		return;
	}

	struct m_hdr *mh = (struct m_hdr *) ptr - 1, *next, *other;

	/* See if we can merge with the next block.
	 */
	next = (struct m_hdr *) ((char *) ptr + mh->size);
	if (next->status == M_FREE) {
		other = (struct m_hdr *) ((char *) &next[1] + next->size);
		mh->size += sizeof(*next) + next->size;
		other->prev = mh->size;
		next = other;
	}

	/* See if we can merge with the prior block, if any.
	 */
	if (mh != (struct m_hdr *) base) {
		other = (struct m_hdr *) ((char *) mh - mh->prev) - 1;
		if (other->status == M_FREE) {
			other->size += mh->size + sizeof(*mh);
			next->prev = other->size;
			return;
		}
	}

	mh->status = M_FREE;
}

/* TODO.  Something smarter.
 */
void *m_realloc(void *ptr, int size){
	char *p = m_alloc(size), *q;

	if ((q = ptr) != 0) {
		struct m_hdr *mh = (struct m_hdr *) ptr - 1;
		if (size > mh->size) {
			size = mh->size;
		}

		char *r = p;
		while (size > 0) {
			*r++ = *q++;
			size--;
		}

		m_free(ptr);
	}
	return p;
}

#ifdef UNIT_TEST

/* Check the integrity of the data structure.
 */
void m_check(char *descr, int print){
	struct m_hdr *mh = (struct m_hdr *) base;
	unsigned int prev;
	enum m_status last;

	if (print) {
		printf("m_check %s: start\n", descr);
	}
	if (!initialized) {
		return;
	}
	if ((prev = mh->prev) != 0) {
		printf("m_check %s: first header has nonzero prev\n", descr);
		return;
	}
	if (mh->status == M_LAST) {
		printf("m_check %s: first header cannot be last one\n", descr);
		return;
	}
	for (;;) {
		if ((char *) &mh[1] > GETBRK()) {
			printf("m_check %s: beyond break\n", descr);
		}
		if (print) {
			printf("-> %"PRIaddr" size=%d prev=%d status=%d\n",
					(address_t) mh, mh->size, mh->prev, mh->status);
		}
		switch (mh->status) {
		case M_LAST: case M_FREE: case M_INUSE:
			break;
		default:
			printf("m_check %s: bad status\n", descr);
			return;
		}
		if (mh->prev != prev) {
			printf("m_check %s: bad prev\n", descr);
			return;
		}
		if (mh->status == M_FREE && last == M_FREE) {
			printf("m_check %s: consecutive free blocks\n", descr);
			return;
		}
		if (mh->status == M_LAST) {
			break;
		}
		prev = mh->size;
		mh = (struct m_hdr *) ((char *) &mh[1] + mh->size);
	}
	if ((char *) &mh[1] != GETBRK()) {
		printf("m_check %s: last header not at break\n", descr);
	}
	if (print) {
		printf("m_check %s: done\n", descr);
	}
}

int main(void){
	base = valloc(64 * PAGESIZE);
	brk = base + 100 * PAGESIZE;

	printf("hello world %"PRIaddr" %"PRIaddr"\n", (address_t) base, (address_t) brk);

	void *m0 = m_alloc(16);
	printf("m_alloc(16) --> %"PRIaddr"\n", (address_t) m0);
	m_check("m0 = m_alloc(16)", 1);

	void *m1 = m_alloc(16);
	printf("m_alloc(16) --> %"PRIaddr"\n", (address_t) m1);
	m_check("m1 = m_alloc(16)", 1);

	void *m2 = m_alloc(50 * PAGESIZE);
	printf("m_alloc(lots) --> %"PRIaddr"\n", (address_t) m2);
	m_check("m2 = m_alloc(lots)", 1);

	void *m3 = m_alloc(16);
	printf("m_alloc(16) --> %"PRIaddr"\n", (address_t) m3);
	m_check("m3 = m_alloc(16)", 1);

	m_free(m1);
	m_check("m_free(m1)", 1);

	m_free(m3);
	m_check("m_free(m3)", 1);

	m1 = m_alloc(64);
	printf("m_alloc(64) --> %"PRIaddr"\n", (address_t) m1);
	m_check("m_alloc(64)", 1);

	return 0;
}

#endif /* UNIT_TEST */
