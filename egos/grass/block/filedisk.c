/*
 * (C) 2017, Cornell University
 * All rights reserved.
 */

/* Author: Robbert van Renesse, August 2015
 *
 * This code implements a block store on top of the underlying POSIX
 * file system:
 *
 *		block_store_t *filedisk_init(char *file_name, block_no nblocks, bool_t sync)
 *			Create a new block store, stored in the file by the given
 *			name and with the given number of blocks.  If sync is set, invoke
 *			fsync for every disk write.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "grass.h"
#include "block_store.h"

struct filedisk_state {
	block_no nblocks;			// #blocks in the block store
	bool_t sync;				// sync to disk after each write
	int fd;						// POSIX file descriptor of underlying file
};

static int filedisk_nblocks(block_store_t *this_bs){
	struct filedisk_state *ds = this_bs->state;

	return ds->nblocks;
}

int ftruncate(int fd, off_t length);

static int filedisk_setsize(block_store_t *this_bs, block_no nblocks){
	struct filedisk_state *ds = this_bs->state;

	int before = ds->nblocks;
	ds->nblocks = nblocks;
	ftruncate(ds->fd, (off_t) nblocks * BLOCK_SIZE);
	return before;
}

static struct filedisk_state *filedisk_seek(block_store_t *this_bs, block_no offset){
	struct filedisk_state *ds = this_bs->state;

	if (offset >= ds->nblocks) {
		fprintf(stderr, "--> %u %u\n", offset, ds->nblocks);
		panic("filedisk_seek: offset too large");
	}
	lseek(ds->fd, (off_t) offset * BLOCK_SIZE, SEEK_SET);
	return ds;
}

static int filedisk_read(block_store_t *this_bs, block_no offset, block_t *block){
	struct filedisk_state *ds = filedisk_seek(this_bs, offset);

	int n = read(ds->fd, (void *) block, BLOCK_SIZE);
	if (n < 0) {
		perror("filedisk_read");
		return -1;
	}
	if (n < BLOCK_SIZE) {
		memset((char *) block + n, 0, BLOCK_SIZE - n);
	}
	return 0;
}

static int filedisk_write(block_store_t *this_bs, block_no offset, block_t *block){
	struct filedisk_state *ds = filedisk_seek(this_bs, offset);

	int n = write(ds->fd, (void *) block, BLOCK_SIZE);
	if (n < 0) {
		perror("filedisk_write");
		return -1;
	}
	if (n != BLOCK_SIZE) {
		fprintf(stderr, "filedisk_write: wrote only %d bytes\n", n);
		return -1;
	}
	if (ds->sync) {
		fsync(ds->fd);
	}
	return 0;
}

static void filedisk_destroy(block_store_t *this_bs){
	struct filedisk_state *ds = this_bs->state;

	close(ds->fd);
	free(ds);
	free(this_bs);
}

block_store_t *filedisk_init(const char *file_name, block_no nblocks, bool_t sync){
	fprintf(stderr, "filedisk_init: use of this device is now discouraged\n");

	struct filedisk_state *ds = new_alloc(struct filedisk_state);

	/* Open the disk.  Create if non-existent.
	 */
	ds->fd = open(file_name, O_RDWR, 0600);
	if (ds->fd < 0) {
		ds->fd = open(file_name, O_RDWR | O_CREAT, 0600);

		if (ds->fd < 0) {
			perror(file_name);
			panic("filedisk_init");
		}
	}
	ds->nblocks = nblocks;
	ds->sync = sync;

	block_store_t *this_bs = new_alloc(block_store_t);
	this_bs->state = ds;
	this_bs->nblocks = filedisk_nblocks;
	this_bs->setsize = filedisk_setsize;
	this_bs->read = filedisk_read;
	this_bs->write = filedisk_write;
	this_bs->destroy = filedisk_destroy;
	return this_bs;
}
