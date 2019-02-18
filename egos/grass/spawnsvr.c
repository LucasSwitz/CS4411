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
#include "../shared/dir.h"
#include "../shared/file.h"
#include "../shared/spawn.h"
#include "../shared/context.h"
#include "exec.h"
#include "process.h"

static void spawn_respond(gpid_t src, enum spawn_status status, gpid_t pid){
	struct spawn_reply rep;
	rep.status = status;
	rep.u.pid = pid;
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* Run a user process.  The inode number for the file is in 'arg'.
*/
static void user_proc(void *arg){
	struct spawn_request *req = arg;

	proc_current->interruptable = req->u.exec.interruptable;

	/* Read the header of the executable.
	 */
	proc_current->executable = req->u.exec.executable;
	unsigned int size = sizeof(proc_current->hdr);
	bool_t success = file_read(proc_current->executable.server,
				proc_current->executable.ino, 0, &proc_current->hdr, &size);
	assert(success);
	assert(size == sizeof(proc_current->hdr));

/*
	printf("base = %x\n", proc_current->hdr.eh.teh_base);
	printf("offset = %u\n", proc_current->hdr.eh.teh_offset);
	printf("size = %u\n", proc_current->hdr.eh.teh_size);
	printf("nsegs = %u\n", proc_current->hdr.eh.teh_nsegments);
	printf("start = %"PRIaddr"\n", proc_current->hdr.eh.teh_start);
	printf("first = %x\n", proc_current->hdr.es[0].tes_first);
	printf("npages = %x\n", proc_current->hdr.es[0].tes_npages);
*/

	/* Set up the stack.  The argument block contains the new grass_env
	 * but must be updated with the right 'self' pid.
	 */
	struct grass_env *ge =
		(struct grass_env *) ((char *) &req[1] + req->u.exec.size) - 1;
	ge->self = sys_getpid();
	proc_current->user_sp = (address_t) &GRASS_ENV[1] - req->u.exec.size;
	copy_user((char *) proc_current->user_sp, (char *) (req + 1),
									req->u.exec.size, CU_TO_USER);

	/* Main loop of the user process, which basically involves jumping
	 * back and forth between:
	 *	a) the kernel stack of the process.
	 *	b) the interrupt stack of the process.
	 *	c) the (paged) user stack of the process.
	 *
	 * Interrupts are only interrupted when the process is running on
	 * the user stack.
	 */
	for (;;) {
		/* Set up the signal stack with the process's signal stack.
		 */
		intr_restore(proc_current->sigstack, (void *) proc_current->sig_sp);

		/* Now set the stack pointer into the signal stack, which will "return
		 * from interrupt" and cause the process to run in user space.
		 */
		ctx_switch(&proc_current->kernel_sp, proc_current->sig_sp);

		/* When we get back here, we are back on the kernel stack and have to
		 * save the signal stack.
		 */
		intr_save(proc_current->sigstack, (void *) proc_current->sig_sp);

		/* Also make sure the modified virtual pages are written back to
		 * the physical ones.
		 */
		tlb_sync();

		/* Handle the interrupt.
		 */
		proc_got_interrupt();
	}
}

/* Respond to an exec request.
 */
static void spawn_do_exec(struct spawn_request *req, gpid_t src, void *data, unsigned int size){
	if (req->u.exec.size != size) {
		printf("spawn_do_exec: size mismatch (%u != %u)\n", req->u.exec.size, size);
		spawn_respond(src, SPAWN_ERROR, 0);
		return;
	}

	/* Create the user process.
	 */
	gpid_t pid = proc_create_uid(src, "user", user_proc, req, req->u.exec.uid);

	/* Respond back to the client.
	 */
	spawn_respond(src, pid > 0 ? SPAWN_OK : SPAWN_ERROR, pid);
}

/* Respond to a kill request.
 */
static void spawn_do_kill(struct spawn_request *req, gpid_t src){
	proc_kill(src, req->u.kill.pid, req->u.kill.status);
	spawn_respond(src, SPAWN_OK, req->u.kill.pid);
}

/* Respond to a getuid request.
 */
static void spawn_do_getuid(struct spawn_request *req, gpid_t src){
	struct spawn_reply rep;
	struct process *p = proc_find(req->u.getuid.pid);

	if (p == 0) {
		rep.status = SPAWN_ERROR;
	}
	else {
		rep.status = SPAWN_OK;
		rep.u.uid = p->uid;
	}
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));
}

/* The 'spawn' server.  Currently it only supports a single command:
 * SPAWN_EXEC.
 */
static void spawn_proc(void *arg){
	printf("SPAWN SERVER: %u\n\r", sys_getpid());

	struct spawn_request *req = new_alloc_ext(struct spawn_request, PAGESIZE);
	for (;;) {
		gpid_t src;
		int req_size = sys_recv(MSG_REQUEST, 0, req, sizeof(req) + PAGESIZE, &src);
		if (req_size < 0) {
			printf("spawn server terminating\n\r");
			free(req);
			break;
		}

		assert(req_size >= (int) sizeof(*req));
		switch (req->type) {
		case SPAWN_EXEC:
			spawn_do_exec(req, src, req + 1, req_size - sizeof(*req));
			break;
		case SPAWN_KILL:
			spawn_do_kill(req, src);
			break;
		case SPAWN_GETUID:
			spawn_do_getuid(req, src);
			break;
		default:
			assert(0);
		}
	}
}

gpid_t spawn_init(void){
	return proc_create(1, "spawn", spawn_proc, 0);
}
