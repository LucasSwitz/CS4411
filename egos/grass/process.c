#pragma GCC diagnostic ignored "-Wunknown-pragmas"

#include <inttypes.h>
#include <stdio.h>
#include <ucontext.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include "../earth/earth.h"
#include "../earth/myalloc.h"
#include "../earth/mem.h"
#include "../earth/tlb.h"
#include "../earth/intr.h"
#include "../earth/clock.h"
#include "../earth/log.h"
#include "../shared/queue.h"
#include "../shared/syscall.h"
#include "../shared/block.h"
#include "../shared/file.h"
#include "../shared/dir.h"
#include "../shared/ema.h"
#include "../shared/context.h"
#include "exec.h"
#include "process.h"

/* The Earth layer is a bit ununusal in that we can specify the physical memory
 * and TLB size in software.
 */
#define TLB_SIZE		16			// #entries in TLB
#define PHYS_FRAMES		128			// #physical frames

#define MAX_PROCS		100			// maximum #processes

/* The process that is currently running.  This variable is external
 * and can be used by other modules.
 */
struct process *proc_current;


/* Run (aka ready) queue.
 */
static struct queue proc_runnable;


/* A frame is a physical page.
 */
struct frame {
	char contents[PAGESIZE];
};
static struct frame *proc_frames;
static struct queue proc_freeframes;		// list of free frames

/* Information about frames.
 */
#ifdef HW_PAGING	//<<<<HW_PAGING
/* Information about frames is kept here.
 */
struct frame_info {
	enum { FI_FREE, FI_UNREF, FI_REF, FI_PINNED } status;
	struct page_info *pi;					// points to frame owner
};
static struct frame_info proc_frame_info[PHYS_FRAMES];

static struct queue proc_freeblocks;		// list of free blocks
#endif //>>>>HW_PAGING

/* Other global variables.
 */
static unsigned int proc_nprocs;			// #processes
static unsigned int proc_nrunnable;			// #runnable processes
static struct queue proc_free;				// free processes
static struct process *proc_next;			// next process to run after ctx switch
static struct process proc_set[MAX_PROCS];	// set of all processes
static bool_t proc_shutting_down;			// cleaning up
static unsigned long proc_curfew;			// when to shut down


static void proc_cleanup(){
	printf("final clean up\n\r");

#ifdef HW_PAGING	//<<<<HW_PAGING
	/* Release the free blocks list.
	 */
	unsigned int block;
	while (queue_get_uint(&proc_freeblocks, &block))
		;
#endif //>>>>HW_PAGING

	/* Release the free frames list.
	 */
	unsigned int frame;
	while (queue_get_uint(&proc_freeframes, &frame))
		;

	/* Release the free process list.
	 */
	while (queue_get(&proc_free) != 0)
		;

	/* Release the run queue.
	 */
	while (queue_get(&proc_runnable) != 0)
	 	;
	queue_release(&proc_runnable);

	my_dump(False);		// print info about allocated memory
}

/* Initialize a message queue.
 */
static void mq_init(struct msg_queue *mq){
	mq->waiting = False;
	queue_init(&mq->messages);
}

/* Allocate a process structure.
 */
static struct process *proc_alloc(gpid_t owner, char *descr, unsigned int uid){
	static gpid_t pid_gen = 1;				// to generate new process ids
	struct process *p = queue_get(&proc_free);

	if (p == 0) {
		fprintf(stderr, "proc_alloc: no more slots\n");
		return 0;
	}
	memset(p, 0, sizeof(*p));
	p->pid = pid_gen++;
	p->uid = uid;
	p->owner = owner;
	p->descr = descr;
	p->state = PROC_RUNNABLE;
	proc_nprocs++;
	proc_nrunnable++;
	p->kernel_sp = (address_t) &p->kernelstack[KERNEL_STACK_SIZE - 8];
	unsigned int i;
	for (i = 0; i < MSG_NTYPES; i++) {
		mq_init(&p->mboxes[i]);
	}

