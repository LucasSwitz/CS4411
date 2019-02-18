#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <elf.h>
#include "../earth/earth.h"
#include "../grass/exec.h"

/* Parse an ELF format file.
 */
int main(int argc, char **argv){
	if (argc != 3) {
		fprintf(stderr, "Usage: %s macho-executable tulip-executable\n",
									argv[0]);
		return 1;
	}

	/* Open the two files.
	 */
	FILE *input, *output;
	if ((input = fopen(argv[1], "r")) == 0) {
		fprintf(stderr, "%s: can't open macho executable %s\n",
									argv[0], argv[1]);
		return 1;
	}
	if ((output = fopen(argv[2], "w")) == 0) {
		fprintf(stderr, "%s: can't create tulip executable %s\n",
									argv[0], argv[1]);
		return 1;
	}

#ifdef x86_32
	Elf32_Ehdr hdr;
#endif
#ifdef x86_64
	Elf64_Ehdr hdr;
#endif

	int n = fread((char *) &hdr, sizeof(hdr), 1, input);
	if (n != 1) {
		fprintf(stderr, "%s: can't read header in %s\n", argv[0], argv[1]);
		return 1;
	}
	if (hdr.e_ident[EI_MAG0] != ELFMAG0 ||
			hdr.e_ident[EI_MAG1] != ELFMAG1 ||
			hdr.e_ident[EI_MAG2] != ELFMAG2 ||
			hdr.e_ident[EI_MAG3] != ELFMAG3 ||
#ifdef x86_32
			hdr.e_ident[EI_CLASS] != ELFCLASS32 ||
#endif
#ifdef x86_64
			hdr.e_ident[EI_CLASS] != ELFCLASS64 ||
#endif
			hdr.e_type != ET_EXEC) {
		fprintf(stderr, "%s: %s not an executable\n", argv[0], argv[1]);
		return 1;
	}

	/* Output header.
	 */
	struct exec_header eh;
	eh.eh_nsegments = 0;
	eh.eh_base = VIRT_BASE / PAGESIZE;
	eh.eh_offset = 1;
	eh.eh_size = 0;
	eh.eh_start = hdr.e_entry;

	/* Find the program header table.
	 */
	unsigned int inpos = hdr.e_phoff, outpos = sizeof(eh), i, max_size = 0;
#ifdef x86_32
	Elf32_Phdr phdr;
#endif
#ifdef x86_64
	Elf64_Phdr phdr;
#endif
	for (i = 0; i < hdr.e_phnum; i++) {
		fseek(input, inpos, SEEK_SET);
		n = fread((char *) &phdr, sizeof(phdr), 1, input);
		if (n != 1) {
			fprintf(stderr, "%s: can't read program header in %s\n",
												argv[0], argv[1]);
			return 1;
		}
		inpos += sizeof(phdr);

		/* TODO.  Don't really understand the ELF binary.  What to do
		 *		  with DYNAMIC sections?  BSS appears to be in here.
		 */
		if (phdr.p_memsz == 0 || phdr.p_type == PT_DYNAMIC) {
			continue;
		}
		if (phdr.p_vaddr < VIRT_BASE) {
			continue;
		}

		/* Some sanity checks.
		 */
		if (phdr.p_vaddr % PAGESIZE != 0) {
			fprintf(stderr, "%s: segment offset not page aligned\n", argv[0]);
			return 1;
		}
		if (phdr.p_offset % PAGESIZE != 0) {
			fprintf(stderr, "%s: file offset not page aligned\n", argv[0]);
			return 1;
		}
		if (phdr.p_filesz != 0 && phdr.p_filesz > phdr.p_memsz) {
			fprintf(stderr, "%s: bad size %u %u\n", argv[0],
						(unsigned int) phdr.p_filesz,
						(unsigned int) phdr.p_memsz);
			return 1;
		}

		printf("%s: %d type=%d vaddr=%lx memsz=%ld offset=%lx filesz=%ld prot=%x\n", argv[1], i, phdr.p_type, phdr.p_vaddr, phdr.p_memsz, phdr.p_offset, phdr.p_filesz, phdr.p_flags);

		/* Create a segment descriptor.
		 */
		struct exec_segment es;
		es.es_first = phdr.p_vaddr / PAGESIZE;
		es.es_npages = (phdr.p_memsz + PAGESIZE - 1) / PAGESIZE,
		es.es_prot = 0;
		if (phdr.p_flags & PF_R) {
			es.es_prot |= P_READ;
		}
		if (phdr.p_flags & PF_W) {
			es.es_prot |= P_WRITE;
		}
		if (phdr.p_flags & PF_X) {
			es.es_prot |= P_EXEC;
		}

		/* Write the segment descriptor to the output file.
		 */
		fseek(output, outpos, SEEK_SET);
		fwrite((char *) &es, sizeof(es), 1, output);
		outpos += sizeof(es);
		eh.eh_nsegments++;

		/* Copy the segment into the output file.
		 */
		fseek(input, phdr.p_offset, SEEK_SET);
		fseek(output, PAGESIZE + (phdr.p_vaddr - VIRT_BASE), SEEK_SET);
		char buf[PAGESIZE];
		unsigned int total = phdr.p_filesz, size;
		if ((phdr.p_vaddr - VIRT_BASE) + total > max_size) {
			max_size = (phdr.p_vaddr - VIRT_BASE) + total;
		}
		while (total > 0) {
			size = total > PAGESIZE ? PAGESIZE : total;
			if ((n = fread(buf, 1, size, input)) <= 0) {
				fprintf(stderr, "%s: unexpexted EOF in %s\n", argv[0], argv[1]);
				return 1;
			}
			fwrite(buf, 1, n, output);
			total -= n;
		}
	}

	/* Write the output header.
	 */
	fseek(output, 0, SEEK_SET);
	eh.eh_size = (max_size + PAGESIZE - 1) / PAGESIZE;
	fwrite((char *) &eh, sizeof(eh), 1, output);
	fclose(output);
	fclose(input);

	return 0;
}
