#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <mach-o/loader.h>
#include "../earth/earth.h"
#include "../grass/exec.h"

/* Parse a Mach-O format file.
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

	union {
		struct load_command lc;
		struct segment_command_64 sc;
	} u;
	struct section_64 se;

	/* Read the macho header.
	 */
	struct mach_header_64 hdr;
	int n = fread((char *) &hdr, sizeof(hdr), 1, input);
	if (n != 1) {
		fprintf(stderr, "%s: can't read header in %s\n", argv[0], argv[1]);
		return 1;
	}
	if (hdr.magic != MH_MAGIC_64 || hdr.filetype != MH_EXECUTE) {
		fprintf(stderr, "%s: %s not a 64bit executable\n", argv[0], argv[1]);
		return 1;
	}

	/* Output header.
	 */
	struct exec_header eh;
	eh.eh_nsegments = 0;
	eh.eh_base = VIRT_BASE / PAGESIZE;
	eh.eh_offset = 1;
	eh.eh_size = 0;
	eh.eh_start = VIRT_BASE;

	/* Read the "commands".
	 */
	fpos_t inpos = sizeof(hdr), outpos = sizeof(eh);
	unsigned int max_size = 0;
	int i, j;
	for (i = 0; i < hdr.ncmds; i++) {
		/* Seek to the next command and read the command header.
		 */
		fseek(input, inpos, SEEK_SET);
		n = fread((char *) &u.lc, sizeof(u.lc), 1, input);
		if (n != 1) {
			fprintf(stderr, "%s: bad command in %s\n", argv[0], argv[1]);
			return 1;
		}
		inpos += sizeof(u.lc);

		/* TODO.  Extract start address and stack size??
		 */
		if (u.lc.cmd == LC_MAIN) {
			fprintf(stderr, "LC_MAIN\n");
			continue;
		}

		/* Skip other stuff except segments.
		 */
		if (u.lc.cmd != LC_SEGMENT_64) {
			inpos += u.lc.cmdsize - sizeof(u.lc);
			continue;
		}

		/* Read the remainder of the command.
		 */
		n = fread((char *) &u.sc + sizeof(u.lc),
									sizeof(u.sc) - sizeof(u.lc), 1, input);
		if (n != 1) {
			fprintf(stderr, "%s: bad segment in %s\n", argv[0], argv[1]);
			return 1;
		}
		inpos += sizeof(u.sc) - sizeof(u.lc);
		if (u.sc.nsects == 0) {
			continue;
		}

		printf("%s: %llx %llx %llx %llx %x %d\n", argv[1], u.sc.vmaddr, u.sc.vmsize, u.sc.fileoff, u.sc.filesize, u.sc.initprot, (int) outpos);

		/* Some sanity checks.
		 */
		if (u.sc.vmaddr < VIRT_BASE) {
			fprintf(stderr, "%s: segment offset too small\n", argv[0]);
			return 1;
		}
		if (u.sc.vmaddr % PAGESIZE != 0) {
			fprintf(stderr, "%s: segment offset not page aligned\n", argv[0]);
			return 1;
		}
		if (u.sc.vmsize % PAGESIZE != 0) {
			fprintf(stderr, "%s: segment size not multiple of pages\n", argv[0]);
			return 1;
		}
		if (u.sc.fileoff % PAGESIZE != 0) {
			fprintf(stderr, "%s: file offset not page aligned\n", argv[0]);
			return 1;
		}
		if (u.sc.fileoff % PAGESIZE != 0) {
			fprintf(stderr, "%s: file offset not page aligned\n", argv[0]);
			return 1;
		}
		if (u.sc.filesize != u.sc.vmsize) {
			fprintf(stderr, "%s: unequal size %lld %lld\n", argv[0],
										u.sc.filesize, u.sc.vmsize);
			// return 1;
		}

		/* Create a segment descriptor.
		 */
		struct exec_segment es;
		es.es_first = u.sc.vmaddr / PAGESIZE;
		es.es_npages = u.sc.vmsize / PAGESIZE,
		es.es_prot = 0;
		if (u.sc.initprot & PROT_READ) {
			es.es_prot |= P_READ;
		}
		if (u.sc.initprot & PROT_WRITE) {
			es.es_prot |= P_WRITE;
		}
		if (u.sc.initprot & PROT_EXEC) {
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
		fseek(input, u.sc.fileoff, SEEK_SET);
		fseek(output, PAGESIZE + (u.sc.vmaddr - VIRT_BASE), SEEK_SET);
		char buf[PAGESIZE];
		unsigned int total = u.sc.filesize, size;
		if ((u.sc.vmaddr - VIRT_BASE) + total > max_size) {
			max_size = (u.sc.vmaddr - VIRT_BASE) + total;
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

		/* See if the start address is in one of the sections.
		 */
		fseek(input, inpos, SEEK_SET);
		for (j = 0; j < u.sc.nsects; j++) {
			n = fread((char *) &se, sizeof(se), 1, input);
			if (n != 1) {
				fprintf(stderr, "%s: bad section in %s\n", argv[0], argv[1]);
				return 1;
			}
			inpos += sizeof(se);
			if (strcmp(se.sectname, "__text") == 0) {
				eh.eh_start = se.addr;
			}
		}
	}

	/* Write the output header.
	 */
	fseek(output, 0, SEEK_SET);
	eh.eh_size = max_size / PAGESIZE;
	fwrite((char *) &eh, sizeof(eh), 1, output);
	fclose(output);
	fclose(input);

	return 0;
}
