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
#include "../earth/log.h"
#include "../shared/syscall.h"
#include "../shared/queue.h"
#include "../shared/file.h"
#include "../shared/block.h"
#include "../shared/dir.h"
#include "../shared/spawn.h"
#include "../shared/context.h"
#include "exec.h"
//#include "ramfile.h"
#include "process.h"
#include "blocksvr.h"

static bool_t initializing = True;
#ifdef PAGE_TO_FILE
fid_t pgfile;									// file to page to
#else
fid_t pgdev;									// page device
#endif

static address_t kernel_sp;						// kernel stack pointer
static address_t signal_sp;						// signal stack pointer
static char signal_stack[SIGNAL_STACK_SIZE];	// stack for initial pgfault


/* These helper functions are declared here and defined later:
 *   checksum: For debugging purposes.
 *   sigstk_init: Initialize the signal stack of the given process.
 *   intr_handler: Interrupt handler.
 *   proc_mainloop: Loop of the main thread.
 *   file_install: Copy the given file from the underlying operating systems into the file system using the given inode number.
 *   dev_install: Initialize device files
 *   fs_install: Copy files if the underlying disk is a new one (disk.dev didn't exist)
 *   block_test: unit test for block server, only when BLOCK_TEST is defined
 *   file_test: unit test for file server, only when BLOCK_TEST is defined
 */
uint64_t checksum(char *data, unsigned int size);
void sigstk_init(struct process *proc){
	memcpy(proc->sigstack, signal_stack, SIGNAL_STACK_SIZE);
	proc->sig_sp = signal_sp;
}
static void intr_handler(struct intr_context *ic, void *arg);
static void proc_mainloop();
static void file_install(gpid_t ds, fid_t root, char *file_name, unsigned int uid, unsigned int mode);
static void dev_install(struct grass_env *ge, fid_t dev, char *name, enum grass_servers svr);
static void fs_install(struct grass_env *ge, fid_t etc, fid_t bin, fid_t usr, fid_t dev);
#ifdef BLOCK_TEST
static void block_test(struct grass_env *ge);
static void file_test(struct grass_env *ge);
#endif // BLOCK_TEST

/* It all begins here...
 */
