/* This file contains much of the interface of processes to the kernel.
 * It contains basic types such a process identifier (gpid_t), file
 * identifier (fid_t), message types, as well as system call interfaces.
 * All these can be used both by kernel and user processes.
 */

/* Process identifier.
 */
typedef unsigned int gpid_t;		// already define in types.h

/* A file is identified by the server that stores it and an inode number
 * local to that server.
 */
typedef struct file_id {
	gpid_t server;
	unsigned int ino;
} fid_t;

#define fid_val(s, i)		((fid_t) { .server = (s), .ino = (i) })
#define fid_eq(f1, f2)		((f1).server == (f2).server && (f1).ino == (f2).ino)

/* Special status codes for unnaturally dying processes.
 */
#define STAT_ILLMEM		(-1)	// illegal memory access
#define STAT_KILL		(-2)	// process purposely killed
#define STAT_INTR		(-3)	// <ctrl>C
#define STAT_SHUTDOWN	(-4)	// shutdown

/* Currently only used for notifying process deaths.
 */
struct msg_event {
	enum { MEV_UNUSED, MEV_PROCDIED } type;
	gpid_t pid;
	int status;				// status when process exited (negative is special)
};

/* Types of messages.  Currently there are 3 (MSG_NTYPES is there only to
 * count them).
 */
enum msg_type {
	MSG_REQUEST,			// request message
	MSG_REPLY,				// reply message
	MSG_EVENT,				// event message --- see struct msg_event
	MSG_NTYPES				// # msg types
};

enum grass_servers {
	GPID_UNUSED,			// to simplify debugging

	/* Default servers.
	 */
	GPID_TTY,				// terminal server
	GPID_DISK,				// disk server
	GPID_FILE,				// file server
	GPID_DIR,				// directory server
	GPID_SPAWN,				// spawn server
	GPID_PWD,				// password server (runs in user space)

	/* Specific servers.
	 */
	GPID_FILE_RAM,			// ramfile server
	GPID_FILE_DISK,			// diskfile server
	GPID_BLOCK_PHYS,		// physical block server
	GPID_BLOCK_VIRT,		// virtual block server

	GPID_NSERVERS			// #servers
};

/* Environment for a user process.
 */
struct grass_env {
	/* Process id of this process.
	 */
	gpid_t self;

	/* Process ids of server processes that a process might need to contact.
	 */
	gpid_t servers[GPID_NSERVERS];

	/* Arguments and environment.
	 */
	int argc;
	char **argv, **envp;

	/* Root, current, and home directory.
	 */
	fid_t root, cwd, home;

	/* Standard input/output/error.
	 */
	fid_t stdin, stdout, stderr, stdnull;
};

/* This points to the top of the (paged) stack of a user process where
 * the struct grass_env is kept.
 */
#define GRASS_ENV	((struct grass_env *) (VIRT_TOP - 8 - sizeof(struct grass_env)))

/* System call types.
 */
enum syscall_type {
	SYS_UNUSED,				// helps with debugging
	SYS_EXIT,
	SYS_PRINT,				// mostly for debugging
	SYS_RECV,
	SYS_SEND,
	SYS_RPC,
	SYS_GETTIME,
	SYS_NCALLS
};

struct sys_exit {
	/* IN */		int status;
};

struct sys_print {
	/* IN */		const char *str;
	/* IN */		unsigned int size;
};

struct sys_recv {
	/* OUT */		enum msg_type mtype;
	/* OUT */		unsigned int max_time;
	/* IN/OUT */	unsigned int size;
	/* IN */		char *data;
	/* IN */		gpid_t src;
};

struct sys_send {
	/* OUT */		gpid_t pid;
	/* OUT */		enum msg_type mtype;
	/* OUT */		unsigned int size;
	/* OUT */		const char *data;
};

struct sys_rpc {
	/* OUT */		gpid_t pid;
	/* OUT */		unsigned int reqsize;
	/* OUT */		const char *request;
	/* IN/OUT */	unsigned int repsize;
	/* IN */		char *reply;
};

struct sys_gettime {
	/* IN */		unsigned long time;
};

struct syscall {
	int result;
	enum syscall_type type;
	union {
		struct sys_exit exit;
		struct sys_print print;
		struct sys_recv recv;
		struct sys_send send;
		struct sys_rpc rpc;
		struct sys_gettime gettime;
	} u;
};

/* All the system calls are here.
 */
int sys_exit(int status);
int sys_print(const char *str);
int sys_recv(enum msg_type mtype, unsigned int max_time,
								void *msg, unsigned int size, gpid_t *src);
int sys_send(gpid_t pid, enum msg_type mtype,
								const void *msg, unsigned int size);
int sys_rpc(gpid_t pid, const void *request, unsigned int reqsize,
								void *reply, unsigned int repsize);
unsigned long sys_gettime(void);

/* Not really a system call, but convenient.
 */
gpid_t sys_getpid(void);
