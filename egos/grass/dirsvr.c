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
#include "../earth/clock.h"
#include "../earth/mem.h"
#include "../shared/syscall.h"
#include "../shared/file.h"
#include "../shared/dir.h"
#include "../shared/queue.h"
#include "exec.h"
#include "process.h"

#define MAX_PATH_NAME	1024
#define NENTRIES		(PAGESIZE / DIR_ENTRY_SIZE)

/* See if the name in de->name is the same as in s for the given size.
 */
static bool_t name_cmp(struct dir_entry *de, char *s, unsigned int size){
	if (strnlen(de->name, DIR_NAME_SIZE) != size) {
		return False;
	}
	return strncmp(de->name, s, size) == 0;
}

static void dir_respond(gpid_t src, enum dir_status status, fid_t *fid){
	struct dir_reply rep;
	memset(&rep, 0, sizeof(rep));
	rep.status = status;
	if (fid != 0) {
		rep.fid = *fid;
	}
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Respond to a lookup request.
 */
static void dir_do_lookup(struct dir_request *req, gpid_t src, char *name, unsigned int size){
	if (size > DIR_NAME_SIZE) {
		dir_respond(src, DIR_ERROR, 0);
		return;
	}

	/* Read the directory one page at a time.
	 */
	char *buf = malloc(PAGESIZE);
	unsigned long offset;
	for (offset = 0;;) {
		unsigned int n = PAGESIZE;
		bool_t status = file_read(req->dir.server, req->dir.ino,
					offset, buf, &n);
		if (!status) {
			dir_respond(src, DIR_ERROR, 0);			// file error
			free(buf);
			return;
		}
		if (n == 0) {
			dir_respond(src, DIR_ERROR, 0);			// not found
			free(buf);
			return;
		}
		assert(n % DIR_ENTRY_SIZE == 0);
		unsigned int i;
		for (i = 0; i < n; i += DIR_ENTRY_SIZE, offset += DIR_ENTRY_SIZE) {
			struct dir_entry *de = (struct dir_entry *) &buf[i];
			if (name_cmp(de, name, size)) {
				dir_respond(src, DIR_OK, &de->fid);
				free(buf);
				return;
			}
		}
	}
}

/* Respond to an insert request.
 */
static void dir_do_insert(struct dir_request *req, gpid_t src, char *name, unsigned int size){
	if (size > DIR_NAME_SIZE) {
		dir_respond(src, DIR_ERROR, 0);
		return;
	}

	/* Read the directory one page at a time.
	 */
	char *buf = malloc(PAGESIZE);
	bool_t found_free_entry = False;
	unsigned long offset, free_offset;
	for (offset = 0;;) {
		unsigned int n = PAGESIZE;
		bool_t status = file_read(req->dir.server, req->dir.ino,
					offset, buf, &n);
		if (!status) {
			free(buf);
			dir_respond(src, DIR_ERROR, 0);			// file error
			return;
		}
		if (n == 0) {
			break;
		}
		assert(n % DIR_ENTRY_SIZE == 0);
		unsigned int i;
		for (i = 0; i < n; i += DIR_ENTRY_SIZE, offset += DIR_ENTRY_SIZE) {
			struct dir_entry *de = (struct dir_entry *) &buf[i];
			if (de->name[0] == 0) {
				if (!found_free_entry) {
					free_offset = offset;
					found_free_entry = True;
				}
			}
			else if (name_cmp(de, name, size)) {
				free(buf);
				dir_respond(src, DIR_ERROR, 0);			// already exists
				return;
			}
		}
	}

	/* Add the new entry.
	 */
	if (found_free_entry) {
		offset = free_offset;
	}
	struct dir_entry nde;
	memset(&nde, 0, sizeof(nde));
	strncpy(nde.name, name, size);
	nde.fid = req->fid;
	bool_t wstatus = file_write(req->dir.server, req->dir.ino,
											offset, &nde, sizeof(nde));
	free(buf);
	dir_respond(src, wstatus ? DIR_OK : DIR_ERROR, &req->fid);
}

/* The directory server.
 */
static void dir_proc(void *arg){
	printf("DIRECTORY SERVER: %u\n\r", sys_getpid());

	struct dir_request *req = new_alloc_ext(struct dir_request, MAX_PATH_NAME);
	for (;;) {
		gpid_t src;

		int req_size = sys_recv(MSG_REQUEST, 0,
								req, sizeof(req) + MAX_PATH_NAME, &src);
		if (req_size < 0) {
			printf("directory server terminating\n\r");
			free(req);
			break;
		}

		assert(req_size >= (int) sizeof(*req));
		switch (req->type) {
		case DIR_LOOKUP:
			dir_do_lookup(req, src, (char *) &req[1], req_size - sizeof(*req));
			break;
		case DIR_INSERT:
			dir_do_insert(req, src, (char *) &req[1], req_size - sizeof(*req));
			break;
		case DIR_REMOVE:
			// dir_do_remove(req, src, (char *) &req[1], req_size - sizeof(*req));
			break;
		default:
			assert(0);
		}
	}
}

gpid_t dir_init(void){
	assert(sizeof(struct dir_entry) == DIR_ENTRY_SIZE);
	return proc_create(1, "dir", dir_proc, 0);
}