	return p;
}

/* Release a process structure, the final step in the death of a process.
 */
static void proc_release(struct process *proc){
	assert(proc->state == PROC_ZOMBIE);

	/* Release any messages that might still be on the queues.
	 */
	unsigned int i;
	for (i = 0; i < MSG_NTYPES; i++) {
		struct msg_queue *mq = &proc->mboxes[i];
		struct message *msg;

		while ((msg = queue_get(&mq->messages)) != 0) {
			free(msg->contents);
			free(msg);
		}
		queue_release(&mq->messages);
	}

	/* Release the message buffer.
	 */
	if (proc->msgbuf != 0) {
		free(proc->msgbuf);
	}

	/* Invoke the cleanup function if any.
	 */
	if (proc->finish != 0) {
		(*proc->finish)(proc->arg);
	}

	proc_nprocs--;
	proc->state = PROC_FREE;
	queue_add(&proc_free, proc);
}

/* Put the current process on the run queue.
 */
static void proc_to_runqueue(struct process *p){
	assert(p->state == PROC_RUNNABLE);
	queue_add(&proc_runnable, p);
}

/* Find a process by process id.
 */
struct process *proc_find(gpid_t pid){
	struct process *p;

	for (p = proc_set; p < &proc_set[MAX_PROCS]; p++) {
		if (p->state != PROC_FREE && p->pid == pid) {
			return p;
		}
	}
	return 0;
}

/* Current process wants to wait for a message on a particular queue
 * that it owns.
 */
bool_t proc_recv(enum msg_type mtype, unsigned int max_time,
								void *contents, unsigned int *psize, gpid_t *psrc){
	assert(proc_current->state == PROC_RUNNABLE);
	struct msg_queue *mq = &proc_current->mboxes[mtype];
	assert(!mq->waiting);

	/* If there are no messages, wait.
	 */
	if (queue_empty(&mq->messages)) {
		mq->waiting = True;
		proc_current->state = PROC_WAITING;
		proc_nrunnable--;
		if (max_time != 0) {
			proc_current->exptime = sys_gettime() + max_time;
			proc_current->alarm_set = True;
		}
		proc_yield();
		assert(!mq->waiting);
	}
	else {
		/* There shouldn't be a reply yet if this is part of an RPC.
		 */
		assert(mtype != MSG_REPLY);
	}

	/* Get the message, if any.
	 */
	assert(proc_current->state == PROC_RUNNABLE);
	struct message *msg = queue_get(&mq->messages);
	if (msg == 0) {
		return False;
	}

	/* Copy the message to the recipient.
	 */
	if (msg->size < *psize) {
		*psize = msg->size;
	}
	memcpy(contents, msg->contents, *psize);
	*psrc = msg->src;
	free(msg->contents);
	free(msg);
	return True;
}

/* Send a message of the given type to the given process.  This routine
 * may be called from an interrupt handler, and so proc_current is not
 * necessarily the source of the message.
 */