int main(int argc, char **argv){
	struct grass_env ge;

	clock_initialize();				// initialize the clock

	setbuf(stdout, NULL);			// simplifies debugging
	log_init();						// for logging

	printf("main: virt base = %"PRIaddr"\n\r", VIRT_BASE);
	printf("main: virt top  = %"PRIaddr"\n\r", VIRT_TOP);

	/* Initialize interrupts.
	 */
	intr_initialize(intr_handler);

	/* Cause a segmentation fault to create an initialized signal stack.
	 */
	printf("main: cause fault\n\r");
	ctx_switch(&kernel_sp, 1);		// 1 is an illegal address!

	/* At this point, the underlying kernel (Linux, MacOSX) thinks we're
	 * executing on the signal stack with clock and I/O interrupts disabled.
	 * We're actually back on the main stack after a context switch.
	 */
	printf("main: back from causing fault\n\r");

	/* Save the signal stack.  This one is going to be reused every time
	 * a new process starts.
	 */
	intr_save(signal_stack, (void *) signal_sp);

	/* Initialize process management.
	 */
	proc_initialize();

	/* Start the periodic timer.
	 */
	clock_start_timer(10);

	printf("main: starting servers\n\r");

	/* Start a bunch of servers.
	 */
	gpid_t tty_init(void);
	ge.servers[GPID_TTY] = tty_init();

	gpid_t disk_init(char *filename, unsigned int nblocks, bool_t sync);
	ge.servers[GPID_DISK] = disk_init("disk.dev", 16 * 1024, False);

	gpid_t block_init(char *type, gpid_t below);
	ge.servers[GPID_BLOCK_PHYS] = block_init("phys", ge.servers[GPID_DISK]);
	ge.servers[GPID_BLOCK_VIRT] = block_init("virt", ge.servers[GPID_BLOCK_PHYS]);

	gpid_t ramfile_init(void);
	ge.servers[GPID_FILE_RAM] = ramfile_init();

	gpid_t blkfile_init(gpid_t);
	ge.servers[GPID_FILE_DISK] = blkfile_init(ge.servers[GPID_BLOCK_VIRT]);

	/* Set the default file server.
	 */
	ge.servers[GPID_FILE] = ge.servers[GPID_FILE_DISK];
//	ge.servers[GPID_FILE] = ge.servers[GPID_FILE_RAM];

	/* code for testing, create, write and read a file
     */
#ifdef BLOCK_TEST
	block_test(&ge);
	return 0;
	file_test(&ge);
	printf("BLOCK and FILE test finished\n");
	return 0;
#endif // BLOCK_TEST

	/* Create the root directory for the directory server.
	 */
	ge.root.server = ge.servers[GPID_FILE];

	gpid_t dir_init(void);
	ge.servers[GPID_DIR] = dir_init();

	/* Try to read inode 1 (file for dir server) and see whether it exists
	 */
	fid_t etc, bin, usr, dev, tmp;
	if (file_exist(ge.servers[GPID_FILE], 1)) {
		// file for dir server already exists
		ge.root.ino = 1;

		bool_t r1 = dir_lookup(ge.servers[GPID_DIR], ge.root, "bin.dir", &bin);
		bool_t r2 = dir_lookup(ge.servers[GPID_DIR], ge.root, "dev.dir", &dev);
		bool_t r3 = dir_lookup(ge.servers[GPID_DIR], ge.root, "etc.dir", &etc);
		bool_t r4 = dir_lookup(ge.servers[GPID_DIR], ge.root, "usr.dir", &usr);

		assert(r1);
		assert(r2);
		assert(r3);
		assert(r4);
	}
	else {
		// file for dir server doesn't exist
		bool_t r = file_create(ge.servers[GPID_FILE], P_FILE_DEFAULT, &ge.root.ino);
		assert(r);
		assert(ge.root.ino == 1);
		printf("Init new disk: installing dir file at inode 1\n\r");

		struct dir_entry de;
		de.fid = ge.root;
		strcpy(de.name, "..dir");
		(void) file_write(ge.root.server, ge.root.ino, 0, &de, DIR_ENTRY_SIZE);
		strcpy(de.name, "...dir");
		(void) file_write(ge.root.server, ge.root.ino, DIR_ENTRY_SIZE,
										&de, DIR_ENTRY_SIZE);

		/* Create some directories.
		 */
		(void) dir_create(ge.servers[GPID_DIR], ge.root, "bin.dir", &bin);
		(void) dir_create(ge.servers[GPID_DIR], ge.root, "dev.dir", &dev);
		(void) dir_create(ge.servers[GPID_DIR], ge.root, "etc.dir", &etc);
		(void) dir_create(ge.servers[GPID_DIR], ge.root, "usr.dir", &usr);

		fs_install(&ge, etc, bin, usr, dev);
	}

	/* Create /tmp on the ram file server.
	 *
	 * TODO.  I suspect this actually barely works right now.  After the
	 *		  first time, there is a /tmp in the root directory, but the
	 *		  ram file server is not persistent.  On the second time,
	 *		  the update of the root directory, on the disk file server
	 *		  probably fails because the entry is already there.  It happens
	 *		  to work out as the tmp directory is re-created using the
	 *		  same inode.
	 */
	(void) dir_create2(ge.servers[GPID_DIR], ge.servers[GPID_FILE_RAM], P_FILE_ALL, ge.root, "tmp.dir", &tmp);


#ifdef HW_PAGING	//<<<<HW_PAGING
#ifdef PAGE_TO_FILE
	/* Create the paging file.
	 */
	pgfile.server = ge.servers[GPID_FILE];

	if (file_exist(ge.servers[GPID_FILE], 2)) {
		// file for paging server already exists
		pgfile.ino = 2;
	} else {
		bool_t r = file_create(ge.servers[GPID_FILE], P_FILE_DEFAULT, &pgfile.ino);
		assert(r);
		assert(pgfile.ino == 2);
	}
#else // !PAGE_TO_FILE
	pgdev.server = ge.servers[GPID_BLOCK_PHYS];
	pgdev.ino = PAGE_PARTITION;
#endif // PAGE_TO_FILE
#endif //>>>>HW_PAGING

	gpid_t spawn_init(void);
	ge.servers[GPID_SPAWN] = spawn_init();


	/* Spawn init.exe.  First find its inode number.
	 */
	fid_t fid;
	bool_t r = dir_lookup(ge.servers[GPID_DIR], etc, "init.exe", &fid);
	assert(r);

	/* Create its initial stack frame.
	 */
	ge.self = sys_getpid();
	ge.cwd = ge.root;
	ge.stdin = fid_val(ge.servers[GPID_TTY], 0);
	ge.stdout = fid_val(ge.servers[GPID_TTY], 1);
	ge.stderr = fid_val(ge.servers[GPID_TTY], 2);
	ge.stdnull = fid_val(ge.servers[GPID_TTY], 3);		// sort of like /dev/null

	/* Create an argument block for init.exe and spawn.
	 */
	gpid_t pid;
	char *args[2], *argb;
	unsigned int size;
	args[0] = "init";
	args[1] = 0;
	(void) spawn_load_args(&ge, 1, args, &argb, &size);
	(void) spawn_exec(ge.servers[GPID_SPAWN], fid, argb, size, False, 0, &pid);
	free(argb);

	printf("main: initialization completed\n\r");

	/* Start the main loop.
	 */
	proc_mainloop();

	return 0;
}


