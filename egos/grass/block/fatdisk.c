#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "grass.h"
#include "block_store.h"

#ifdef HW_FS	
#include "fatdisk.h"

/* Temporary information about the file system and a particular inode.
 * Convenient for all operations. See "fatdisk.h" for field details.
 */
struct fatdisk_snapshot {
    union fatdisk_block superblock;
    union fatdisk_block inodeblock;
    block_no inode_blockno;
    unsigned int inode_no;
    struct fatdisk_inode *inode;
};

struct fatdisk_state {
    block_store_t *below;   // block store below
    unsigned int inode_no;  // inode number in file
};


static int fatdisk_get_snapshot(struct fatdisk_snapshot *snapshot, 
                                block_store_t *below, unsigned int inode_no) {
    snapshot->inode_no = inode_no;

    /* Get the super block.
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
    snapshot->inode = &(snapshot->inodeblock.inodeblock.inodes[inode_no % INODES_PER_BLOCK]);

    return 0;
}

/* Create a new FAT file system on the block store below
 */
int fatdisk_create(block_store_t *below, unsigned int n_inodes) {
    /* Your code goes here:
     */
    panic("Fatdisk create not implemented!");
}


static void fatdisk_free_file(struct fatdisk_snapshot *snapshot, 
                                      block_store_t *below) {
    /* Your code goes here:
     */
}


/* Write *block at the given block number 'offset'.
 */
static int fatdisk_write(block_store_t *this_bs, block_no offset, block_t *block) {
    /* Your code goes here:
     */
    return 0;
}

/* Read a block at the given block number 'offset' and return in *block.
 */
static int fatdisk_read(block_store_t *this_bs, block_no offset, block_t *block){
    /* Your code goes here:
     */
    return 0;
}


/* Get size.
 */
static int fatdisk_nblocks(block_store_t *this_bs){
    struct fatdisk_state *fs = this_bs->state;

    /* Get info from underlying file system.
     */
    struct fatdisk_snapshot snapshot;
    if (fatdisk_get_snapshot(&snapshot, fs->below, fs->inode_no) < 0) {
        return -1;
    }

    return snapshot.inode->nblocks;
}

/* Set the size of the file 'this_bs' to 'nblocks'.
 */
static int fatdisk_setsize(block_store_t *this_bs, block_no nblocks){
    struct fatdisk_state *fs = this_bs->state;

    struct fatdisk_snapshot snapshot;
    fatdisk_get_snapshot(&snapshot, fs->below, fs->inode_no);
    if (nblocks == snapshot.inode->nblocks) {
        return nblocks;
    }
    if (nblocks > 0) {
        fprintf(stderr, "!!TDERR: nblocks > 0 not supported\n");
        return -1;
    }

    fatdisk_free_file(&snapshot, fs->below);
    return 0;
}

static void fatdisk_destroy(block_store_t *this_bs){
    free(this_bs->state);
    free(this_bs);
}


/* Create or open a new virtual block store at the given inode number.
 */
block_store_t *fatdisk_init(block_store_t *below, unsigned int inode_no){
    /* Get info from underlying file system.
     */
    struct fatdisk_snapshot snapshot;
    if (fatdisk_get_snapshot(&snapshot, below, inode_no) < 0) {
        return 0;
    }

    /* Create the block store state structure.
     */
    struct fatdisk_state *fs = new_alloc(struct fatdisk_state);
    fs->below = below;
    fs->inode_no = inode_no;

    /* Return a block interface to this inode.
     */
    block_store_t *this_bs = new_alloc(block_store_t);
    this_bs->state = fs;
    this_bs->nblocks = fatdisk_nblocks;
    this_bs->setsize = fatdisk_setsize;
    this_bs->read = fatdisk_read;
    this_bs->write = fatdisk_write;
    this_bs->destroy = fatdisk_destroy;
    return this_bs;
}

#endif
