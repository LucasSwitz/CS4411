/*
 * (C) 2017, Cornell University
 * All rights reserved.
 */

/* Author: Robbert van Renesse, August 2015, updated November 2015
 *
 * This code implements a set of virtualized block store on top of another
 *	block store.  Each virtualized block store is identified by a so-called
 *	"inode number", which indexes into an array of inodes.  The interface
 *	is as follows:
 *
 *		void treedisk_create(block_store_t *below, unsigned int n_inodes)
 *			Initializes the underlying block store "below" with a
 *			file system.  The file system consists of one "superblock",
 *			a number of blocks containing inodes, and the remaining
 *			blocks explained below.
 *
 *		block_store_t *treedisk_init(block_store_t *below, unsigned int inode_no)
 *			Opens a virtual block store at the given inode number.
 *
 * The layout of the file system is described in the file "treedisk.h".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "grass.h"
#include "block_store.h"
#include "treedisk.h"

/* Temporary information about the file system and a particular inode.
 * Convenient for all operations. See "treedisk.h" for field details.
 */
struct treedisk_snapshot {
	union treedisk_block superblock; 
	union treedisk_block inodeblock; 
	block_no inode_blockno;
	struct treedisk_inode *inode;
};

/* The state of a virtual block store, which is identified by an inode number.
 */
struct treedisk_state {
	block_store_t *below;			// block store below
	unsigned int inode_no;			// inode number in file system
};

static unsigned int log_rpb;		// log2(REFS_PER_BLOCK)
static block_t null_block;			// a block filled with null bytes

/* Stupid ANSI C compiler leaves shifting by #bits in unsigned int or more
 * undefined, but the result should clearly be 0...
 */
static block_no log_shift_r(block_no x, unsigned int nbits){
	if (nbits >= sizeof(block_no) * 8) {
		return 0;
	}
	return x >> nbits;
}

/* Get a snapshot of the file system, including the superblock and the block
 * containing the inode.
 */
static int treedisk_get_snapshot(struct treedisk_snapshot *snapshot,
								block_store_t *below, unsigned int inode_no){
	/* Get the superblock.
	 */
	if ((*below->read)(below, 0, (block_t *) &snapshot->superblock) < 0) {
		return -1;
	}

	/* Check the inode number.
	 */
	if (inode_no >= snapshot->superblock.superblock.n_inodeblocks * INODES_PER_BLOCK) {
		fprintf(stderr, "!!TDERR: inode number too large %u %u\n", inode_no, snapshot->superblock.superblock.n_inodeblocks);
		return -1;
	}

	/* Find the inode.
	 */
	snapshot->inode_blockno = 1 + inode_no / INODES_PER_BLOCK;
	if ((*below->read)(below, snapshot->inode_blockno, (block_t *) &snapshot->inodeblock) < 0) {
		return -1;
	}
	snapshot->inode = &snapshot->inodeblock.inodeblock.inodes[inode_no % INODES_PER_BLOCK];
	return 0;
}

/* Allocate a block from the free list.
 */
static block_no treedisk_alloc_block(block_store_t *below, struct treedisk_snapshot *snapshot){
	block_no b;

	if ((b = snapshot->superblock.superblock.free_list) == 0) {
		panic("treedisk_alloc_block: block store is full\n");
	}

	/* Read the freelist block and scan for a free block reference.
	 */
	union treedisk_block freelistblock;
	if ((*below->read)(below, b, (block_t *) &freelistblock) < 0) {
		panic("treedisk_alloc_block");
	}
	int i;
	for (i = REFS_PER_BLOCK; --i > 0;) {
		if (freelistblock.freelistblock.refs[i] != 0) {
			break;
		}
	}

	/* If there is a free block reference use that.  Otherwise use
	 * the free list block itself and update the superblock.
	 */
	block_no free_blockno;
	if (i == 0) {
		free_blockno = b;
		snapshot->superblock.superblock.free_list = freelistblock.freelistblock.refs[0];
		if ((*below->write)(below, 0, (block_t *) &snapshot->superblock) < 0) {
			panic("treedisk_alloc_block: superblock");
		}
	}
	else {
		free_blockno = freelistblock.freelistblock.refs[i];
		freelistblock.freelistblock.refs[i] = 0;
		if ((*below->write)(below, b, (block_t *) &freelistblock) < 0) {
			panic("treedisk_alloc_block: freelistblock");
		}
	}

	//fprintf(stdout, "### alloc: %d\n", free_blockno);
	return free_blockno;
}

/* Free a block to the free list.
 */
