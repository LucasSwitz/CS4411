#include <inttypes.h>
#include <stdio.h>
#include <ucontext.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../earth/earth.h"
#include "../earth/myalloc.h"
#include "../earth/tlb.h"
#include "../earth/intr.h"
#include "../earth/mem.h"
#include "../shared/syscall.h"
#include "../shared/queue.h"
#include "../shared/file.h"
#include "../shared/dir.h"
#include "../shared/block.h"
#include "exec.h"
#include "process.h"

// for MAX_INODES
#include "blocksvr.h"

struct file_server_state {
	gpid_t block_svr;
	struct file_stat stat_cache[MAX_INODES];
};

// file_stat is 16 bytes on Linux64
#define STATS_PER_BLOCK      (BLOCK_SIZE / sizeof(struct file_stat))

// these helper functions are declared here and defined later
static void flush_stat_cache(struct file_server_state *);
static bool_t load_stat_cache(struct file_server_state *);
static void blkfile_do_create(struct file_server_state *, struct file_request *req, gpid_t src);
static void blkfile_do_delete(struct file_server_state *, struct file_request *req, gpid_t src);
static void blkfile_do_chown(struct file_server_state *, struct file_request *req, gpid_t src);
static void blkfile_do_chmod(struct file_server_state *fss, struct file_request *req, gpid_t src);
static void blkfile_do_read(struct file_server_state *, struct file_request *req, gpid_t src);
static void blkfile_do_write(struct file_server_state *, struct file_request *req, void *data, unsigned int size, gpid_t src);
static void blkfile_do_stat(struct file_server_state *, struct file_request *req, gpid_t src);
static void blkfile_do_setsize(struct file_server_state *, struct file_request *req, gpid_t src);
static void blkfile_respond(struct file_request *req, enum file_status status,
                void *data, unsigned int size, gpid_t src);
static bool_t blkfile_read_allowed(struct file_stat *stat, gpid_t src);
static bool_t blkfile_write_allowed(struct file_stat *stat, gpid_t src);

bool_t multiblock_read(gpid_t svr, unsigned int ino, unsigned int offset, void *addr, unsigned int *p_nblocks){
	unsigned int nblocks = *p_nblocks;
	unsigned int i;

	for (i = 0; i < nblocks; i++) {
		bool_t r = block_read(svr, ino, offset, addr);
		if (!r) {
			if (i == 0) {
				return False;
			}
			else {
				break;
			}
		}
		offset++;
		addr = (char *) addr + BLOCK_SIZE;
	}
	*p_nblocks = i;
	return True;
}

bool_t multiblock_write(gpid_t svr, unsigned int ino, unsigned int offset, const void *addr, unsigned int nblocks){
	unsigned int i;

	for (i = 0; i < nblocks; i++) {
		bool_t r = block_write(svr, ino, offset, addr);
		if (!r) {
			return False;
		}
		offset++;
		addr = (char *) addr + BLOCK_SIZE;
	}
	return True;
}

/* A file server based on block server. Each file corresponds to an inode in block server
 */
static void blkfile_proc(void *arg){
    printf("DISK FILE SERVER: %u\n\r", sys_getpid());

	struct file_server_state *fss = arg;

    // initialize blkfile internal states
    if (!load_stat_cache(fss)) {
        memset(fss->stat_cache, 0, sizeof(fss->stat_cache));
        fss->stat_cache[0].st_alloc = True;
        fss->stat_cache[0].st_size = MAX_INODES / STATS_PER_BLOCK;
        flush_stat_cache(fss);
    }

    struct file_request *req = new_alloc_ext(struct file_request, PAGESIZE);
    for (;;) {
        gpid_t src;
        int req_size = sys_recv(MSG_REQUEST, 0, req, sizeof(*req) + PAGESIZE, &src);
		if (req_size < 0) {
			printf("block file server terminated\n\r");
			free(req);
			free(fss);
			break;
		}

        assert(req_size >= (int) sizeof(*req));
        switch (req->type) {
        case FILE_CREATE:
            //fprintf(stderr, "!!DEBUG: calling blkfile create\n");
            blkfile_do_create(fss, req, src);
            break;
        case FILE_DELETE:
            blkfile_do_delete(fss, req, src);
            break;
        case FILE_CHOWN:
            //fprintf(stderr, "!!DEBUG: calling blkfile chown\n");
            blkfile_do_chown(fss, req, src);
            break;
        case FILE_CHMOD:
            //fprintf(stderr, "!!DEBUG: calling blkfile chmod\n");
            blkfile_do_chmod(fss, req, src);
            break;
        case FILE_READ:
            //fprintf(stderr, "!!DEBUG: calling blkfile read\n");
            blkfile_do_read(fss, req, src);
            break;
        case FILE_WRITE:
            //fprintf(stderr, "!!DEBUG: calling blkfile write\n");
            blkfile_do_write(fss, req, &req[1], req_size - sizeof(*req), src);
            break;
        case FILE_STAT:
            //fprintf(stderr, "!!DEBUG: calling blkfile stat\n");
            blkfile_do_stat(fss, req, src);
            break;
        case FILE_SETSIZE:
            //fprintf(stderr, "!!DEBUG: calling blkfile create\n");
            blkfile_do_setsize(fss, req, src);
            break;
        default:
            assert(0);
        }
    }
}