bool_t proc_send(gpid_t src_pid, gpid_t dst_pid, enum msg_type mtype,
							const void *contents, unsigned int size){
	/* See who the destination process is.
	 */
	struct process *dst = proc_find(dst_pid);
	if (dst == 0) {
		printf("proc_send %u: unknown destination %u\n\r", src_pid, dst_pid);
		return False;
	}
	if (dst->state == PROC_ZOMBIE) {
		return False;
	}

	struct msg_queue *mq = &dst->mboxes[mtype];

	/* If it's a response, the process should be waiting for one.
	 */
	if (mtype == MSG_REPLY) {
		if (dst->state != PROC_WAITING || !mq->waiting
											|| dst->server != src_pid) {
			fprintf(stderr, "%u: dst %u (%u) not waiting for reply (%d %d %u)\n",
									src_pid, dst_pid, dst->pid,
									dst->state, mq->waiting, dst->server);
			return False;
		}
	}

	/* Copy the message.
	 */
	struct message *msg = new_alloc(struct message);
	msg->src = src_pid;
	msg->contents = malloc(size);
	memcpy(msg->contents, contents, size);
	msg->size = size;

	/* Add the message to the message queue.
	 */
	queue_add(&mq->messages, msg);

	/* Wake up the process if it's waiting.
	 */
	if (mq->waiting) {
		assert(dst->state == PROC_WAITING);
		dst->state = PROC_RUNNABLE;
		proc_nrunnable++;
		dst->alarm_set = False;

		/* dst == proc_current is possible if the process is waiting for
		 * input.  In that case it shouldn't be put on the runnable queue
		 * because it will automatically resume from intr_suspend().
		 */
		if (dst != proc_current) {

			proc_to_runqueue(dst);
		}
		mq->waiting = False;
	}

	return True;
}

/* Wake up the given process that is waiting for a message.
 */
static void proc_wakeup(struct process *p){
	assert(p->state == PROC_WAITING);
	p->state = PROC_RUNNABLE;
	proc_nrunnable++;
	p->alarm_set = False;
	if (p != proc_current) {
		proc_to_runqueue(p);
	}
	unsigned int i;
	for (i = 0; i < MSG_NTYPES; i++) {
		p->mboxes[i].waiting = False;
	}
}

/* All processes come here when they die.  The process may still executing on
 * its kernel stack, but we can clean up most everything else, and also notify
 * the owner of the process.
 */
void proc_term(struct process *proc, int status){
	assert(proc_nprocs > 0);

	if (proc->state == PROC_RUNNABLE) {
		proc_nrunnable--;
	}

	/* See if the owner is still around.
	 */
	if (proc->state != PROC_ZOMBIE) {
		proc->state = PROC_ZOMBIE;

		/* Notify owner, if any.
		 */
		struct process *owner = proc_find(proc->owner);
		if (owner != 0) {
			struct msg_event mev;
			memset(&mev, 0, sizeof(mev));
			mev.type = MEV_PROCDIED;
			mev.pid = proc->pid;
			mev.status = status;
			(void) proc_send(proc->pid, owner->pid, MSG_EVENT, &mev, sizeof(mev));
		}

		/* Also, if this is a server that clients are waiting for, fail
		 * their rpcs.
		 */
		struct process *p;
		for (p = proc_set; p < &proc_set[MAX_PROCS]; p++) {
			if (p->state == PROC_WAITING &&
					p->mboxes[MSG_REPLY].waiting && p->server == proc->pid) {
				printf("Process %u waiting for reply from %u\n\r", p->pid, proc->pid);
				proc_wakeup(p);
			}
		}
	}

	/* Release any allocated frames.
	 */
	unsigned int i;
	for (i = 0; i < VIRT_PAGES; i++) {
		switch (proc->pages[i].status) {
		case PI_UNINIT:
			break;
		case PI_VALID:
#ifdef HW_PAGING	//<<<<HW_PAGING
			assert(proc_frame_info[proc->pages[i].u.frame].status != FI_FREE);
			proc_frame_info[proc->pages[i].u.frame].status = FI_FREE;
#endif //>>>>HW_PAGING
			// printf("release frame %u from process %u\n\r", proc->pages[i].u.frame, proc->pid);
			queue_add_uint(&proc_freeframes, proc->pages[i].u.frame);
			break;
#ifdef HW_PAGING	//<<<<HW_PAGING
		case PI_ONDISK:
			queue_add_uint(&proc_freeblocks, proc->pages[i].u.block);
			break;
#endif //>>>>HW_PAGING
		default:
			assert(0);
		}
	}
}

/* What exactly needs to happen to a process to kill it depends on its state.
 */
