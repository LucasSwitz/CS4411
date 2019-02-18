#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include "egos.h"
#include "spawn.h"

#define ALIGNMENT	16		// for floating point stuff...

/* This file is included in the kernel as well as user space, with
 * different include files.
 */

bool_t spawn_exec(gpid_t svr, fid_t executable, const char *argb, unsigned int size,
						bool_t interruptable, unsigned int uid, gpid_t *ppid){
	/* Prepare request.
	 */
	struct spawn_request *req = malloc(sizeof(*req) + size);
	memset(req, 0, sizeof(*req));
	req->type = SPAWN_EXEC;
	req->u.exec.executable = executable;
	req->u.exec.size = size;
	req->u.exec.interruptable = interruptable;
	req->u.exec.uid = uid;
	memcpy(req + 1, argb, size);

	/* Do the RPC.
	 */
	struct spawn_reply reply;
	int n = sys_rpc(svr, req, sizeof(*req) + size, &reply, sizeof(reply));
	free(req);
	if (n < (int) sizeof(reply)) {
		return False;
	}
	if (reply.status != SPAWN_OK) {
		return False;
	}
	*ppid = reply.u.pid;
	return True;
}

bool_t spawn_kill(gpid_t svr, gpid_t pid, int status){
	/* Prepare request.
	 */
	struct spawn_request req;
	memset(&req, 0, sizeof(req));
	req.type = SPAWN_KILL;
	req.u.kill.pid = pid;
	req.u.kill.status = status;

	/* Do the RPC.
	 */
	struct spawn_reply reply;
	int n = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (n < (int) sizeof(reply)) {
		return False;
	}
	return reply.status == SPAWN_OK;
}

/* Get the user identifier of process pid.
 */
bool_t spawn_getuid(gpid_t svr, gpid_t pid, unsigned int *p_uid){
	/* Prepare request.
	 */
	struct spawn_request req;
	memset(&req, 0, sizeof(req));
	req.type = SPAWN_GETUID;
	req.u.getuid.pid = pid;

	/* Do the RPC.
	 */
	struct spawn_reply reply;
	int n = sys_rpc(svr, &req, sizeof(req), &reply, sizeof(reply));
	if (n < (int) sizeof(reply)) {
		return False;
	}
	if (reply.status == SPAWN_OK) {
		*p_uid = reply.u.uid;
	}
	return reply.status == SPAWN_OK;
}

/* Create a stack frame for a new process.
 */
bool_t spawn_load_args(const struct grass_env *ge_init, int argc, char *const *argv,
							char **p_argb, unsigned int *p_size){
	/* Memory needed for argv and grass env.
	 */
	unsigned int total = (argc + 1) * sizeof(*argv) + sizeof(*GRASS_ENV);

	/* Add size of memory needed for args themselves and align.
	 */
	int i;
	for (i = 0; i < argc; i++) {
		total += strlen(argv[i]) + 1;
	}
	int rem = total % ALIGNMENT;
	if (rem > 0) {
		total += ALIGNMENT - rem;
	}

	/* Allocate this much memory and fill it with this info.  This goes
	 * right below GRASS_ENV on the stack.
	 */
	char *argb = malloc(total);
	char **v1 = (char **) argb, **v2 = (char **) ((char *) &GRASS_ENV[1] - total);
	char *s1 = (char *) &v1[argc + 1], *s2 = (char *) &v2[argc + 1];

	/* Find the location of the grass environment.
	 */
	struct grass_env *ge = (struct grass_env *) (argb + total) - 1;
	*ge = *ge_init;
	ge->argc = argc;
	ge->argv = v2;
	ge->envp = 0;

	/* Fill in the block.
	 */
	for (i = 0; i < argc; i++) {
		int len = strlen(argv[i]) + 1;
		strcpy(s1, argv[i]);
		*v1++ = s2;
		s1 += len;
		s2 += len;
	}
	*v1++ = 0;

	*p_argb = argb;
	*p_size = total;

	return True;
}