gpid_t blkfile_init(gpid_t block_server){
	struct file_server_state *fss = new_alloc(struct file_server_state);
    fss->block_svr = block_server;
    return proc_create(1, "blkfile", blkfile_proc, fss);
}

/* Respond to a create request.
 */
static void blkfile_do_create(struct file_server_state *fss, struct file_request *req, gpid_t src) {
    unsigned int ino;
    for(ino = 1; ino < MAX_INODES; ino++) {
        if (!fss->stat_cache[ino].st_alloc) {
            break;
        }
    }

    if (ino == MAX_INODES) {
        printf("blkfile_do_create: out of files\n");
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }
    fss->stat_cache[ino].st_alloc = True;
    fss->stat_cache[ino].st_mode = req->mode;
    struct process *p = proc_find(src);
    fss->stat_cache[ino].st_uid = p->uid;
    flush_stat_cache(fss);

    /* Send response.
     */
    struct file_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = FILE_OK;
    rep.ino = ino;
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a delete request.
 */
static void blkfile_do_delete(struct file_server_state *fss, struct file_request *req, gpid_t src) {
    if (req->ino > MAX_INODES) {
        printf("blkfile_do_delete: invalid req->ino\n");
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    // check permission
    if (!blkfile_write_allowed(&fss->stat_cache[req->ino], src)) {
        printf("blkfile_do_delete: permission denied: %u\n", req->ino);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    struct file_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = FILE_OK;

    if (!fss->stat_cache[req->ino].st_alloc) {
        // already deleted
        sys_send(src, MSG_REPLY, &rep, sizeof(rep));
    } else {
        // delete the file
        fss->stat_cache[req->ino].st_alloc = False;
        flush_stat_cache(fss);

        if (!block_setsize(fss->block_svr, req->ino, 0)) {
            printf("blkfile_do_delete: bad size %u\n", req->ino);
            blkfile_respond(req, FILE_ERROR, 0, 0, src);
        }
        sys_send(src, MSG_REPLY, &rep, sizeof(rep));
    }

}

/* Respond to a chown request.
 */
static void blkfile_do_chown(struct file_server_state *fss, struct file_request *req, gpid_t src) {
    unsigned int ino = req->ino;
    if (ino >= MAX_INODES || !fss->stat_cache[ino].st_alloc) {
        printf("blkfile_do_chown: bad inode: %u\n", req->ino); 
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }
    struct process *p = proc_find(src);
    if (p->uid != 0) {
        printf("blkfile_do_chown: permission denied for uid %u\n", p->uid); 
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    fss->stat_cache[ino].st_uid = req->uid;
    flush_stat_cache(fss);

    /* Send response.
     */
    struct file_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = FILE_OK;
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a chmod request.
 */
static void blkfile_do_chmod(struct file_server_state *fss, struct file_request *req, gpid_t src) {
    unsigned int ino = req->ino;
    if (ino >= MAX_INODES || !fss->stat_cache[ino].st_alloc) {
        printf("blkfile_do_chmod: bad inode: %u\n", req->ino); 
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }
    struct process *p = proc_find(src);
    if (p->uid != 0 && p->uid != fss->stat_cache[ino].st_uid) {
        printf("blkfile_do_chmod: permission denied for uid %u\n", p->uid); 
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    fss->stat_cache[ino].st_mode = req->mode;
    flush_stat_cache(fss);

    /* Send response.
     */
    struct file_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = FILE_OK;
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}


/* Respond to a read request.
 */
static void blkfile_do_read(struct file_server_state *fss, struct file_request *req, gpid_t src){

    if (req->ino >= MAX_INODES || !fss->stat_cache[req->ino].st_alloc) {
        if (req->size == 0 && req->ino == 1) {
            // initializing dir server for new disk
            // print nothing
        } else {
            printf("blkfile_do_read: bad inode: %u\n", req->ino);
        } 
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }
    // check permission
    if (!blkfile_read_allowed(&fss->stat_cache[req->ino], src)) {
        printf("blkfile_do_read: permission denied: %u, %u\n", req->ino, fss->stat_cache[req->ino].st_mode);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    /* Allocate room for the reply.
     */
    struct file_reply *rep = new_alloc_ext(struct file_reply, req->size);
    char *contents = NULL;

    if (req->size == 0) {
        // the request is just checking whether the file exists
        rep->status = FILE_OK;
		rep->op = FILE_READ;
        rep->stat.st_size = 0;
        sys_send(src, MSG_REPLY, rep, sizeof(*rep));

        free(rep);
        return;
    }

    unsigned int n;
    if (req->offset < fss->stat_cache[req->ino].st_size) {
        n = fss->stat_cache[req->ino].st_size - req->offset;
        if (n > req->size) {
            n = req->size;
        }

        // Read file parameters
		// TODO  is this reading one more block than necessary?
        unsigned int start_block_no = req->offset / BLOCK_SIZE;
        unsigned int end_block_no = (req->offset + n) / BLOCK_SIZE;
        unsigned int psize_nblock = end_block_no - start_block_no + 1;

        // Call block server
        contents = malloc(psize_nblock * BLOCK_SIZE);
        if (!multiblock_read(fss->block_svr, req->ino, start_block_no, contents, &psize_nblock) 
          || psize_nblock != end_block_no - start_block_no + 1) {
            free(contents);
            free(rep);
            printf("blkfile_do_read: block server read error: %d %d\n", psize_nblock, end_block_no - start_block_no + 1);
            blkfile_respond(req, FILE_ERROR, 0, 0, src);
            return;
        }

        memcpy(&rep[1], &contents[req->offset % BLOCK_SIZE], n);
    }
    else {
        n = 0;
    }

    rep->status = FILE_OK;
	rep->op = FILE_READ;
    rep->stat.st_size = n;
    sys_send(src, MSG_REPLY, rep, sizeof(*rep) + n);

    free(rep);
    if (contents != NULL)
        free(contents);
}

static int blkfile_put(struct file_server_state *fss, unsigned int ino, unsigned long offset, void* data, unsigned int size) {

    // Get file size and nblock
    unsigned long file_size = fss->stat_cache[ino].st_size;

    // File read parameter
    unsigned long start_block_no = offset / BLOCK_SIZE;
    unsigned long final_size;
    unsigned long end_block_no;
    unsigned int psize_nblock; // how many to read from file

    if (offset + size > file_size) {
        final_size = offset + size;
        end_block_no = final_size / BLOCK_SIZE;
        psize_nblock = (file_size / BLOCK_SIZE) - start_block_no + 1;
    } else {
        final_size = file_size;
        end_block_no = (offset + size) / BLOCK_SIZE;
        psize_nblock = end_block_no - start_block_no + 1;
    }

    // Get file content
    char *contents = malloc((end_block_no - start_block_no + 1) * BLOCK_SIZE);
    memset(contents, 0, (end_block_no - start_block_no + 1) * BLOCK_SIZE);

    if (start_block_no > file_size / BLOCK_SIZE) {
        // Write directly to block server
        memcpy(&contents[offset % BLOCK_SIZE], data, size);
        if (!multiblock_write(fss->block_svr, ino, start_block_no, contents, end_block_no - start_block_no + 1)) {
            free(contents);
            return -1;
        }
    } else {
        // Read file first
        if (file_size != 0 && 
            (!multiblock_read(fss->block_svr, ino, start_block_no, contents, &psize_nblock))) {
            free(contents);
            return -1;
        }

        // Modify file content
        memcpy(&contents[offset % BLOCK_SIZE], data, size);

        // Write back to block store
        if (!multiblock_write(fss->block_svr, ino, start_block_no, contents, end_block_no - start_block_no + 1)) {
            free(contents);
            return -1;
        }
    }

    free(contents);
    fss->stat_cache[ino].st_size = final_size;
    flush_stat_cache(fss);
    return 0;
}

/* Respond to a write request.
 */
static void blkfile_do_write(struct file_server_state *fss, struct file_request *req, void *data, unsigned int size, gpid_t src){
    // req->ino
    // req->offset
    // req->size
    if (req->ino >= MAX_INODES || !fss->stat_cache[req->ino].st_alloc) {
        printf("blkfile_do_write: bad inode %u\n", req->ino);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    if (req->size != size) {
        printf("blkfile_do_write: size mismatch %u %u\n", req->size, size);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    // check permission
    if (!blkfile_write_allowed(&fss->stat_cache[req->ino], src)) {
        printf("blkfile_do_write: permission denied: %u\n", req->ino);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    if (blkfile_put(fss, req->ino, req->offset, data, size) == 0) {
        blkfile_respond(req, FILE_OK, 0, 0, src);
    } else {
        printf("blkfile_do_write: write error, offset: %lu, size: %u\n", req->offset, req->size);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
    }
}

/* Respond to a stat request.
 */
static void blkfile_do_stat(struct file_server_state *fss, struct file_request *req, gpid_t src){
    if (req->ino >= MAX_INODES || !fss->stat_cache[req->ino].st_alloc) {
        printf("blkfile_do_stat: bad inode %u\n", req->ino);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }
    // check permission
    if (!blkfile_read_allowed(&fss->stat_cache[req->ino], src)) {
        // printf("blkfile_do_stat: permission denied: %u\n", req->ino);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    /* Send size of file.
     */
    struct file_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = FILE_OK;
	rep.op = FILE_STAT;
    rep.stat = fss->stat_cache[req->ino];
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a setsize request.
 */
static void blkfile_do_setsize(struct file_server_state *fss, struct file_request *req, gpid_t src){
    if (req->ino >= MAX_INODES || !fss->stat_cache[req->ino].st_alloc) {
        printf("blkfile_do_setsize: bad inode %u\n", req->ino);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    if (req->offset > fss->stat_cache[req->ino].st_size) {
        printf("blkfile_do_setsize: bad size %u\n", req->ino);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    // check permission
    if (!blkfile_write_allowed(&fss->stat_cache[req->ino], src)) {
        printf("blkfile_do_setsize: permission denied: %u\n", req->ino);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
        return;
    }

    // call block server set size
    unsigned int final_nblock = (req->offset == 0? 0 : 1 + (req->offset / BLOCK_SIZE));
    if (!block_setsize(fss->block_svr, req->ino, final_nblock)) {
        printf("blkfile_do_setsize: bad size %u\n", req->ino);
        blkfile_respond(req, FILE_ERROR, 0, 0, src);
    }

    fss->stat_cache[req->ino].st_size = req->offset;
    flush_stat_cache(fss);
    blkfile_respond(req, FILE_OK, 0, 0, src);
}

static void blkfile_respond(struct file_request *req, enum file_status status,
                void *data, unsigned int size, gpid_t src){
    struct file_reply *rep = new_alloc_ext(struct file_reply, size);
    rep->status = status;
    memcpy(&rep[1], data, size);
    sys_send(src, MSG_REPLY, rep, sizeof(*rep) + size);
    free(rep);
}

static void flush_stat_cache(struct file_server_state *fss) {
    unsigned int size = sizeof(fss->stat_cache);
    unsigned int end_block_no = (size - 1) / BLOCK_SIZE;

    if (!multiblock_write(fss->block_svr, 0, 0, fss->stat_cache, end_block_no + 1)) {
        printf("flush_stat_cache: error\n");
    }
}

static bool_t load_stat_cache(struct file_server_state *fss) {
    unsigned int size = sizeof(fss->stat_cache);
    unsigned int psize_nblock = 0;

    block_getsize(fss->block_svr, 0, &psize_nblock);
    if (psize_nblock == 0) {
        printf("Init new disk: installing stat array at inode0\n\r");
        return False;
    } else if (psize_nblock != (size - 1) / BLOCK_SIZE + 1) {
        printf("load_stat_cache: error, reinstalling stat array inode0\n");
        return False;
    }

    psize_nblock = (size - 1) / BLOCK_SIZE + 1;
    multiblock_read(fss->block_svr, 0, 0, &fss->stat_cache, &psize_nblock);
    return True;
}

static bool_t blkfile_read_allowed(struct file_stat *stat, gpid_t src) {
    struct process *p = proc_find(src);

    //printf("%u is trying to read file owned by %u, other: %u\n", p->uid, stat->st_uid, stat->st_mode & P_FILE_OTHER_READ);

    if (p->uid == 0) {
        // read from root user
        return True;
    } else if (p->uid == stat->st_uid) {
        // read from owner
        return stat->st_mode & P_FILE_OWNER_READ;
    } else {
        // read from other
        return stat->st_mode & P_FILE_OTHER_READ;
    }
}

static bool_t blkfile_write_allowed(struct file_stat *stat, gpid_t src) {
    struct process *p = proc_find(src);

    if (p->uid == 0) {
        // write from root user
        return True;
    } else if (p->uid == stat->st_uid) {
        // write from owner
        return stat->st_mode & P_FILE_OWNER_WRITE;
    } else {
        // write from other
        return stat->st_mode & P_FILE_OTHER_WRITE;
    }
}

#define BUF_SIZE    1024

/* Load a "grass" file with contents of a local file.
 */
void blkfile_load(fid_t fid, char *file){
    FILE *fp = fopen(file, "r");
    assert(fp != 0);

    char buf[BUF_SIZE];

    unsigned long offset = 0;
    while (!feof(fp)) {
        assert(!ferror(fp));
        size_t n = fread(buf, 1, BUF_SIZE, fp);
        bool_t status = file_write(fid.server, fid.ino, offset, buf, n);
        assert(status);
        offset += n;
    }
}
