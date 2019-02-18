/* Author: Robbert van Renesse, August 2015
 *
 * This virtual block store module turns a partition of the underlying block
 * store into a block store of its own.
 *
 *		block_if partdisk_init(block_if below, block_no delta, block_no nblocks)
 *			'below' is the underlying block store, and a new block store
 *			interface is returned.  Each read and write method on this 
 *			new block store is simply a call to the underlying one but with
 *			the offset increased by 'delta'.  'nblocks' is the number of
 *			blocks in the virtualized block store.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grass.h"
#include "block_store.h"

struct partdisk_state {
	block_if below;			// block store below
	block_no delta;			// offset in block store below
	block_no nblocks;		// size
};

static int partdisk_nblocks(block_if bi){
	struct partdisk_state *ps = bi->state;

	return ps->nblocks;
}

static int partdisk_setsize(block_if bi, block_no nblocks){
	struct partdisk_state *ps = bi->state;

	int before = ps->nblocks;
	ps->nblocks = nblocks;
	return before;
}

static int partdisk_read(block_if bi, block_no offset, block_t *block){
	struct partdisk_state *ps = bi->state;

	if (offset >= ps->nblocks) {
		fprintf(stderr, "partdisk_read: offset too large\n");
		return -1;
	}
	return (*ps->below->read)(ps->below, ps->delta + offset, block);
}

static int partdisk_write(block_if bi, block_no offset, block_t *block){
	struct partdisk_state *ps = bi->state;

	if (offset >= ps->nblocks) {
		fprintf(stderr, "partdisk_write: offset too large\n");
		return -1;
	}
	return (*ps->below->write)(ps->below, ps->delta + offset, block);
}

static void partdisk_destroy(block_if bi){
	free(bi->state);
	free(bi);
}

block_if partdisk_init(block_if below, block_no delta, block_no nblocks){
	/* Create the block store state structure.
	 */
	struct partdisk_state *ps = new_alloc(struct partdisk_state);
	ps->below = below;
	ps->delta = delta;
	ps->nblocks = nblocks;

	/* Return a block interface to this inode.
	 */
	block_if bi = new_alloc(block_store_t);
	bi->state = ps;
	bi->nblocks = partdisk_nblocks;
	bi->setsize = partdisk_setsize;
	bi->read = partdisk_read;
	bi->write = partdisk_write;
	bi->destroy = partdisk_destroy;
	return bi;
}