void proc_zap(struct process *killer, struct process *p, int status){
	/* See if it's allowed.
	 */
	if (killer != 0 && killer->uid != 0 && killer->uid != p->uid) {
		printf("proc_zap: only allowed to kill own processes %u %u\n\r", killer->uid, p->uid);
		return;
	}
	assert(proc_nprocs > 0);

	switch (p->state) {
	case PROC_RUNNABLE:
		proc_term(p, status);
		break;
	case PROC_WAITING:
		proc_term(p, status);
		if (p != proc_current) {
			proc_release(p);
		}
		break;
	case PROC_ZOMBIE:
		break;
	default:
		assert(0);
	}
}

/* Kill process pid, or all interruptable processes if pid == 0.
 */
void proc_kill(gpid_t killer, gpid_t pid, int status){
	struct process *p, *k;

	if ((k = proc_find(killer)) == 0) {
		printf("proc_kill: unknown killer");
		return;
	}

	/* If pid == 0, kill all interruptable processes.
	 */
	if (pid == 0) {
		for (p = proc_set; p < &proc_set[MAX_PROCS]; p++) {
			if (p->state != PROC_FREE && p->interruptable) {
				proc_zap(k, p, status);
			}
		}
		if (proc_current->state == PROC_ZOMBIE) {
			proc_yield();
		}
		return;
	}

	if ((p = proc_find(pid)) == 0) {
		printf("proc_kill: no process %u\n", pid);
	}
	else {
		proc_zap(k, p, status);

		/* If terminated self, don't return but give up the CPU.
		 */
		if (p->state == PROC_ZOMBIE) {
			proc_yield();
		}
	}
}

/* Entry point of new processes.  It is invoked from ctx_start().
 */
void ctx_entry(void){
	/* Invoke the new process.
	 */
	proc_current = proc_next;
	(*proc_current->start)(proc_current->arg);

	printf("process %u terminated!!\n\r", proc_current->pid);
	sys_exit(STAT_ILLMEM);
}

/* Right after a context switch we need to do some administration.
 *	- clean up the previous process if it's a zombie
 *	- update proc_current
 *	- flush the TLB
 */
static void proc_after_switch(){
	assert(proc_next != proc_current);
	assert(proc_next->state == PROC_RUNNABLE);

	/* Clean up the old process if it's a zombie.
	 */
	if (proc_current->state == PROC_ZOMBIE) {
		proc_release(proc_current);
	}

	/* Update the proc_current pointer.
	 */
	proc_current = proc_next;

	/* Flush the TLB
	 */
	tlb_flush();
}

/* Create a process.  Initially only contains a stack segment.  Boolean
 * 'user' is true is the process should run as a user process with a
 * paged stack and interrupts enabled.
 */
gpid_t proc_create_uid(gpid_t owner, char *descr, void (*start)(void *), void *arg, unsigned int uid){
	/* Don't allow new processes to be created.
	 */
	if (proc_shutting_down) {
		return 0;
	}

	/* See who the owner is.
	 */
	struct process *parent;
	if ((parent = proc_find(owner)) == 0) {
		fprintf(stderr, "proc_create_uid: no parent\n");
		return 0;
	}

	/* If not the superuser, new process has same uid as parent.
	 */
	if (parent->uid != 0) {
		uid = parent->uid;
	}

	/* Allocate a process control block.
	 */
	struct process *proc;
	if ((proc = proc_alloc(owner, descr, uid)) == 0) {
		fprintf(stderr, "proc_create_uid: out of memory\n");
		return 0;
	}

	proc->start = start;
	proc->arg = arg;

	/* Initialize the signal stack for first use.
	 */
	sigstk_init(proc);

	/* Save the process id for the result value.
	 */
	gpid_t pid = proc->pid;

	/* Put the current process on the run queue.
	 */
	proc_to_runqueue(proc_current);

	/* Start the new process, which commences at ctx_entry();
	 */
	proc_next = proc;

#ifdef NO_UCONTEXT
	ctx_start(&proc_current->kernel_sp, proc->kernel_sp);
#else
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

	getcontext(&proc->ctx);
	proc->ctx.uc_stack.ss_sp = proc->kernelstack;
    proc->ctx.uc_stack.ss_size = KERNEL_STACK_SIZE;
    proc->ctx.uc_link = 0;
	makecontext(&proc->ctx, ctx_entry, 0);
	swapcontext(&proc_current->ctx, &proc->ctx);

#pragma clang diagnostic push
#endif

	proc_after_switch();
	return pid;
}

