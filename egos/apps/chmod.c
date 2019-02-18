#include "egos.h"
#include "string.h"
#include "file.h"

static void usage(){
	fprintf(stderr, "Usage: chmod [r-][w-][r-][w-] files...\n");
	exit(1);
}

static int chmod(const char *file, mode_t mode){
	fid_t fid;
	bool_t r = flookup(file, False, 0, &fid);
	if (!r) {
		fprintf(stderr, "file '%s' not found\n", file);
		return 1;
	}
	r = file_chmod(fid.server, fid.ino, mode);
	if (!r) {
		fprintf(stderr, "can't change mode of file '%s'\n", file);
		return 1;
	}
	return 0;
}

int main(int argc, char **argv){
	int i;

	/* First make sure the arguments are valid.
	 */
	if (argc < 3) {
		usage();
	}
	char *mode = argv[1];
	if (strlen(mode) != 4) {
		usage();
	}
	if (mode[0] != 'r' && mode[0] != '-') usage();
	if (mode[1] != 'w' && mode[1] != '-') usage();
	if (mode[2] != 'r' && mode[2] != '-') usage();
	if (mode[3] != 'w' && mode[3] != '-') usage();

	/* Compute the mode bits.
	 */
	mode_t bits = 0;
	if (mode[0] == 'r') bits |= P_FILE_OWNER_READ;
	if (mode[1] == 'w') bits |= P_FILE_OWNER_WRITE;
	if (mode[2] == 'r') bits |= P_FILE_OTHER_READ;
	if (mode[3] == 'w') bits |= P_FILE_OTHER_WRITE;

	int success = 0;
	for (i = 2; i < argc; i++) {
		success += chmod(argv[i], bits);
	}

	return success;
}
