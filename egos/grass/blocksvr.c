#include <inttypes.h>
#include <stdio.h>
#include <ucontext.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <arpa/inet.h>
#include "../earth/earth.h"
#include "../earth/myalloc.h"
#include "../earth/log.h"
#include "../earth/tlb.h"
#include "../earth/intr.h"
#include "../earth/clock.h"
#include "../earth/mem.h"
#include "../shared/syscall.h"
#include "../shared/dir.h"
#include "../shared/block.h"
#include "../shared/queue.h"
#include "exec.h"
#include "process.h"
#include "blocksvr.h"
// header file for block store
#include "block/block_store.h"

#define DISK_SIZE		(16 * 1024)     // size of "physical" disk in blocks
#define NCACHE_BLOCKS	20				// size of cache

/* State of the block server.
 */
struct block_server_state {
	char *type;
	unsigned int n_inodes;
	block_store_t **inodes;
};

// these helper functions are declared here and defined later
static void block_do_read(struct block_server_state *bss, struct block_request *req, gpid_t src);
static void block_do_write(struct block_server_state *bss, struct block_request *req, void *data, unsigned int nblock, gpid_t src);
static void block_do_getsize(struct block_server_state *bss, struct block_request *req, gpid_t src);
static void block_do_setsize(struct block_server_state *bss, struct block_request *req, gpid_t src);

static void block_cleanup(void *arg){
	struct block_server_state *bss = arg;

	printf("block server: cleaning up\n\r");

	unsigned int i;
	for (i = 0; i < bss->n_inodes; i++) {
		block_store_t *bs = bss->inodes[i];
		(*bs->destroy)(bs);
	}

	free(bss);
}

static void block_proc(void *arg){
	struct block_server_state *bss = arg;

    printf("%s BLOCK SERVER: %u\n\r", bss->type, sys_getpid());

	proc_current->finish = block_cleanup;

    struct block_request *req = new_alloc_ext(struct block_request, PAGESIZE);
    for (;;) {
        gpid_t src;
        int req_size = sys_recv(MSG_REQUEST, 0, req, sizeof(req) + PAGESIZE, &src);
		if (req_size < 0) {
			printf("%s block server shutting down\n\r", bss->type);
			free(bss);
			free(req);
			break;
		}

        assert(req_size >= (int) sizeof(*req));

        switch (req->type) {
            case BLOCK_READ:
                //fprintf(stderr, "!!DEBUG: calling block read\n");
                block_do_read(bss, req, src);
                break;
            case BLOCK_WRITE:
                //fprintf(stderr, "!!DEBUG: calling block write: %u %u %u\n", req_size, sizeof(*req), BLOCK_SIZE);
                block_do_write(bss, req, &req[1], (req_size - sizeof(*req)) / BLOCK_SIZE, src);
                break;
            case BLOCK_GETSIZE:
                //fprintf(stderr, "!!DEBUG: calling block getsize\n");
                block_do_getsize(bss, req, src);
                break;
            case BLOCK_SETSIZE:
                //fprintf(stderr, "!!DEBUG: calling block setsize\n");
                block_do_setsize(bss, req, src);
                break;
			default:
				assert(0);
		}
    }
}

// Size of paging partition on disk
#define PG_DEV_BLOCKS		((PG_DEV_SIZE * PAGESIZE) / BLOCK_SIZE)

/* Create a new block device.
 */
gpid_t block_init(char *type, gpid_t below){
	struct block_server_state *bss = new_alloc(struct block_server_state);

	/* The "physical" disk is partitioned into a paging partition (inode 0)
	 * and the file system partition (inode 1).
	 */
	if (strcmp(type, "phys") == 0) {
		bss->type = "PHYS";
		block_store_t *physdisk = protdisk_init(below, 0);
		bss->n_inodes = 2;
		bss->inodes = calloc(bss->n_inodes, sizeof(*bss->inodes));
		bss->inodes[PAGE_PARTITION] = partdisk_init(physdisk, 0, PG_DEV_BLOCKS);
		bss->inodes[FILE_PARTITION] = partdisk_init(physdisk, PG_DEV_BLOCKS,
												DISK_SIZE - PG_DEV_BLOCKS);
		return proc_create(1, "phys block", block_proc, bss);
	}
	if (strcmp(type, "virt") == 0) {
		bss->type = "VIRT";
		bss->n_inodes = MAX_INODES;
		bss->inodes = calloc(bss->n_inodes, sizeof(*bss->inodes));
		block_store_t *physdisk = protdisk_init(below, FILE_PARTITION);

		block_t *cache = malloc(NCACHE_BLOCKS * BLOCK_SIZE);
		block_store_t *cachedisk = clockdisk_init(physdisk, cache, NCACHE_BLOCKS);


		/* Virtualize the store, creating a collection of MAX_INODES virtual stores.
		 */
#ifdef HW_FS
		if (fatdisk_create(cachedisk, MAX_INODES) < 0) {
			panic("block_init: can't create fatdisk file system");
		}
		unsigned int inode;
		for (inode = 0; inode < MAX_INODES; inode++) {
			bss->inodes[inode] = fatdisk_init(cachedisk, inode);
		}
#else
		if (treedisk_create(cachedisk, MAX_INODES) < 0) {
			panic("block_init: can't create treedisk file system");
		}
		unsigned int inode;
		for (inode = 0; inode < MAX_INODES; inode++) {
			bss->inodes[inode] = treedisk_init(cachedisk, inode);
		}
#endif
		return proc_create(1, "virt block", block_proc, bss);
	}
	assert(0);
}

