#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include "egos.h"
#include "file.h"
#include "dir.h"

/* This is the client code for looking up an inode number.
 */
bool_t dir_lookup(gpid_t svr, fid_t dir, const char *path, fid_t *pfid){
	/* Prepare request.
	 */
	int n = strlen(path);
	struct dir_request *req = malloc(sizeof(*req) + n);
	memset(req, 0, sizeof(*req));
	req->type = DIR_LOOKUP;
	req->dir = dir;
	req->size = n;
	memcpy(req + 1, path, n);

	/* Do the RPC.
	 */
	struct dir_reply reply;
	int r = sys_rpc(svr, req, sizeof(*req) + n, &reply, sizeof(reply));
	free(req);
	if (r != sizeof(reply) || reply.status != DIR_OK) {
		return False;
	}
	*pfid = reply.fid;
	return True;
}

bool_t dir_insert(gpid_t svr, fid_t dir, const char *path, fid_t fid){
	/* Prepare request.
	 */
	int n = strlen(path);
	struct dir_request *req = malloc(sizeof(*req) + n);
	memset(req, 0, sizeof(*req));
	req->type = DIR_INSERT;
	req->dir = dir;
	req->fid = fid;
	req->size = n;
	memcpy(req + 1, path, n);

	/* Do the RPC.
	 */
	struct dir_reply reply;
	int r = sys_rpc(svr, req, sizeof(*req) + n, &reply, sizeof(reply));
	free(req);
	if (r != sizeof(reply) || reply.status != DIR_OK) {
		return False;
	}
	return True;
}

/* This is currently implemented just by creating a file.
 */
bool_t dir_create2(gpid_t dirsvr, gpid_t filesvr, mode_t mode, fid_t dir, const char *path, fid_t *p_fid){
	fid_t fid;

	/* First create a new file.
	 */
	bool_t r = file_create(filesvr, mode, &fid.ino);
	if (!r) {
		return False;
	}
	fid.server = filesvr;

	/* Add the "." and ".." entries.
	 */
	struct dir_entry de;
	de.fid = fid;
	strcpy(de.name, "..dir");
	(void) file_write(fid.server, fid.ino, 0, &de, DIR_ENTRY_SIZE);
	de.fid = dir;
	strcpy(de.name, "...dir");
	(void) file_write(fid.server, fid.ino, DIR_ENTRY_SIZE, &de, DIR_ENTRY_SIZE);

	/* Insert the new file in the directory.
	 */
	(void) dir_insert(dirsvr, dir, path, fid);
	if (p_fid != 0) {
		*p_fid = fid;
	}
	return True;
}

/* This is currently implemented just by creating a file.
 */
bool_t dir_create(gpid_t svr, fid_t dir, const char *path, fid_t *p_fid){
	return dir_create2(svr, dir.server, P_FILE_DEFAULT, dir, path, p_fid);
}
