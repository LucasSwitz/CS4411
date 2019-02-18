#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include "earth.h"
#include "myalloc.h"
#include "tlb.h"
#include "prot.h"

/* An entry in the TLB.  If phys == 0, the entry is unused.
 */
struct tlb_entry {
	page_no virt;					// virtual page number
	void *phys;						// physical address of frame
	unsigned int prot;				// protection bits
};

/* Global but private data.
 */
struct tlb {
	address_t virt_start;			// start of virtual address space
	page_no virt_pages;				// size of virtual address space
	unsigned int nentries;			// size of TLB
	struct tlb_entry *entries;		// array of tlb_entries
};
static struct tlb tlb;

/* Find a TLB mapping by virtual page number.
 */
int tlb_get_entry(page_no virt){
	unsigned int i;
	struct tlb_entry *te;

	for (i = 0, te = tlb.entries; i < tlb.nentries; i++, te++) {
		if (te->virt == virt && te->phys != 0) {
			return i;
		}
	}
	return -1;
}

/* Sync the given entry in the tlb.
 */
static void tlb_sync_entry(struct tlb_entry *te){
	/* Find its virtual address.
	 */
	address_t addr = (address_t) te->virt * PAGESIZE;

	/* If the page is writable, it may have been updated.  Save it.
	 */
	if (te->prot & P_WRITE) {
		if (!(te->prot & P_READ)) {
			mprotect((void *) addr, PAGESIZE, PROT_READ);
		}

		/* TODO.  If stack page do not have to save below stack pointer.
		 */
		memcpy(te->phys, (void *) addr, PAGESIZE);
	}
}

/* Flush the given entry from the TLB.
 */
static void tlb_flush_entry(struct tlb_entry *te){
	/* If not mapped, we're done.
	 */
	if (te->phys == 0) {
		return;
	}

	/* Write page back to frame.
	 */
	tlb_sync_entry(te);

	/* Mark page as inaccessible.
	 */
	address_t addr = (address_t) te->virt * PAGESIZE;
	mprotect((void *) addr, PAGESIZE, PROT_NONE);

	/* Release the entry.
	 */
	te->virt = 0;
	te->phys = 0;
	te->prot = 0;
}

/* Unmap TLB entries corresponding to the given virtual page.
 */
void tlb_unmap(page_no virt){
	unsigned int i;
	struct tlb_entry *te;

	for (i = 0, te = tlb.entries; i < tlb.nentries; i++, te++) {
		if (te->virt == virt) {
			tlb_flush_entry(te);
		}
	}
}

/* Get the content of a TLB entry.
 */
int tlb_get(unsigned int tlb_index, page_no *virt,
							void **phys, unsigned int *prot){
	struct tlb_entry *te;

	if (tlb_index >= tlb.nentries) {
		fprintf(stderr, "tlb_map: non-existing entry\n");
		return 0;
	}
	te = &tlb.entries[tlb_index];

	if (virt != 0) {
		*virt = te->virt;
	}
	if (phys != 0) {
		*phys = te->phys;
	}
	if (prot != 0) {
		*prot = te->prot;
	}
	return 1;
}

/* In the TLB, map the given virtual page to the given physical address.
 */
int tlb_map(unsigned int tlb_index, page_no virt,
									void *phys, unsigned int prot){
	struct tlb_entry *te;

	if (tlb_index >= tlb.nentries) {
		fprintf(stderr, "tlb_map: non-existing entry\n");
		return 0;
	}
	te = &tlb.entries[tlb_index];

	/* Figure out the base address.
	 */
	address_t addr = (address_t) virt * PAGESIZE;

	// printf("tlb_map %"PRIaddr" to %"PRIaddr"\n", addr, (uint64_t) phys);

	/* Check to see if we're just changing the protection.
	 *
	 * TODO.  Optimize this case.
	 */
	if (te->virt == virt && te->phys == phys) {
		// printf("tlb_map: changing protection bits\n");
	}

	/* See if the entry is currently mapped.  If so, flush it.
	 */
	if (te->phys != 0) {
		tlb_flush_entry(te);
	}

	/* Fill the TLB entry.
	 */
	te->virt = virt;
	te->phys = phys;
	te->prot = prot;

	/* Temporarily set write access so we can copy the frame into the
	 * right position.
	 */
	if (!(prot & P_WRITE)) {
		if (mprotect((void *) addr, PAGESIZE, PROT_WRITE) != 0) {
			perror("mprotect 1");
		}
	}
	else {
		if (mprotect((void *) addr, PAGESIZE, prot_cvt(prot)) != 0) {
			perror("mprotect 2");
		}
	}

	// printf("restore %x %"PRIaddr" to %"PRIaddr" %x\n",
	// 			virt, (uint64_t) phys, (uint64_t) addr, prot);
	/* TODO.  If stack do not restore below stack pointer.
	 */
	memcpy((void *) addr, phys, PAGESIZE);

	/* Now set the permissions correctly and we're done.
	 */
	if (!(prot & P_WRITE)) {
		if (mprotect((void *) addr, PAGESIZE, prot_cvt(prot)) != 0) {
			perror("mprotect 3");
		}
	}

	return 1;
}

/* Flush the TLB.
 */
void tlb_flush(void){
	unsigned int i;

	for (i = 0; i < tlb.nentries; i++) {
		tlb_flush_entry(&tlb.entries[i]);
	}
}

/* Sync the TLB: write all modified pages back to the frames.
 */
void tlb_sync(void){
	unsigned int i;

	for (i = 0; i < tlb.nentries; i++) {
		if (tlb.entries[i].phys != 0) {
			tlb_sync_entry(&tlb.entries[i]);
		}
	}
}

/* Set up everything.
 */
void tlb_initialize(unsigned int nentries){
	unsigned int pagesize = getpagesize();

	/* Sanity checks.
	 */
	if (sizeof(address_t) != sizeof(void *)) {
		fprintf(stderr, "address_t needs to be size of pointer\n");
		exit(1);
	}
	if (PAGESIZE % pagesize != 0) {
		fprintf(stderr, "tlb_initialize: PAGESIZE not a multiple of actual page size\n");
		exit(1);
	}
	if (VIRT_BASE % pagesize != 0) {
		fprintf(stderr, "tlb_initialize: VIRT_BASE not a multiple of actual page size\n");
		exit(1);
	}

	tlb.nentries = nentries;
	tlb.entries = (struct tlb_entry *) calloc(nentries, sizeof(*tlb.entries));

	/* Try to map the virtual address range.
	 */
	void *addr = mmap((void *) VIRT_BASE, VIRT_PAGES * PAGESIZE,
			PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
	if (addr != (void *) VIRT_BASE) {
		fprintf(stderr, "Fatal error: can't map virtual address space at %"PRIaddr"\n", VIRT_BASE);
		exit(1);
	}
}
