#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include "earth.h"
#include "myalloc.h"
#include "mem.h"
#include "prot.h"

struct mem {
	void *base;					// base of physical frames
	page_no nframes;			// # frames
	unsigned int prot;			// current protection
};
static struct mem mem;

/* Change the protection on the physical memory.
 */
void mem_protect(unsigned int prot){
	if (mprotect(mem.base, mem.nframes * PAGESIZE, prot_cvt(prot)) != 0) {
		perror("mem_protect: mprotect");
	}
	mem.prot = prot;
}

/* Allocate physical memory.
 */
void *mem_initialize(page_no nframes, unsigned int prot){
	void *base = mmap(0, nframes * PAGESIZE, prot_cvt(prot),
						MAP_PRIVATE | MAP_ANON, -1, 0);

	if (base == MAP_FAILED || base == 0) {
		perror("mem_initialize mmap");
		return 0;
	}
	mem.base = base;
	mem.nframes = nframes;
	mem.prot = prot;
	return base;
}
