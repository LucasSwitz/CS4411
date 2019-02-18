#include "assert.h"
#include "egos.h"
#include "string.h"
#include "dir.h"

static void print_parent(fid_t me){
	/* Look up the file identifier of the parent directory.
	 */
	fid_t dir, parent;
	bool_t r = flookup("..", True, &dir, &parent);
	assert(r);
	if (fid_eq(parent, dir)) {
		return;
	}

	/* Find own entry in the parent directory.
	 */
	FILE *fp = fopen("...dir", "r");
	assert(fp != 0);
	for (;;) {
		struct dir_entry de;
		int sz = fread(&de, sizeof(de), 1, fp);
		if (sz == 0) {
			break;
		}
		assert(sz == 1);
		if (fid_eq(de.fid, me)) {
			GRASS_ENV->cwd = parent;
			print_parent(parent);
			unsigned int n = strnlen(de.name, DIR_NAME_SIZE);
			if (n > 4) {			// strip .dir
				de.name[n - 4] = 0;
			}
			printf("/%s", de.name);
		}
	}
}

int main(int argc, char **argv){
	print_parent(GRASS_ENV->cwd);
	printf("/\n");
	return 0;
}
