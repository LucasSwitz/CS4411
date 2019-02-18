#include "egos.h"
#include "string.h"
#include "file.h"
#include "dir.h"

void ls(char *name, fid_t fid){
	char *buf = malloc(PAGESIZE);
	unsigned long offset;
	for (offset = 0;;) {
		unsigned int n = PAGESIZE;
		bool_t status = file_read(fid.server, fid.ino, offset, buf, &n);
		if (!status) {
			fprintf(stderr, "ls: error reading '%s'\n", name);
			free(buf);
			return;
		}
		if (n == 0) {
			free(buf);
			return;
		}
		// assert(n % DIR_ENTRY_SIZE == 0);
		unsigned int i;
		for (i = 0; i < n; i += DIR_ENTRY_SIZE, offset += DIR_ENTRY_SIZE) {
			struct dir_entry *de = (struct dir_entry *) &buf[i];

			if (de->name[0] != 0) {
				printf("%u:%u\t", de->fid.server, de->fid.ino);

				struct file_stat stat;
				status = file_stat(de->fid.server, de->fid.ino, &stat);
				if (status) {
					printf("%u\t", (unsigned int) stat.st_size);
					putchar((stat.st_mode & P_FILE_OWNER_READ) ? 'r' : '-');
					putchar((stat.st_mode & P_FILE_OWNER_WRITE) ? 'w' : '-');
					putchar((stat.st_mode & P_FILE_OTHER_READ) ? 'r' : '-');
					putchar((stat.st_mode & P_FILE_OTHER_WRITE) ? 'w' : '-');
				}
				else {
					printf("?\t?");
				}
				de->name[DIR_NAME_SIZE - 1] = 0;
				printf("\t%s\n", de->name);
			}
		}
	}
}

int main(int argc, char **argv){
	int i;
	fid_t fid;

	printf("S:I\tSIZE\tMODE\tNAME\n");
	printf("============================\n");

	if (argc < 2) {
		ls(".", GRASS_ENV->cwd);
		return 0;
	}

	for (i = 1; i < argc; i++) {
		fid_t dir;
		if (!flookup(argv[i], True, &dir, &fid)) {
			printf("ls: unknown directory '%s'\n", argv[i]);
		}
		else {
			ls(argv[i], fid);
		}
	}
	return 0;
}
