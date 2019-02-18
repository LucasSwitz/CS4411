/* This implements the ram file server.
 *
 * TODO: implement access control checks.
 */

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
#include "exec.h"
#include "process.h"

#define MAX_FILES	100
#define BUF_SIZE	1024

/* Contents of a file.
 */
struct file {
	struct file_stat stat;
	char *contents;			// contents of file
};

static void ramfile_respond(gpid_t src, enum file_status status,
				void *data, unsigned int size){
	struct file_reply *rep = new_alloc_ext(struct file_reply, size);
	rep->status = status;
	memcpy(&rep[1], data, size);
	sys_send(src, MSG_REPLY, rep, sizeof(*rep) + size);
	free(rep);
}

static bool_t ramfile_read_allowed(struct file *file, gpid_t src) {
	struct process *p = proc_find(src);

	if (p->uid == 0) {
		// read from root user
		return True;
	} else if (p->uid == file->stat.st_uid) {
		// read from owner
		return file->stat.st_mode & P_FILE_OWNER_READ;
	} else {
		// read from other
		return file->stat.st_mode & P_FILE_OTHER_READ;
	}
}

static bool_t ramfile_write_allowed(struct file *file, gpid_t src) {
	struct process *p = proc_find(src);

	if (p->uid == 0) {
		// write from root user
		return True;
	} else if (p->uid == file->stat.st_uid) {
		// write from owner
		return file->stat.st_mode & P_FILE_OWNER_WRITE;
	} else {
		// write from other
		return file->stat.st_mode & P_FILE_OTHER_WRITE;
	}
}


/* Respond to a create request.
 */