/* For debugging purposes.
 */
uint64_t checksum(char *data, unsigned int size){
	uint64_t sum = 0;

	while (size--) {
		sum = (sum << 1) | ((sum >> 63) & 0x1);		// rotate
		sum ^= *data++;
	}
	return sum;
}

void shut_down(){
	printf("\n\rShutting down\n\r");
	proc_shutdown();
}

/* Interrupt handler.
 */
static void intr_handler(struct intr_context *ic, void *arg){
	/* The very first page fault is for initialization.  To distinguish
	 * illegal accesses to address 1 from initialization, we use the
	 * 'initializing' flag.
	 */
	if (initializing && ic->type == INTR_PAGE_FAULT && arg == (void *) 1) {
		initializing = False;

		/* Switch back to the kernel stack.
		 */
		ctx_switch(&signal_sp, kernel_sp);

		/* We return here everytime a new user process is created.  We're now
		 * running on the signal stack of the process.  Start the user process
		 * by returning from interrupt with the ip (program counter) and stack
		 * pointer updated.
		 */
		ic->ip = (void (*)(void)) proc_current->hdr.eh.eh_start;
		ic->sp = (void *) proc_current->user_sp;
	}

	/* In the normal case, save the interrupt info in the process structure
	 * and switch to its kernel stack, after which the interrupt may be handled.
	 */
	else {
		assert(proc_current->state == PROC_RUNNABLE);
		proc_current->intr_type = ic->type;
		proc_current->intr_arg = arg;
		ctx_switch(&proc_current->sig_sp, proc_current->kernel_sp);
	}
}

/* Loop of the main thread.
 */
static void proc_mainloop(){
	for (;;) {
		struct msg_event mev;

		int size = sys_recv(MSG_EVENT, 0, &mev, sizeof(mev), 0);
		if (size < 0) {
			printf("proc_mainloop: exiting\n");
			return;
		}
		if (size == sizeof(mev) && mev.type == MEV_PROCDIED) {
			printf("Kernel main loop: process %u died\n\r", mev.pid);
		}
		else {
			printf("Kernel main loop: unexpected event\n\r");
		}
	}
}

/* Copy the given file from the underlying operating systems into
 * the file system using the given inode number.
 */
static void file_install(gpid_t ds, fid_t root, char *file_name, unsigned int uid, unsigned int mode){
	/* Allocate a file.
	 */
	unsigned int ino;
	bool_t r = file_create(root.server, P_FILE_DEFAULT, &ino);
	assert(r);
	r = file_chown(root.server, ino, uid);
	assert(r);

	/* Initialize the file.
	 */
	fid_t fid = fid_val(root.server, ino);

#ifdef RAMFILE
	void ramfile_load(fid_t fid, char *file);
	ramfile_load(fid, file_name);
#else
	void blkfile_load(fid_t fid, char *file);
	blkfile_load(fid, file_name);
#endif

	/* Insert into the directory.
	 */
	r = dir_insert(ds, root, file_name, fid);
	assert(r);
}

static void dev_install(struct grass_env *ge, fid_t dev,
								char *name, enum grass_servers svr){
	fid_t fid;
	fid.server = ge->servers[svr];
	fid.ino = 0;
	(void) dir_insert(ge->servers[GPID_DIR], dev, name, fid);
}