/* Like proc_create_uid, but with uid = 0.
 */
gpid_t proc_create(gpid_t owner, char *descr, void (*start)(void *), void *arg){
	return proc_create_uid(owner, descr, start, arg, 0);
}

/* Yield to another process.  This is basically the main scheduler.
 */
void proc_yield(void){
	/* See if there's any I/O to be done.
	 */
	intr_suspend(0);

	assert(proc_nprocs > 0);

	/* Try to find a process to run.
	 */
	for (;;) {
		/* First check if there are any processes waiting for a timeout
		 * that are now runnable.  Also keep track of how long until
		 * the next timeout, if any.
		 */
		struct process *p;
		unsigned long now = sys_gettime(), next = now + 1000;
		if (proc_curfew != 0 && now > proc_curfew) {
			printf("cleanup: timed out\n");
			proc_cleanup();
			exit(0);
		}
		for (p = proc_set; p < &proc_set[MAX_PROCS]; p++) {
			if (proc_shutting_down && p->state != PROC_FREE) {
				proc_zap(0, p, STAT_SHUTDOWN);
			}
			else if (p->state == PROC_WAITING && p->alarm_set) {
				if (p->exptime <= now) {
					proc_wakeup(p);
				}
				if (p->exptime < next) {
					next = p->exptime;
				}
			}
		}

		/* See if there are other processes to run.  If so, we're done.
		 */
		while ((proc_next = queue_get(&proc_runnable)) != 0) {
			if (proc_next->state == PROC_RUNNABLE) {
				break;
			}
			assert(proc_next->state == PROC_ZOMBIE);
			proc_release(proc_next);
		}

		/* There should always be at least one process.
		 */
		assert(proc_nprocs > 0);

		/* See if we found a suitable process to schedule.
		 */
		if (proc_next != 0) {
			break;
		}

		/* If I'm runnable, I can simply return now without a context switch,
		 * TLB flush and other nasty overheads.  Note: the process may not be
		 * runnable any more because processes can be killed.
		 */
		if (proc_current->state == PROC_RUNNABLE) {
			return;
		}

		/* If this is the last process remaining when shutting down,
		 * actually clean things up.
		 */
		if (proc_nprocs == 1 && proc_shutting_down) {
			proc_cleanup();
			exit(0);
		}

		/* No luck.  We'll wait for a while.
		 */
		intr_suspend(next - now);

	}


	assert(proc_next != proc_current);
	if (proc_next->pid == proc_current->pid) {
		printf("==> %u\n\r", proc_current->pid);
		proc_dump();
	}
	assert(proc_next->pid != proc_current->pid);
	assert(proc_next->state == PROC_RUNNABLE);

	/* Make sure the current process is schedulable.
	 */
	if (proc_current->state == PROC_RUNNABLE) {
		proc_to_runqueue(proc_current);
	}

	if (proc_shutting_down) {
		printf("proc_yield: switch from %u to %u next\n", proc_current->pid, proc_next->pid);
	}


#ifdef NO_UCONTEXT
	ctx_switch(&proc_current->kernel_sp, proc_next->kernel_sp);
#else
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	swapcontext(&proc_current->ctx, &proc_next->ctx);
#pragma clang diagnostic push
#endif

	proc_after_switch();
}

#ifdef HW_PAGING	//<<<<HW_PAGING
#ifdef PAGE_TO_FILE