static void treedisk_free_block(block_store_t *below, struct treedisk_snapshot *snapshot,
	                                block_no target){
	//fprintf(stdout, "### free: %d\n", target);
	// write 0s to target block
	char zeros[BLOCK_SIZE];
	memset(zeros, 0, sizeof(zeros));
	if ((*below->write)(below, target, (block_t*) zeros) < 0) {
		panic("treedisk_free_block: target block");
	}

	// get the head of free list
	block_no b;
	if ((b = snapshot->superblock.superblock.free_list) == 0) {
		snapshot->superblock.superblock.free_list = target;
		if ((*below->write)(below, 0, (block_t*) &snapshot->superblock) < 0) {
			panic("treedisk_free_block: superblock");
		}
		return;
	}

	/* Read the freelist block and scan for an empty slot.
	 */
	union treedisk_block freelistblock;
	if ((*below->read)(below, b, (block_t *) &freelistblock) < 0) {
		panic("treedisk_alloc_block");
	}
	unsigned int i;
	for (i = 1; i < REFS_PER_BLOCK; i++) {
		if (freelistblock.freelistblock.refs[i] == 0) {
			break;
		}
	}

	if (i != REFS_PER_BLOCK) {
		// add target to existing slot
		freelistblock.freelistblock.refs[i] = target;
		if ((*below->write)(below, b, (block_t *) &freelistblock) < 0) {
			panic("treedisk_alloc_block: freelistblock");
		}
	} else {
		// target becomes the new head of freelist
		((struct treedisk_freelistblock *)(zeros))->refs[0] = b;
		snapshot->superblock.superblock.free_list = target;
		if ((*below->write)(below, 0, (block_t*) &snapshot->superblock) < 0) {
			panic("treedisk_free_block: superblock");
		}
		if ((*below->write)(below, target, (block_t*) zeros) < 0) {
			panic("treedisk_free_block: target block");
		}
	}
}


static void recursive_free_file(int level, int curr_level, block_store_t *below, block_no b, struct treedisk_snapshot *snapshot) {
	if (level == curr_level) {
		treedisk_free_block(below, snapshot, b);
		return;
	}

	union treedisk_block indirblock;
	if ((*below->read)(below, b, (block_t *) &indirblock) < 0) {
		panic("treedisk_alloc_block");
	}	
	unsigned int i;
	for (i = 0; i < REFS_PER_BLOCK; i++) {
		if (indirblock.indirblock.refs[i] != 0) {
			recursive_free_file(level, curr_level + 1, below, indirblock.indirblock.refs[i], snapshot);
		}
	}

	treedisk_free_block(below, snapshot, b);
}

/* Free all blocks in a file (inode) to the free list.
 */
static void treedisk_free_file(block_store_t *below, struct treedisk_snapshot *snapshot){
    //fprintf(stdout, "### Free file of size %d\n", snapshot->inode->nblocks);

	// already empty
	if (snapshot->inode->nblocks == 0) {
		return;
	}

	// direct
	if (snapshot->inode->nblocks == 1) {
		treedisk_free_block(below, snapshot, snapshot->inode->root);
		return;
	}

	// indirect
	unsigned int level = 1;
	unsigned int max_capacity = REFS_PER_BLOCK;
	while (max_capacity < snapshot->inode->nblocks) {
		level++;
		max_capacity *= REFS_PER_BLOCK;
	}

	recursive_free_file(level, 0, below, snapshot->inode->root, snapshot);
}


/* Retrieve the number of blocks in the file referenced by 'this_bs'.  This
 * information is maintained in the inode itself.
 */
static int treedisk_nblocks(block_store_t *this_bs){
	struct treedisk_state *ts = this_bs->state;

	struct treedisk_snapshot snapshot;
	if (treedisk_get_snapshot(&snapshot, ts->below, ts->inode_no) < 0) {
		return -1;
	}
	return snapshot.inode->nblocks; 
}



/* Set the size of the file 'this_bs' to 'nblocks'.
 */
static int treedisk_setsize(block_store_t *this_bs, block_no nblocks){
	struct treedisk_state *ts = this_bs->state;

	struct treedisk_snapshot snapshot;
	treedisk_get_snapshot(&snapshot, ts->below, ts->inode_no);
	if (nblocks == snapshot.inode->nblocks) {
		return nblocks;
	}
	if (nblocks > 0) {
		fprintf(stderr, "!!TDERR: nblocks > 0 not supported\n");
		return -1;
	}

	// TODO.  Release all the blocks used by this inode.
	treedisk_free_file(ts->below, &snapshot);

	snapshot.inode->nblocks = 0;
	snapshot.inode->root = 0;
	if ((*ts->below->write)(ts->below, snapshot.inode_blockno, (block_t*) &snapshot.inodeblock) < 0) {
		panic("treedisk_free_block: inode block");
	}
	return 0;

	fprintf(stderr, "!!TDERR: setsize not yet supported\n");
	return -1;
}