static void block_respond(struct block_request *req, enum block_status status,
                void *data, unsigned int nblock, gpid_t src){
    struct block_reply *rep = new_alloc_ext(struct block_reply, nblock * BLOCK_SIZE);
    rep->status = status;
    memcpy(&rep[1], data, nblock * BLOCK_SIZE);
    sys_send(src, MSG_REPLY, rep, sizeof(*rep) + nblock * BLOCK_SIZE);
	free(rep);
}

/* Respond to a read block request.
 */
static void block_do_read(struct block_server_state *bss, struct block_request *req, gpid_t src){
    // req->ino
    // req->offset_nblock
    if (req->ino >= MAX_INODES) {
        printf("block_do_read: bad inode: %u\n", req->ino);
        block_respond(req, BLOCK_ERROR, 0, 0, src);
        return;
    }

    /* Allocate room for the reply.
     */
    struct block_reply *rep = new_alloc_ext(struct block_reply, BLOCK_SIZE);

    /* Read the block from block store
     */
    int result;
    block_t *buffer = (block_t*)(&rep[1]);
    block_store_t *virt = bss->inodes[req->ino];

    result = (*virt->read)(virt, req->offset_nblock, buffer);
    if (result < 0) {
        printf("block_do_read: bad offset: %u in inode %u\n", req->offset_nblock, req->ino);
        block_respond(req, BLOCK_ERROR, 0, 0, src);
    }
	else {
		rep->status = BLOCK_OK;
		rep->size_nblock = 1;
		sys_send(src, MSG_REPLY, rep, sizeof(*rep) + BLOCK_SIZE);
	}
	free(rep);
}

/* Respond to a write block request.
 */
static void block_do_write(struct block_server_state *bss, struct block_request *req, void *data, unsigned int nblock, gpid_t src){
    // req->ino
    // req->offset_nblock
    if (req->ino >= MAX_INODES) {
        printf("block_do_write: bad inode %u\n", req->ino);
        block_respond(req, BLOCK_ERROR, 0, 0, src);
        return;
    }

    if (nblock != 1) {
        printf("block_do_write: size mismatch %u %u\n", 1, nblock);
        block_respond(req, BLOCK_ERROR, 0, 0, src);
        return;
    }

    int result;
    block_t *buffer = (block_t*)(data);
    block_store_t *virt = bss->inodes[req->ino];

    result = (*virt->write)(virt, req->offset_nblock, buffer);
    if (result < 0) {
        printf("block_do_write: bad offset: %u in inode %u\n", req->offset_nblock, req->ino);
        block_respond(req, BLOCK_ERROR, 0, 0, src);
        return;
    }

    block_respond(req, BLOCK_OK, 0, 0, src);
}

/* Respond to a getsize block request.
 */
static void block_do_getsize(struct block_server_state *bss, struct block_request *req, gpid_t src){
    // req->ino
    // req->offset_nblock
    if (req->ino >= MAX_INODES) {
        printf("block_do_getsize: bad inode %u\n", req->ino);
        block_respond(req, BLOCK_ERROR, 0, 0, src);
        return;
    }

    /* Get size of block store.
     */
    block_store_t *virt = bss->inodes[req->ino];
    unsigned int size = (*virt->nblocks)(virt);

    /* Send size of block store.
     */
    struct block_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = BLOCK_OK;
    rep.size_nblock = size;
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}


/* Respond to a setsize block request.
 */
static void block_do_setsize(struct block_server_state *bss, struct block_request *req, gpid_t src){
    // req->ino
    // req->offset_nblock
    if (req->ino >= MAX_INODES) {
        printf("block_do_setsize: bad inode %u\n", req->ino);
        block_respond(req, BLOCK_ERROR, 0, 0, src);
        return;
    }

    block_store_t *virt = bss->inodes[req->ino];
    int result = (*virt->setsize)(virt, req->offset_nblock);

    if (result < 0) {
        printf("block_do_setsize: bad size ino: %u, size: %u\n", req->ino, req->offset_nblock);
        block_respond(req, BLOCK_ERROR, 0, 0, src);
        return;
    }

    block_respond(req, BLOCK_OK, 0, 0, src);
}
