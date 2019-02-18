#include "egos.h"
#include "string.h"

/* Generate an illegal instruction trap to serve as a system call interrupt.
 */
void sys_trap(struct syscall *sc){
#ifdef x86_32
	asm("movl %0, %%eax" :: "r" (sc));
#endif
	asm(".long 6");
}

void (*sys_entry_point)(struct syscall *sc) = sys_trap;

void sys_invoke(struct syscall *sc){
	(*sys_entry_point)(sc);
}

int sys_exit(int status){
	struct syscall sc;

	sc.type = SYS_EXIT;
	sc.u.exit.status = status;
	sys_invoke(&sc);
	return sc.result;
}

int sys_print(const char *str){
	struct syscall sc;

	sc.type = SYS_PRINT;
	sc.u.print.str = str;
	sc.u.print.size = 0;
	while (*str++ != 0) {
		sc.u.print.size++;
	}
	sys_invoke(&sc);
	return sc.result;
}

int sys_recv(enum msg_type mtype, unsigned int max_time,
								void *msg, unsigned int size, gpid_t *psrc){
	struct syscall sc;

	sc.type = SYS_RECV;
	sc.u.recv.mtype = mtype;
	sc.u.recv.max_time = max_time;
	sc.u.recv.size = size;
	sc.u.recv.data = msg;
	sys_invoke(&sc);
	if (sc.result >= 0 && psrc != 0) {
		*psrc = sc.u.recv.src;
	}
	return sc.result;
}

int sys_send(gpid_t pid, enum msg_type mtype,
								const void *msg, unsigned int size){
	struct syscall sc;

	sc.type = SYS_SEND;
	sc.u.send.pid = pid;
	sc.u.send.mtype = mtype;
	sc.u.send.size = size;
	sc.u.send.data = msg;
	sys_invoke(&sc);
	return sc.result;
}

int sys_rpc(gpid_t pid, const void *request, unsigned int reqsize,
								void *reply, unsigned int repsize){
	struct syscall sc;

	sc.type = SYS_RPC;
	sc.u.rpc.pid = pid;
	sc.u.rpc.reqsize = reqsize;
	sc.u.rpc.request = request;
	sc.u.rpc.repsize = repsize;
	sc.u.rpc.reply = reply;
	sys_invoke(&sc);
	return sc.result;
}

unsigned long sys_gettime(){
	struct syscall sc;

	sc.type = SYS_GETTIME;
	sys_invoke(&sc);
	return sc.u.gettime.time;
}

gpid_t sys_getpid(){
	gpid_t pid = GRASS_ENV->self;
	return pid;
}
