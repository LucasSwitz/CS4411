#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include "egos.h"
#include "file.h"

bool_t file_exist(gpid_t svr, unsigned int ino) {
	unsigned int psize = 0;
	return file_read(svr, ino, 0, NULL, &psize);
}

bool_t file_create(gpid_t svr, mode_t mode, unsigned int *p_ino){
	/* Prepare request.
	 */
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_CREATE;
	req.mode = mode;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return False;
	}
	*p_ino = reply.ino;
	return reply.status == FILE_OK;
}

bool_t file_chown(gpid_t svr, unsigned int ino, unsigned int uid) {
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_CHOWN;
	req.ino = ino;
	req.uid = uid;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return False;
	}
	return reply.status == FILE_OK;
}

bool_t file_chmod(gpid_t svr, unsigned int ino, mode_t mode) {
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_CHMOD;
	req.ino = ino;
	req.mode = mode;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return False;
	}
	return reply.status == FILE_OK;
}

bool_t file_read(gpid_t svr, unsigned int ino, unsigned long offset,
										void *addr, unsigned int *psize){
	/* Prepare request.
	 */
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_READ;
	req.ino = ino;
	req.offset = offset;
	req.size = *psize;

	/* Allocate reply.
	 */
	struct file_reply *reply = (struct file_reply *) malloc(sizeof(*reply) + *psize);
	unsigned int reply_size = sizeof(*reply) + *psize;

	/* Do the RPC.
	 */
	int n = sys_rpc(svr, &req, sizeof(req), reply, reply_size);
	if (n < (int) sizeof(*reply)) {
		free(reply);
		return False;
	}
	if (reply->status != FILE_OK) {
		free(reply);
		return False;
	}
	n -= sizeof(*reply);
	memcpy(addr, &reply[1], n);
	*psize = n;
	free(reply);
	return True;
}

bool_t file_write(gpid_t svr, unsigned int ino, unsigned long offset,
										const void *addr, unsigned int size){
	/* Prepare request.
	 */
	struct file_request *req =
				(struct file_request *) malloc(sizeof(*req) + size);
	memset(req, 0, sizeof(*req));
	req->type = FILE_WRITE;
	req->ino = ino;
	req->offset = offset;
	req->size = size;
	memcpy(&req[1], addr, size);

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, req, sizeof(*req) + size, &reply, sizeof(reply));
	free(req);
	if (result < (int) sizeof(reply)) {
		return False;
	}
	return reply.status == FILE_OK;
}

bool_t file_stat(gpid_t svr, unsigned int ino, struct file_stat *pstat){
	/* Prepare request.
	 */
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_STAT;
	req.ino = ino;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return False;
	}
	*pstat = reply.stat;
	return reply.status == FILE_OK;
}

bool_t file_setsize(gpid_t svr, unsigned int ino, unsigned long size){
	/* Prepare request.
	 */
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_SETSIZE;
	req.ino = ino;
	req.offset = size;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return False;
	}
	return reply.status == FILE_OK;
}

bool_t file_delete(gpid_t svr, unsigned int ino){
	/* Prepare request.
	 */
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_DELETE;
	req.ino = ino;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return False;
	}
	return reply.status == FILE_OK;
}

bool_t file_set_flags(gpid_t svr, unsigned int ino, unsigned long flags){
	/* Prepare request.
	 */
	struct file_request req;
	memset(&req, 0, sizeof(req));
	req.type = FILE_SET_FLAGS;
	req.ino = ino;
	req.offset = flags;

	/* Do the RPC.
	 */
	struct file_reply reply;
	int result = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (result < (int) sizeof(reply)) {
		return False;
	}
	return reply.status == FILE_OK;
}
