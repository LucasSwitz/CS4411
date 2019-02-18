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
#include "../earth/devdisk.h"
#include "../shared/syscall.h"
#include "../shared/dir.h"
#include "../shared/block.h"
#include "../shared/queue.h"
#include "exec.h"
#include "process.h"
#include "blocksvr.h"

/* State of the block server.
 */
struct disk_server_state {
	struct dev_disk *dd;
	unsigned int nblocks;
};

struct disk_request {
	gpid_t pid, src;
	struct block_reply *rep;
};

static void disk_respond(struct block_request *req, enum block_status status,
                void *data, unsigned int nblock, gpid_t src){
    struct block_reply *rep = new_alloc_ext(struct block_reply, nblock * BLOCK_SIZE);
    rep->status = status;
    memcpy(&rep[1], data, nblock * BLOCK_SIZE);
    sys_send(src, MSG_REPLY, rep, sizeof(*rep) + nblock * BLOCK_SIZE);
	free(rep);
}

/* This is an interrupt handler, invoked when the read has completed.
 */
static void disk_read_complete(void *arg, bool_t success){
	struct disk_request *dr = arg;

	dr->rep->size_nblock = 1;
	if (success) {
		dr->rep->status = BLOCK_OK;
		proc_send(dr->pid, dr->src, MSG_REPLY, dr->rep, sizeof(*dr->rep) + BLOCK_SIZE);
	}
	else {
		dr->rep->status = BLOCK_ERROR;
		proc_send(dr->pid, dr->src, MSG_REPLY, dr->rep, sizeof(*dr->rep));
	}
	free(dr->rep);
	free(dr);
}

/* Respond to a read block request.
 */
static void disk_do_read(struct disk_server_state *dss, struct block_request *req, gpid_t src){
    if (req->ino != 0) {
        printf("disk_do_read: bad inode: %u\n", req->ino);
        disk_respond(req, BLOCK_ERROR, 0, 0, src);
        return;
    }

    /* Allocate room for the reply.
     */
    struct block_reply *rep = new_alloc_ext(struct block_reply, BLOCK_SIZE);

	/* Schedule the disk read operation.
	 */
	struct disk_request *dr = new_alloc(struct disk_request);
	dr->pid = sys_getpid();
	dr->src = src;
	dr->rep = rep;
	dev_disk_read(dss->dd, req->offset_nblock, (char *) &rep[1], disk_read_complete, dr);
}

/* This is an interrupt handler, invoked when the write has completed.
 */
static void disk_write_complete(void *arg, bool_t success){
	struct disk_request *dr = arg;

	dr->rep->status = success ? BLOCK_OK : BLOCK_ERROR;
	dr->rep->size_nblock = 1;
	proc_send(dr->pid, dr->src, MSG_REPLY, dr->rep, sizeof(*dr->rep));
	free(dr->rep);
	free(dr);
}

/* Respond to a write block request.
 */
static void disk_do_write(struct disk_server_state *dss, struct block_request *req,
														unsigned int size, gpid_t src){
	assert(size == BLOCK_SIZE);
    if (req->ino != 0) {
        printf("disk_do_write: bad inode: %u\n", req->ino);
        disk_respond(req, BLOCK_ERROR, 0, 0, src);
        return;
    }

    /* Allocate room for the reply.
     */
    struct block_reply *rep = new_alloc(struct block_reply);

	/* Schedule the disk read operation.
	 */
	struct disk_request *dr = new_alloc(struct disk_request);
	dr->pid = sys_getpid();
	dr->src = src;
	dr->rep = rep;
	dev_disk_write(dss->dd, req->offset_nblock, (char *) &req[1], disk_write_complete, dr);
}

/* Respond to a getsize block request.
 */
static void disk_do_getsize(struct disk_server_state *dss, struct block_request *req, gpid_t src){
    if (req->ino != 0) {
        printf("disk_do_getsize: bad inode %u\n", req->ino);
        disk_respond(req, BLOCK_ERROR, 0, 0, src);
        return;
    }

    /* Send size of block store.
     */
    struct block_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = BLOCK_OK;
    rep.size_nblock = dss->nblocks;
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a setsize block request.
 */
static void disk_do_setsize(struct disk_server_state *dss, struct block_request *req, gpid_t src){
	fprintf(stderr, "disk_do_setsize: not supported\n");
	disk_respond(req, BLOCK_ERROR, 0, 0, src);
}

static void disk_proc(void *arg){
	struct disk_server_state *dss = arg;

    printf("DISK SERVER: %u\n\r", sys_getpid());

    struct block_request *req = new_alloc_ext(struct block_request, PAGESIZE);
    for (;;) {
        gpid_t src;
        int req_size = sys_recv(MSG_REQUEST, 0, req, sizeof(req) + PAGESIZE, &src);
		if (req_size < 0) {
			printf("disk server shutting down\n\r");
			// free(dss);			-- events may still come in
			free(req);
			break;
		}

        assert(req_size >= (int) sizeof(*req));

        switch (req->type) {
            case BLOCK_READ:
                disk_do_read(dss, req, src);
                break;
            case BLOCK_WRITE:
                disk_do_write(dss, req, req_size - sizeof(*req), src);
                break;
            case BLOCK_GETSIZE:
                disk_do_getsize(dss, req, src);
                break;
            case BLOCK_SETSIZE:
                disk_do_setsize(dss, req, src);
                break;
			default:
				assert(0);
		}
    }
}

/* Create a disk device.
 */
gpid_t disk_init(char *filename, unsigned int nblocks, bool_t sync){
	struct disk_server_state *dss = new_alloc(struct disk_server_state);
	dss->nblocks = nblocks;
	dss->dd = dev_disk_create(filename, nblocks, sync);
	return proc_create(1, "disk", disk_proc, dss);
}