/* Read a block at the given block number 'offset' and return in *block.
 */
static int treedisk_read(block_store_t *this_bs, block_no offset, block_t *block){
	struct treedisk_state *ts = this_bs->state;

	/* Get info from underlying file system.
	 */
	struct treedisk_snapshot snapshot;
	if (treedisk_get_snapshot(&snapshot, ts->below, ts->inode_no) < 0) {
		return -1;
	}

	/* See if the offset is too big.
	 */
	if (offset >= snapshot.inode->nblocks) {
		fprintf(stderr, "!!TDERR: offset too large\n");
		return -1;
	}

	/* Figure out how many levels there are in the tree.
	 */
	unsigned int nlevels = 0;
	if (snapshot.inode->nblocks > 0) {
		while (log_shift_r(snapshot.inode->nblocks - 1, nlevels * log_rpb) != 0) {
			nlevels++;
		}
	}

	/* Walk down from the root block.
	 */
	block_no b = snapshot.inode->root;
	for (;;) {
		/* If there's a hole, return the null block.
		 */
		if (b == 0) {
			memset(block, 0, BLOCK_SIZE);
			return 0;
		}

		/* Return the next level.  If the last level, we're done.
		 */
		int result = (*ts->below->read)(ts->below, b, block);
		if (result < 0) {
			return result;
		}
		if (nlevels == 0) {
			return 0;
		}

		/* The block is an indirect block.  Figure out the index into this
		 * block and get the block number.
		 */
		nlevels--;
		struct treedisk_indirblock *tib = (struct treedisk_indirblock *) block;
		unsigned int index = log_shift_r(offset, nlevels * log_rpb) % REFS_PER_BLOCK;
		b = tib->refs[index];
	}
	return 0;
}

/* Write *block at the given block number 'offset'.
 */
static int treedisk_write(block_store_t *this_bs, block_no offset, block_t *block){
	struct treedisk_state *ts = this_bs->state;
	int dirty_inode = 0;

	/* Get info from underlying file system.
	 */
	struct treedisk_snapshot snapshot;
	if (treedisk_get_snapshot(&snapshot, ts->below, ts->inode_no) < 0) {
		return -1;
	}

	/* Figure out how many levels there are in the tree now.
	 */
	unsigned int nlevels = 0;
	if (snapshot.inode->nblocks > 0) {
		while (log_shift_r(snapshot.inode->nblocks - 1, nlevels * log_rpb) != 0) {
			nlevels++;
		}
	}

	/* Figure out how many levels we need after writing.  Files cannot shrink
	 * by writing.
	 */
	unsigned int nlevels_after;
	if (offset >= snapshot.inode->nblocks) {
		snapshot.inode->nblocks = offset + 1;
		dirty_inode = 1;
		nlevels_after = 0;
		while (log_shift_r(offset, nlevels_after * log_rpb) != 0) {
			nlevels_after++;
		}
	}
	else {
		nlevels_after = nlevels;
	}

	/* Grow the number of levels as needed by inserting indirect blocks.
	 */
	if (snapshot.inode->nblocks == 0) {
		nlevels = nlevels_after;
	}
	else if (nlevels_after > nlevels) {
		while (nlevels_after > nlevels) {
			block_no indir = treedisk_alloc_block(ts->below, &snapshot);

			/* Insert the new indirect block into the inode.
			 */
			struct treedisk_indirblock tib;
			memset(&tib, 0, BLOCK_SIZE);
			tib.refs[0] = snapshot.inode->root;
			snapshot.inode->root = indir;
			dirty_inode = 1;
			if ((*ts->below->write)(ts->below, indir, (block_t *) &tib) < 0) {
				panic("treedisk_write: indirect block");
			}

			nlevels++;
		}
	}

	/* If the inode block was updated, write it back now.
	 */
	if (dirty_inode) {
		if ((*ts->below->write)(ts->below, snapshot.inode_blockno, (block_t *) &snapshot.inodeblock) < 0) {
			panic("treedisk_write: inode block");
		}
	}

	/* Find the block by walking the tree, allocating new blocks
	 * (and indirect blocks) if necessary.
	 */
	block_no b;
	block_no *parent_no = &snapshot.inode->root;
	block_no parent_off = snapshot.inode_blockno;
	block_t *parent_block = (block_t *) &snapshot.inodeblock;
	for (;;) {
		/* Get or allocate the next block.
		 */
		struct treedisk_indirblock tib;
		if ((b = *parent_no) == 0) {
			b = *parent_no = treedisk_alloc_block(ts->below, &snapshot);
			if ((*ts->below->write)(ts->below, parent_off, parent_block) < 0) {
				panic("treedisk_write: parent");
			}
			if (nlevels == 0) {
				break;
			}
			memset(&tib, 0, BLOCK_SIZE);
		}
		else {
			if (nlevels == 0) {
				break;
			}
			if ((*ts->below->read)(ts->below, b, (block_t *) &tib) < 0) {
				panic("treedisk_write");
			}
		}

		/* Figure out the index into this block and get the block number.
		 */
		nlevels--;
		unsigned int index = log_shift_r(offset, nlevels * log_rpb) % REFS_PER_BLOCK;
		parent_no = &tib.refs[index];
		parent_block = (block_t *) &tib;
		parent_off = b;
	}
	if ((*ts->below->write)(ts->below, b, block) < 0) {
		panic("treedisk_write: data block");
	}
	return 0;
}