static void fs_install(struct grass_env *ge, fid_t etc, fid_t bin, fid_t usr, fid_t dev) {
	/* Install kernel in root directory.
	 */
	file_install(ge->servers[GPID_DIR], ge->root, "a.out", 0, P_FILE_DEFAULT);

	/* Contents of /etc
	 */
	file_install(ge->servers[GPID_DIR], etc, "init.exe", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], etc, "login.exe", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], etc, "passwd", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], etc, "pwdsvr.exe", 0, P_FILE_DEFAULT);

	/* Load the bin directory.
	 */
	file_install(ge->servers[GPID_DIR], bin, "cat.exe", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], bin, "chmod.exe", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], bin, "cp.exe", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], bin, "echo.exe", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], bin, "ed.exe", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], bin, "kill.exe", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], bin, "loop.exe", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], bin, "ls.exe", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], bin, "mkdir.exe", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], bin, "mt.exe", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], bin, "passwd.exe", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], bin, "pwd.exe", 0, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], bin, "shell.exe", 0, P_FILE_DEFAULT);

	/* TODO.  Get rid of these.
	 */
	fid_t guest, rvr, yunhao;
	(void) dir_create(ge->servers[GPID_DIR], usr, "guest.dir", &guest);
	file_chown(guest.server, guest.ino, 666);
	(void) dir_create(ge->servers[GPID_DIR], usr, "rvr.dir", &rvr);
	file_chown(rvr.server, rvr.ino, 10);
	(void) dir_create(ge->servers[GPID_DIR], usr, "yunhao.dir", &yunhao);
	file_chown(yunhao.server, yunhao.ino, 20);
	file_install(ge->servers[GPID_DIR], guest, "README.md", 666, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], guest, "script.bat", 666, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], rvr, "README.md", 10, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], rvr, "script.bat", 10, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], yunhao, "README.md", 20, P_FILE_DEFAULT);
	file_install(ge->servers[GPID_DIR], yunhao, "script.bat", 20, P_FILE_DEFAULT);

	/* Create some "file devices" in the /dev directory.
	 */
	dev_install(ge, dev, "file.dev", GPID_FILE);
	dev_install(ge, dev, "file_disk.dev", GPID_FILE_DISK);
	dev_install(ge, dev, "file_ram.dev", GPID_FILE_RAM);
	dev_install(ge, dev, "tty.dev", GPID_TTY);
}

#ifdef BLOCK_TEST
static void block_test(struct grass_env *ge) {
	/* code for testing, write 3 block first and then read the first 2
     */
	char buffer_write[BLOCK_SIZE * 3];
	memset(buffer_write, 0, sizeof(buffer_write));
	buffer_write[0] = 'Z';
	buffer_write[1] = 'Y';
	buffer_write[2] = 'H';
	buffer_write[3] = '1';
//	block_write(ge->servers[GPID_BLOCK_VIRT], 1, 0, buffer_write);
//	block_write(ge->servers[GPID_BLOCK_VIRT], 1, 1, buffer_write);
//	block_write(ge->servers[GPID_BLOCK_VIRT], 1, 2, buffer_write);


	block_write(ge->servers[GPID_BLOCK_VIRT], 0, 1, buffer_write);
	buffer_write[3] = '2';
	block_write(ge->servers[GPID_BLOCK_VIRT], 0, 2, buffer_write);
	buffer_write[3] = '3';
	block_write(ge->servers[GPID_BLOCK_VIRT], 0, 3, buffer_write);
	buffer_write[3] = '4';
	block_write(ge->servers[GPID_BLOCK_VIRT], 0, 4, buffer_write);


	char buffer_read[BLOCK_SIZE * 2];
	unsigned int psize = 2;
	if (block_read(ge->servers[GPID_BLOCK_VIRT], 0, 4, buffer_read)) {
		printf("!!TESTING: correct, wrote %s, read %s\n", buffer_write, buffer_read);
	} else {
		printf("!!TESTING: wrong\n");
	}
	
	block_getsize(ge->servers[GPID_BLOCK_VIRT], 0, &psize);
	printf("!!TESTING: inode %u has size %u\n", 0, psize);

	//block_setsize(ge->servers[GPID_BLOCK_VIRT], 0, 0);
	block_getsize(ge->servers[GPID_BLOCK_VIRT], 1, &psize);
	printf("!!TESTING: inode %u has size %u\n", 1, psize);
}

static void file_test(struct grass_env *ge) {
	// create file
	fid_t tmp;
	tmp.server = ge->servers[GPID_FILE];
	bool_t r1 = file_create(ge->servers[GPID_FILE], P_FILE_DEFAULT, &tmp.ino);
	assert(r1);
	assert(tmp.ino == 1);

	// write to file
	char buffer_write[BLOCK_SIZE];
	memset(buffer_write, 0, sizeof(buffer_write));
	buffer_write[0] = 'Z';
	buffer_write[1] = 'Y';
	buffer_write[2] = 'H';
	file_write(ge->servers[GPID_FILE], tmp.ino, 0, buffer_write, 5);

	// get file size
	unsigned long fsize;
	file_getsize(ge->servers[GPID_FILE], tmp.ino, &fsize);

	// read the file
	char buffer_read[BLOCK_SIZE];
	memset(buffer_read, 0, sizeof(buffer_read));
	unsigned int psize = 10;
	file_read(ge->servers[GPID_FILE], tmp.ino, 0, buffer_read, &psize);

	printf("!!TESTING: file size %lu bytes, read size %u bytes, file content: %s\n", fsize, psize, buffer_read);
}
#endif  //BLOCK_TEST
