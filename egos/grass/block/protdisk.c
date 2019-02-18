/* Author: Robbert van Renesse, July 2018
 *
 * This block store module forwards calls to a remote block server.
 *
 *		block_if protdisk_init(gpid_t below, unsigned int ino);
 *			'below' is the process identifier of the remote block store.
 *			'ino' is the inode number of the remote disk
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "grass.h"
#include "block_store.h"

struct protdisk_state {
	gpid_t below;			// process id of remote block store
	unsigned int ino;		// inode number of remote disk
};

static int protdisk_nblocks(block_if bi){
	struct protdisk_state *ps = bi->state;

	unsigned int nblocks;
	bool_t r = block_getsize(ps->below, ps->ino, &nblocks);
	assert(r);
	return nblocks;
}

static int protdisk_setsize(block_if bi, block_no nblocks){
	// struct protdisk_state *ps = bi->state;

	assert(0);
	return -1;
}

static int protdisk_read(block_if bi, block_no offset, block_t *block){
	struct protdisk_state *ps = bi->state;

	bool_t r = block_read(ps->below, ps->ino, offset, block);
	return r ? 0 : -1;
}

static int protdisk_write(block_if bi, block_no offset, block_t *block){
	struct protdisk_state *ps = bi->state;

	bool_t r = block_write(ps->below, ps->ino, offset, block);
	return r ? 0 : -1;
}

static void protdisk_destroy(block_if bi){
	// struct protdisk_state *ps = bi->state;

	free(bi->state);
	free(bi);
}

block_if protdisk_init(gpid_t below, unsigned int ino){
	/* Create the block store state structure.
	 */
	struct protdisk_state *ps = new_alloc(struct protdisk_state);
	ps->below = below;
	ps->ino = ino;

	/* Return a block interface to this inode.
	 */
	block_if bi = new_alloc(block_store_t);
	bi->state = ps;
	bi->nblocks = protdisk_nblocks;
	bi->setsize = protdisk_setsize;
	bi->read = protdisk_read;
	bi->write = protdisk_write;
	bi->destroy = protdisk_destroy;
	return bi;
}
