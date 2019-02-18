/* Author: Robbert van Renesse, November 2015
 *
 * This block store module implements RAID0.
 *
 *		block_if raid0disk_init(block_if *below, unsigned int nbelow){
 *			'below' is an array of underlying block stores, all of which
 *			are assumed to be of the same size.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grass.h"
#include "block_store.h"

struct raid0disk_state {
	block_if *below;		// block stores below
	unsigned int nbelow;	// #block stores
};

static int raid0disk_nblocks(block_if bi){
	struct raid0disk_state *rds = bi->state;
	int total = 0;
	unsigned int i;

	for (i = 0; i < rds->nbelow; i++) {
		int r = (*rds->below[0]->nblocks)(rds->below[0]);
		if (r < 0) {
			return r;
		}
		total += r;
	}
	return total;
}

static int raid0disk_setsize(block_if bi, block_no nblocks){
	fprintf(stderr, "raid0disk_setsize: not yet implemented\n");
	return -1;
}

static int raid0disk_read(block_if bi, block_no offset, block_t *block){
	struct raid0disk_state *rds = bi->state;

	int i = offset % rds->nbelow;
	offset /= rds->nbelow;
	return (*rds->below[i]->read)(rds->below[i], offset, block);
}

static int raid0disk_write(block_if bi, block_no offset, block_t *block){
	struct raid0disk_state *rds = bi->state;

	int i = offset % rds->nbelow;
	offset /= rds->nbelow;
	return (*rds->below[i]->write)(rds->below[i], offset, block);
}

static void raid0disk_destroy(block_if bi){
	free(bi->state);
	free(bi);
}

block_if raid0disk_init(block_if *below, unsigned int nbelow){
	/* Create the block store state structure.
	 */
	struct raid0disk_state *rds = new_alloc(struct raid0disk_state);
	rds->below = below;
	rds->nbelow = nbelow;

	/* Return a block interface to this inode.
	 */
	block_if bi = new_alloc(block_store_t);
	bi->state = rds;
	bi->nblocks = raid0disk_nblocks;
	bi->setsize = raid0disk_setsize;
	bi->read = raid0disk_read;
	bi->write = raid0disk_write;
	bi->destroy = raid0disk_destroy;
	return bi;
}
