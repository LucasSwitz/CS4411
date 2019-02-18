/* Author: Robbert van Renesse, August 2015
 *
 * This block store module simply forwards its method calls to an
 * underlying block store, but prints out invocations and returns
 * for debugging purposes:
 *
 *		block_if debugdisk_init(block_if below, char *descr);
 *			'below' is the underlying block store.  'descr' is included
 *			in print statements in case there are multiple debug modules
 *			active.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grass.h"
#include "block_store.h"

struct debugdisk_state {
	block_if below;			// block store below
	char *descr;			// description of underlying block store.
};

static int debugdisk_nblocks(block_if bi){
	struct debugdisk_state *ds = bi->state;

	fprintf(stderr, "%s: invoke nblocks()\n", ds->descr);
	int nblocks = (*ds->below->nblocks)(ds->below);
	fprintf(stderr, "%s: nblocks() --> %d\n", ds->descr, nblocks);
	return nblocks;
}

static int debugdisk_setsize(block_if bi, block_no nblocks){
	struct debugdisk_state *ds = bi->state;

	fprintf(stderr, "%s: invoke setsize(%u)\n", ds->descr, nblocks);
	int r = (*ds->below->setsize)(ds->below, nblocks);
	fprintf(stderr, "%s: setsize(%u) --> %d\n", ds->descr, nblocks, r);
	return r;
}

static int debugdisk_read(block_if bi, block_no offset, block_t *block){
	struct debugdisk_state *ds = bi->state;

	fprintf(stderr, "%s: invoke read(offset = %u)\n", ds->descr, offset);
	int r = (*ds->below->read)(ds->below, offset, block);
	fprintf(stderr, "%s: read(offset = %u) --> %d\n", ds->descr, offset, r);
	return r;
}

static int debugdisk_write(block_if bi, block_no offset, block_t *block){
	struct debugdisk_state *ds = bi->state;

	fprintf(stderr, "%s: invoke write(offset = %u)\n", ds->descr, offset);
	int r = (*ds->below->write)(ds->below, offset, block);
	fprintf(stderr, "%s: write(offset = %u) --> %d\n", ds->descr, offset, r);
	return r;
}

static void debugdisk_destroy(block_if bi){
	struct debugdisk_state *ds = bi->state;

	fprintf(stderr, "%s: destroy()\n", ds->descr);
	free(bi->state);
	free(bi);
}

block_if debugdisk_init(block_if below, char *descr){
	/* Create the block store state structure.
	 */
	struct debugdisk_state *ds = new_alloc(struct debugdisk_state);
	ds->below = below;
	ds->descr = descr;

	/* Return a block interface to this inode.
	 */
	block_if bi = new_alloc(block_store_t);
	bi->state = ds;
	bi->nblocks = debugdisk_nblocks;
	bi->setsize = debugdisk_setsize;
	bi->read = debugdisk_read;
	bi->write = debugdisk_write;
	bi->destroy = debugdisk_destroy;
	return bi;
}
