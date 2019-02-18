/* This file contains system call code.
 */

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

/* Emulate the sys_recv system call for kernel processes.
 */
int sys_recv(enum msg_type mtype, unsigned int max_time,
								void *msg, unsigned int size, gpid_t *psrc){
	log_p("sys_recv entry pid=%u mtype=%u", proc_current->pid, mtype);
	if (mtype != MSG_REQUEST && mtype != MSG_EVENT) {
		log_p("sys_recv exit pid=%u error=BadMsgType", proc_current->pid);
		return -1;
	}
	gpid_t src;
	bool_t r = proc_recv(mtype, max_time, msg, &size, &src);
	log_p("sys_recv exit pid=%u r=%u", proc_current->pid, r);
	if (r && psrc != 0) {
		*psrc = src;
	}
	return r ? (int) size : -1;
}

/* Emulate the sys_send system call for kernel processes.
 */
int sys_send(gpid_t pid, enum msg_type mtype,
								const void *msg, unsigned int size){
	log_p("sys_send entry src=%u dst=%u mtype=%u", proc_current->pid, pid, mtype);
	if (mtype != MSG_REPLY && mtype != MSG_EVENT) {
		log_p("sys_send exit pid=%u error=BadMsgType", proc_current->pid);
		return -1;
	}
	bool_t r = proc_send(proc_current->pid, pid, mtype, msg, size);
	log_p("sys_send exit pid=%u r=%u", proc_current->pid, r);
	return r ? 0 : -1;
}

/* Emulate the sys_rpc system call for kernel processes.
 */
int sys_rpc(gpid_t pid, const void *request, unsigned int reqsize,
								void *reply, unsigned int repsize){
	log_p("sys_rpc entry src=%u dst=%u", proc_current->pid, pid);
	if (pid == proc_current->pid) {
		log_p("sys_rpc exit pid=%u error=SendSelf", proc_current->pid);
		return -1;
	}
	bool_t r = proc_send(proc_current->pid, pid, MSG_REQUEST, request, reqsize);
	if (!r) {
		log_p("sys_rpc exit pid=%u error=SendFailed", proc_current->pid);
		return -1;
	}
	proc_current->server = pid;
	gpid_t src;
	r = proc_recv(MSG_REPLY, 0, reply, &repsize, &src);
	log_p("sys_rpc exit pid=%u r=%u", proc_current->pid, r);
	if (r) {
		assert(src == pid);
	}
	return r ? (int) repsize : -1;
}

/* Emulate the sys_getpid system call for kernel processes.
 */
gpid_t sys_getpid(void){
	return proc_current->pid;
}

/* Emulate the sys_exit system call for kernel processes.
 */
int sys_exit(int status){
	proc_term(proc_current, status);
	proc_yield();
	assert(0);
	return -1;
}

/* Emulate the sys_gettime system call for kernel processes.
 */
unsigned long sys_gettime(void){
	return clock_now();
}

/* Copy between the user's virtual address space and kernel space.
 */
void copy_user(char *dst, const char *src, unsigned int size,
									enum cu_dir dir){
	/* This is really inefficient but for now it does the job.
	 */
	unsigned int i;
	for (i = 0; i < size; i++) {
		/* First see if the page is mapped already.  If not, simulate
		 * a page fault.
		 */
		address_t virt = (address_t) (dir == CU_FROM_USER ? src : dst);
		if (tlb_get_entry(virt / PAGESIZE) < 0) {
			proc_pagefault(virt);
		}
		*dst++ = *src++;
	}
}

/* Kernel code for the sys_exit() system call.
 */
static void ps_exit(struct syscall *sc){
	sys_exit(sc->u.exit.status);
}

/* Kernel code for the sys_print() system call.  Only used for debugging.
 */
static void ps_print(struct syscall *sc){
	char *buf = malloc(sc->u.print.size);
	copy_user(buf, sc->u.print.str, sc->u.print.size, CU_FROM_USER);
	printf("%.*s", sc->u.print.size, buf);
	fflush(stdout);
	free(buf);
	sc->result = 0;
}

/* Kernel code for the sys_recv() system call.
 */
static void ps_recv(struct syscall *sc){
	unsigned int size = sc->u.recv.size;
	proc_current->msgbuf = malloc(size);
	sc->result = sys_recv(sc->u.recv.mtype, sc->u.recv.max_time,
					proc_current->msgbuf, size, &sc->u.recv.src);
	if (sc->result > 0) {
		copy_user(sc->u.recv.data, proc_current->msgbuf, sc->result, CU_TO_USER);
	}
	free(proc_current->msgbuf);
	proc_current->msgbuf = 0;
}

/* Kernel code for the sys_send() system call.
 */
static void ps_send(struct syscall *sc){
	unsigned int size = sc->u.send.size;
	char *buf = malloc(size);
	copy_user(buf, sc->u.send.data, size, CU_FROM_USER);
	sc->result = sys_send(sc->u.send.pid, sc->u.send.mtype, buf, size);
	free(buf);
}

/* Kernel code for the sys_rpc() system call.
 */
static void ps_rpc(struct syscall *sc){
	/* First copy the request.
	 */
	unsigned int reqsize = sc->u.rpc.reqsize;
	char *request = malloc(reqsize);
	copy_user(request, sc->u.rpc.request, reqsize, CU_FROM_USER);

	/* Allocate reply.
	 */
	unsigned int repsize = sc->u.rpc.repsize;
	char *reply = malloc(repsize);

	/* Do the RPC.
	 */
	sc->result = sys_rpc(sc->u.rpc.pid, request, reqsize, reply, repsize);

	/* Copy the reply.
	 */
	if (sc->result > 0) {
		copy_user(sc->u.rpc.reply, reply, sc->result, CU_TO_USER);
	}

	free(reply);
	free(request);
}

/* Kernel code for the sys_gettime() system call.
 */
static void ps_gettime(struct syscall *sc){
	sc->u.gettime.time = sys_gettime();
	sc->result = 0;
}

/* Kernel entry point for all system calls.
 */
void proc_syscall(){
	struct syscall sc;
	copy_user((char *) &sc, proc_current->intr_arg, sizeof(sc), CU_FROM_USER);

	switch (sc.type) {
	case SYS_EXIT:		ps_exit(&sc);		break;
	case SYS_PRINT:		ps_print(&sc);		break;
	case SYS_RECV:		ps_recv(&sc);		break;
	case SYS_SEND:		ps_send(&sc);		break;
	case SYS_RPC:		ps_rpc(&sc);		break;
	case SYS_GETTIME:	ps_gettime(&sc);	break;
	default:
		assert(0);
	}
	copy_user(proc_current->intr_arg, (char *) &sc, sizeof(sc), CU_TO_USER);
}