static void ramfile_do_create(struct file *files, struct file_request *req, gpid_t src){
	/* See if there's a free file.
	 */
	unsigned int ino;

	for (ino = 1; ino < MAX_FILES; ino++) {
		if (!files[ino].stat.st_alloc) {
			break;
		}
	}
	if (ino == MAX_FILES) {
		printf("ramfile_do_create: out of files\n");
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}
	files[ino].stat.st_alloc = True;
	files[ino].stat.st_mode = req->mode;
	struct process *p = proc_find(src);
	files[ino].stat.st_uid = p->uid;

	/* Send response.
	 */
	struct file_reply rep;
	memset(&rep, 0, sizeof(rep));
	rep.status = FILE_OK;
	rep.ino = ino;
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a read request.
 */
static void ramfile_do_read(struct file *files, struct file_request *req, gpid_t src){
	if (!files[req->ino].stat.st_alloc || req->ino >= MAX_FILES) {
		printf("ramfile_do_read: bad inode: %u\n\r", req->ino);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}

	struct file *file = &files[req->ino];
	unsigned int n;

	// check permission
	if (!ramfile_read_allowed(file, src)) {
		printf("ramfile_do_read: permission denied: %u\n", req->ino);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}

	/* Allocate room for the reply.
	 */
	struct file_reply *rep = new_alloc_ext(struct file_reply, req->size);

	// try reading
	if (req->offset < file->stat.st_size) {
		n = file->stat.st_size - req->offset;
		if (n > req->size) {
			n = req->size;
		}
		memcpy(&rep[1], &file->contents[req->offset], n);
	}
	else {
		n = 0;
	}

	rep->status = FILE_OK;
	rep->op = FILE_READ;
	rep->stat.st_size = n;
	sys_send(src, MSG_REPLY, rep, sizeof(*rep) + n);
	free(rep);
}

/* Code for adding content to a file.
 */
static void ramfile_put(struct file *file, unsigned long offset,
									char *data, unsigned int size){
	if (offset + size > file->stat.st_size) {
		file->contents = realloc(file->contents, offset + size);
		file->stat.st_size = offset + size;
	}
	memcpy(&file->contents[offset], data, size);
}

/* Respond to a write request.
 */
static void ramfile_do_write(struct file *files,
			struct file_request *req, gpid_t src, void *data, unsigned int size){
	if (req->ino >= MAX_FILES || !files[req->ino].stat.st_alloc) {
		printf("ramfile_do_write: bad inode %u\n", req->ino);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}
	if (req->size != size) {
		printf("ramfile_do_write: size mismatch %u %u\n", req->size, size);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}
	// check permission
	if (!ramfile_write_allowed(&files[req->ino], src)) {
		printf("ramfile_do_write: permission denied: %u\n", req->ino);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}

	ramfile_put(&files[req->ino], req->offset, data, size);
	ramfile_respond(src, FILE_OK, 0, 0);
}

/* Respond to a stat request.
 */
static void ramfile_do_stat(struct file *files, struct file_request *req, gpid_t src){
	if (!files[req->ino].stat.st_alloc || req->ino >= MAX_FILES) {
		printf("ramfile_do_stat: bad inode %u\n", req->ino);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}

	// check permission
	if (!ramfile_read_allowed(&files[req->ino], src)) {
		// printf("ramfile_do_stat: permission denied: %u\n", req->ino);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}

	/* Send size of file.
	 */
	struct file_reply rep;
	memset(&rep, 0, sizeof(rep));
	rep.status = FILE_OK;
	rep.op = FILE_STAT;
	rep.stat = files[req->ino].stat;
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a setsize request.
 */
static void ramfile_do_setsize(struct file *files, struct file_request *req, gpid_t src){
	if (req->ino >= MAX_FILES || !files[req->ino].stat.st_alloc) {
		printf("ramfile_do_setsize: bad inode %u\n", req->ino);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}
	if (req->offset > files[req->ino].stat.st_size) {
		printf("ramfile_do_setsize: bad size %u\n", req->ino);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}
	// check permission
	if (!ramfile_write_allowed(&files[req->ino], src)) {
		printf("ramfile_do_setsize: permission denied: %u\n", req->ino);
		ramfile_respond(src, FILE_ERROR, 0, 0);
		return;
	}

	struct file *f = &files[req->ino];
	f->stat.st_size = req->offset;
	f->contents = realloc(f->contents, f->stat.st_size);
	ramfile_respond(src, FILE_OK, 0, 0);
}


/* Respond to a chown request.
 */
static void ramfile_do_chown(struct file *files, struct file_request *req, gpid_t src) {
    if (req->ino >= MAX_FILES || !files[req->ino].stat.st_alloc) {
        printf("ramfile_do_chown: bad inode: %u\n", req->ino); 
        ramfile_respond(src, FILE_ERROR, 0, 0);
        return;
    }
    struct process *p = proc_find(src);
    if (p->uid != 0) {
        printf("ramfile_do_chown: permission denied for uid %u\n", p->uid); 
        ramfile_respond(src, FILE_ERROR, 0, 0);
        return;
    }

    files[req->ino].stat.st_uid = req->uid;

    /* Send response.
     */
    struct file_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = FILE_OK;
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a chmod request.
 */
static void ramfile_do_chmod(struct file *files, struct file_request *req, gpid_t src) {
    if (req->ino >= MAX_FILES || !files[req->ino].stat.st_alloc) {
        printf("ramfile_do_chown: bad inode: %u\n", req->ino); 
        ramfile_respond(src, FILE_ERROR, 0, 0);
        return;
    }
    struct process *p = proc_find(src);
    if (p->uid != 0 && p->uid != files[req->ino].stat.st_uid) {
        printf("ramfile_do_chmod: permission denied for uid %u\n", p->uid); 
        ramfile_respond(src, FILE_ERROR, 0, 0);
        return;
    }

    files[req->ino].stat.st_mode = req->mode;

    /* Send response.
     */
    struct file_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.status = FILE_OK;
    sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

static void ramfile_cleanup(void *arg){
	struct file *files = arg;
	unsigned int i;

	printf("ram file server: cleaning up\n\r");

	for (i = 0; i < MAX_FILES; i++) {
		struct file *f = &files[i];

		if (f->stat.st_alloc) {
			free(f->contents);
		}
	}
	free(files);
}

/* A simple in-memory file server.  All files are simply contiguous
 * memory regions.
 */
static void ramfile_proc(void *arg){
	printf("RAM FILE SERVER: %u\n\r", sys_getpid());

	proc_current->finish = ramfile_cleanup;

	struct file *files = arg;

	struct file_request *req = new_alloc_ext(struct file_request, PAGESIZE);
	for (;;) {
		gpid_t src;
		int req_size = sys_recv(MSG_REQUEST, 0, req, sizeof(*req) + PAGESIZE, &src);
		if (req_size < 0) {
			printf("ram file server shutting down\n\r");
			free(files);
			free(req);
			break;
		}

		assert(req_size >= (int) sizeof(*req));
		switch (req->type) {
		case FILE_CREATE:
			ramfile_do_create(files, req, src);
			break;
		case FILE_CHOWN:
            ramfile_do_chown(files, req, src);
            break;
        case FILE_CHMOD:
            ramfile_do_chmod(files, req, src);
            break;
		case FILE_READ:
			ramfile_do_read(files, req, src);
			break;
		case FILE_WRITE:
			ramfile_do_write(files, req, src, &req[1], req_size - sizeof(*req));
			break;
		case FILE_STAT:
			ramfile_do_stat(files, req, src);
			break;
		case FILE_SETSIZE:
			ramfile_do_setsize(files, req, src);
			break;
		default:
			assert(0);
		}
	}
}

gpid_t ramfile_init(void){
	struct file *files = calloc(MAX_FILES, sizeof(struct file));
	files[0].stat.st_alloc = True;

	return proc_create(1, "ramfile", ramfile_proc, files);
}

/* Load a "grass" file with contents of a local file.
 */
void ramfile_load(fid_t fid, char *file){
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