extern fid_t pgfile;

static void frame_write(unsigned int frame, unsigned int block) {
	bool_t success = file_write(pgfile.server, pgfile.ino,
			 block * PAGESIZE, &proc_frames[frame], PAGESIZE);
	assert(success);
}

static void frame_read(unsigned int frame, unsigned int block) {
	unsigned int size = PAGESIZE;
	bool_t success = file_read(pgfile.server, pgfile.ino,
			 block * PAGESIZE, &proc_frames[frame], &size);
	assert(success);
	assert(size == PAGESIZE);
}

#else // !PAGE_TO_FILE

extern fid_t pgdev;

#define BLOCKS_PER_PAGE			(PAGESIZE / BLOCK_SIZE)

/* Write the given physical frame to disk.  "block" is actually measured in pages.
 * A page covers BLOCKS_PER_PAGE blocks.
 */
static void frame_write(unsigned int frame, unsigned int block) {
	unsigned int i, offset = block * BLOCKS_PER_PAGE;
	const char *mem = (char *) &proc_frames[frame];

	for (i = 0; i < BLOCKS_PER_PAGE; i++, offset++) {
		bool_t success = block_write(pgdev.server, pgdev.ino, offset, mem);
		assert(success);
		mem += BLOCK_SIZE;
	}
}

static void frame_read(unsigned int frame, unsigned int block) {
	unsigned int i, offset = block * BLOCKS_PER_PAGE;
	char *mem = (char *) &proc_frames[frame];

	for (i = 0; i < BLOCKS_PER_PAGE; i++, offset++) {
		bool_t success = block_read(pgdev.server, pgdev.ino, offset, mem);
		assert(success);
		mem += BLOCK_SIZE;
	}
}
#endif // PAGE_TO_FILE
#endif //>>>>HW_PAGING

/* Allocate a frame for the given page.
 */
static void proc_frame_alloc(struct page_info *pi){
	/* First check to see if there's a frame on the free list.
	 */
	unsigned int frame_no;
	bool_t success = queue_get_uint(&proc_freeframes, &frame_no);
	if (success) {
#ifdef HW_PAGING	//<<<<HW_PAGING
		assert(proc_frame_info[frame_no].status == FI_FREE);
		proc_frame_info[frame_no].status = FI_REF;
		proc_frame_info[frame_no].pi = pi;
#endif //>>>>HW_PAGING
		// printf("assign frame %u to process %u\n\r", frame_no, proc_current->pid);
		pi->status = PI_VALID;
		pi->u.frame = frame_no;
		return;
	}

#ifdef HW_PAGING	//<<<<HW_PAGING
	/* Run the clock algorithm to release a frame that is currently in use.
	 */
	static unsigned int clock_hand;

	unsigned int i;
	for (i = 0; i < 2 * PHYS_FRAMES; i++) {
		switch (proc_frame_info[clock_hand].status) {
		case FI_PINNED:
			break;
		case FI_REF:
			assert(proc_frame_info[clock_hand].pi != 0);
			proc_frame_info[clock_hand].status = FI_UNREF;
			break;
		case FI_UNREF:
			{
				/* Try to allocate a block on the paging device.
				 */
				unsigned int block;
				if (!queue_get_uint(&proc_freeblocks, &block)) {
					fprintf(stderr, "paging device full\n\r");
					assert(0);
				}

				/* Update the owning process's page table.
				 */
				proc_frame_info[clock_hand].pi->status = PI_ONDISK;
				proc_frame_info[clock_hand].pi->u.block = block;

				/* Save the frame to the block.  Pin the frame so
				 * nobody else tries to allocate or mess with it.
				 */
				proc_frame_info[clock_hand].status = FI_PINNED;
				frame_write(clock_hand, block);

				/* Assign the frame to the page that needs it.
				 */
				proc_frame_info[clock_hand].status = FI_REF;
				proc_frame_info[clock_hand].pi = pi;
				pi->status = PI_VALID;
				pi->u.frame = clock_hand;
				clock_hand = (clock_hand + 1) % PHYS_FRAMES;
				return;
			}
		default:
			assert(0);
		}
		clock_hand = (clock_hand + 1) % PHYS_FRAMES;
	}
#endif //>>>>HW_PAGING

	panic("proc_frame_alloc: out of frames");
}

