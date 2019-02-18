/* This file contains everything to do with the interface to grass processes.
 */

#define MAX_SEGMENTS		4
#define PG_DEV_SIZE			1024		// #pages on paging device

// Why does it have to be so large...?
#define KERNEL_STACK_SIZE	(64 * 1024)	// size of kernel stack of a process

/* A message consists of the source and a contents.
 */
struct message {
	gpid_t src;
	void *contents;
	unsigned int size;
};

/* Page info.
 */
struct page_info {
	enum {
		PI_UNINIT,
		PI_VALID,
#ifdef HW_PAGING	//<<<<HW_PAGING
		PI_ONDISK
#endif //>>>>HW_PAGING
	} status;
	union {
		unsigned int frame;		// if in memory
#ifdef HW_PAGING	//<<<<HW_PAGING
		unsigned int block;		// if on disk
#endif //>>>>HW_PAGING
	} u;
};

/* Message queue definition.
 */
struct msg_queue {
	bool_t waiting;					// true iff process is waiting for messages
	struct queue messages;			// list of messages that have arrived
};

/* One of these per process.
 */
struct process {
	gpid_t pid;					// process identifier
	char *descr;				// for dumps
	void (*start)(void *);		// starting point
	void (*finish)(void *);		// ending point
	void *arg;					// argument to these functions

	unsigned int uid;			// user identifier of this process

	/* The owner (or "parent") of a process is notified (with a MSG_EVENT)
	 * when this process dies.
	 */
	gpid_t owner;

	/* Process states.
	 */
	enum {
		PROC_FREE,				// this slot is empty
		PROC_RUNNABLE,			// can be run (or is running)
		PROC_WAITING,			// waiting for message
		PROC_ZOMBIE				// dead but not cleaned up
	} state;

	/* Message queues for synchronization between processes.
	 */
	struct msg_queue mboxes[MSG_NTYPES];

	/* Message buffer for receiving user processes.
	 */
	char *msgbuf;

	/* If the process is waiting for a response, this is the server.
	 */
	gpid_t server;

	/* If the process is waiting, it may have an alarm set.
	 */
	bool_t alarm_set;				// see if an alarm has been set
	unsigned long exptime;			// experiration time


	bool_t interruptable;		// can be interrupted with <ctrl>C

	/* Interrupt information.
	 */
	enum intr_type intr_type;	// type of last interrupt
	void *intr_arg;				// argument to last interrupt

	/* Software page table.
	 */
	struct page_info pages[VIRT_PAGES];

	/* id and header of executable.
	 */
	fid_t executable;
	struct header {
		struct exec_header eh;
		struct exec_segment es[MAX_SEGMENTS];
	} hdr;

	/* Signal stack --- only needed for user processes.
	 */
	char sigstack[SIGNAL_STACK_SIZE];
	address_t sig_sp;			// saved stack pointer into this stack

	/* Kernel stack.
	 */
	char kernelstack[KERNEL_STACK_SIZE];
	address_t kernel_sp;

#ifndef NO_UCONTEXT
	ucontext_t ctx;
#endif

	address_t user_sp;			// user mode stack pointer
};

/* A global variable that identifies the current process.
 */
extern struct process *proc_current;

gpid_t proc_create(gpid_t owner, char *descr, void (*fun)(void *), void *arg);
gpid_t proc_create_uid(gpid_t owner, char *descr, void (*fun)(void *), void *arg, unsigned int uid);
void proc_yield(void);
void proc_kill(gpid_t killer, gpid_t pid, int status);
void proc_to_kernel(void);
void proc_got_interrupt(void);
bool_t proc_send(gpid_t src_pid, gpid_t dst_pid, enum msg_type mtype,
							const void *contents, unsigned int size);
struct process *proc_find(gpid_t pid);
void proc_dump(void);
void proc_initialize(void);
void proc_shutdown(void);
bool_t proc_recv(enum msg_type mtype, unsigned int max_time,
					void *contents, unsigned int *psize, gpid_t *psrc);
bool_t proc_send(gpid_t src_pid, gpid_t dst_pid, enum msg_type mtype,
							const void *contents, unsigned int size);
void proc_pagefault(address_t virt);
void proc_term(struct process *p, int status);
void proc_syscall();

/* copy_user() is a routine used to copy data between kernel and user
 * space.  It is unsave to directly try to access user space data from
 * the kernel proc as the data may not be paged in or mapped.
 */
enum cu_dir { CU_TO_USER, CU_FROM_USER };
void copy_user(char *dst, const char *src, unsigned int size, enum cu_dir dir);

void sigstk_init(struct process *proc);

void shut_down();