static void treedisk_destroy(block_store_t *this_bs){
	free(this_bs->state);
	free(this_bs);
}

/* Create or open a new virtual block store at the given inode number.
 */
block_store_t *treedisk_init(block_store_t *below, unsigned int inode_no){
	/* Figure out the log of the number of references per block.
	 */
	if (log_rpb == 0) {		// first time only
		do {
			log_rpb++;
		} while (((REFS_PER_BLOCK - 1) >> log_rpb) != 0);
	}

	/* Get info from underlying file system.
	 */
	struct treedisk_snapshot snapshot;
	if (treedisk_get_snapshot(&snapshot, below, inode_no) < 0) {
		return 0;
	}

	/* Create the block store state structure.
	 */
	struct treedisk_state *ts = new_alloc(struct treedisk_state);
	ts->below = below;
	ts->inode_no = inode_no;

	/* Return a block interface to this inode.
	 */
	block_store_t *this_bs = new_alloc(block_store_t);
	this_bs->state = ts;
	this_bs->nblocks = treedisk_nblocks;
	this_bs->setsize = treedisk_setsize;
	this_bs->read = treedisk_read;
	this_bs->write = treedisk_write;
	this_bs->destroy = treedisk_destroy;
	return this_bs;
}

/*************************************************************************
 * The code below is for creating new tree file systems.  This should
 * only be invoked once per underlying block store.
 ************************************************************************/

/* Create the free list and return the block number of the first
 * block on it.
 */
block_no setup_freelist(block_store_t *below, block_no next_free, block_no nblocks){
	block_no freelist_data[REFS_PER_BLOCK];
	block_no freelist_block = 0;
	unsigned int i;

	while (next_free < nblocks) {
		freelist_data[0] = freelist_block;
		freelist_block = next_free++;
		for (i = 1; i < REFS_PER_BLOCK && next_free < nblocks; i++) {
			freelist_data[i] = next_free++;
		}
		for (; i < REFS_PER_BLOCK; i++) {
			freelist_data[i] = 0;
		}
		if ((*below->write)(below, freelist_block, (block_t *) freelist_data) < 0) {
			panic("treedisk_setup_freelist");
		}
	}
	return freelist_block;
}

/* Create a new file system on the block store below.
 */
int treedisk_create(block_store_t *below, unsigned int n_inodes){
	if (sizeof(union treedisk_block) != BLOCK_SIZE) {
		panic("treedisk_create: block has wrong size");
	}

	/* Compute the number of inode blocks needed to store the inodes.
	 */
	unsigned int n_inodeblocks =
					(n_inodes + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK;

	/* Get the size of the underlying disk and see if it's large enough.
	 */
	unsigned int nblocks = (*below->nblocks)(below);
	if (nblocks < n_inodeblocks + 2) {
		fprintf(stderr, "treedisk_create: too few blocks\n");
		return -1;
	}

	/* Read the superblock to see if it's already initialized.
	 */
	union treedisk_block superblock;
	if ((*below->read)(below, 0, (block_t *) &superblock) < 0) {
		return -1;
	}
	if (superblock.superblock.n_inodeblocks == 0) {
		/* Initialize the superblock.
		 */
		union treedisk_block superblock;
		memset(&superblock, 0, BLOCK_SIZE);
		superblock.superblock.n_inodeblocks = n_inodeblocks;
		superblock.superblock.free_list =
					setup_freelist(below, n_inodeblocks + 1, nblocks);
		if ((*below->write)(below, 0, (block_t *) &superblock) < 0) {
			return -1;
		}

		/* The inodes all start out empty.
		 */
		unsigned int i;
		for (i = 1; i <= n_inodeblocks; i++) {
			if ((*below->write)(below, i, &null_block) < 0) {
				return -1;
			}
		}
	}
	else {
		assert(superblock.superblock.n_inodeblocks == n_inodeblocks);
	}

	return 0;
}