/* Initialize a newly allocated frame.  See if it's in the executable.
 */
static void frame_init(struct frame *frame, unsigned int abs_page){
	struct process *p = proc_current;

	struct exec_header *eh = &p->hdr.eh;
	if (eh->eh_base <= abs_page && abs_page < eh->eh_base + eh->eh_size) {
		/* Read the page.
		 */
		unsigned int size = PAGESIZE;
		bool_t success = file_read(p->executable.server,
			p->executable.ino,
			(eh->eh_offset + (abs_page - eh->eh_base)) * PAGESIZE,
			frame, &size);
		if (!success) {
			printf("--> %u %u\n", p->executable.server, p->executable.ino);
		}
		assert(success);
		assert(size <= PAGESIZE);
		if (size < PAGESIZE) {
			memset((char *) frame + size, 0, PAGESIZE - size);
		}
	}

	/* Otherwise zero-init it.
	 */
	else {
		memset(frame, 0, PAGESIZE);
	}
}

/* Got a page fault at the given virtual address.  Map the page.
 */
void proc_pagefault(address_t virt){
	static unsigned int tlb_index;			// to allocate TLB entries
	struct process *p = proc_current;

	assert(p->state == PROC_RUNNABLE);

	// printf("GOT PAGEFAULT: %"PRIaddr"\n", virt);

	/* First see if it's a legal address.
	 */
	if (virt < VIRT_BASE || virt >= VIRT_TOP) {
		printf("proc_pagefault pid=%u: address %"PRIaddr" out of range\n\r",
								p->pid, virt);
		proc_term(p, STAT_ILLMEM);
		proc_yield();
	}

	/* Figure out which page it is.
	 */
	unsigned int rel_page = (virt - VIRT_BASE) / PAGESIZE;
	unsigned int abs_page = virt / PAGESIZE;
	// printf("proc_pagefault: fault in page %x (%x)\n", abs_page, rel_page);

	/* Make sure the entry isn't already mapped.
	 */
	if (tlb_get_entry(abs_page) >= 0) {
		assert(0);
	}

	switch (p->pages[rel_page].status) {
	case PI_UNINIT:
		proc_frame_alloc(&p->pages[rel_page]);
		frame_init(&proc_frames[p->pages[rel_page].u.frame], abs_page);
		break;
	case PI_VALID:
#ifdef HW_PAGING	//<<<<HW_PAGING
		assert(proc_frame_info[p->pages[rel_page].u.frame].status != FI_FREE);
		assert(proc_frame_info[p->pages[rel_page].u.frame].pi == &p->pages[rel_page]);
		proc_frame_info[p->pages[rel_page].u.frame].status = FI_REF;
#endif //>>>>HW_PAGING
		break;
#ifdef HW_PAGING	//<<<<HW_PAGING
	case PI_ONDISK:
		{
			unsigned int block = p->pages[rel_page].u.block;
			proc_frame_alloc(&p->pages[rel_page]);
			frame_read(p->pages[rel_page].u.frame, block);
		}
		break;
#endif //>>>>HW_PAGING
	default:
		assert(0);
	}

	/* Map the page to the frame.
	 */
	assert(p->pages[rel_page].status == PI_VALID);
	struct frame *frame = &proc_frames[p->pages[rel_page].u.frame];
	tlb_map(tlb_index, abs_page, frame, P_READ | P_WRITE | P_EXEC);
	tlb_index = (tlb_index + 1) % TLB_SIZE;
}

/* Entry point for all interrupts.
 */
void proc_got_interrupt(){
	switch (proc_current->intr_type) {
	case INTR_PAGE_FAULT:
		proc_pagefault((address_t) proc_current->intr_arg);
		break;
	case INTR_SYSCALL:
		proc_syscall();
		break;
	case INTR_CLOCK:
		proc_yield();
		break;
	case INTR_IO:
		/* There might be a process that is now runnable.
		 */
		proc_yield();
		break;
	default:
		assert(0);
	}
}

/* Dump the state of all processes.
 */
void proc_dump(void){
	struct process *p;

	printf("\n\r");
	printf("%u processes (current = %u, nrunnable = %u):\n\r",
		proc_nprocs, proc_current->pid, proc_nrunnable);

	printf("PID   DESCRIPTION  UID STATUS      OWNER ALARM   EXEC\n\r");
	for (p = proc_set; p < &proc_set[MAX_PROCS]; p++) {
		if (p->state == PROC_FREE) {
			continue;
		}
		printf("%4u:", p->pid);
		if (p->descr != 0) {
			printf(" %-12.12s ", p->descr);
		}
		else {
			printf("          ");
		}
		printf("%3u ", p->uid);
		switch (p->state) {
		case PROC_RUNNABLE:
			printf(p == proc_current ? "RUNNING    " : "RUNNABLE   ");
			break;
		case PROC_WAITING:
			if (p->mboxes[MSG_REQUEST].waiting) {
				printf("AWAIT REQST");
			}
			if (p->mboxes[MSG_REPLY].waiting) {
				printf("AWAIT %5u", p->server);
			}
			if (p->mboxes[MSG_EVENT].waiting) {
				printf("AWAIT EVENT");
			}
			break;
		case PROC_ZOMBIE:
			printf("ZOMBIE     ");
			break;
		default:
			assert(0);
		}
		printf(" %5u ", p->owner);
		if (p->state == PROC_WAITING && p->alarm_set) {
			printf("%5ld", p->exptime - sys_gettime());
		}
		else {
			printf("     ");
		}
		if (p->executable.server != 0) {
			printf("   %u:%u", p->executable.server, p->executable.ino);
		}
		printf("\n\r");
	}
}

/* Initialize this module.
 */
void proc_initialize(void){
	/* Sanity check sme constants.
	 */
	assert(PAGESIZE >= BLOCK_SIZE);
	assert(PAGESIZE % BLOCK_SIZE == 0);


	unsigned int i;

	/* Initialize the TLB.
	 */
	tlb_initialize(TLB_SIZE);

	/* Allocate the physical memory.
	 */
	proc_frames = mem_initialize(PHYS_FRAMES, P_READ | P_WRITE);

	/* Create a free list of physical frames.
	 */
	queue_init(&proc_freeframes);
	for (i = 0; i < PHYS_FRAMES; i++) {
		queue_add_uint(&proc_freeframes, i);
	}

#ifdef HW_PAGING	//<<<<HW_PAGING
	/* Create a free list of blocks on the paging device.
	 */
	queue_init(&proc_freeblocks);
	for (i = 0; i < PG_DEV_SIZE; i++) {
		queue_add_uint(&proc_freeblocks, i);
	}
#endif //>>>>HW_PAGING

	/* Initialize the free list of processes.
	 */
	queue_init(&proc_free);
	for (i = 0; i < MAX_PROCS; i++) {
		proc_set[i].pid = i;
		queue_add(&proc_free, &proc_set[i]);
	}

	/* Initialize the run queue (aka ready queue).
	 */
	queue_init(&proc_runnable);

	/* Allocate a process record for the current process.
	 */
	proc_current = proc_alloc(1, "main", 0);
}

/* Invoked when shutting down the kernel.
 */
void proc_shutdown(){
	proc_shutting_down = True;
	proc_curfew = clock_now() + 3000;
}
