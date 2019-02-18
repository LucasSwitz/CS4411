#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include "egos.h"
#include "string.h"
#include "dir.h"
#include "spawn.h"

/* Execute the given executable by file name and return a process id.
 */
bool_t spawn_fexec(const char *exec, const char *argb, int total, bool_t interruptable,
								unsigned int uid, gpid_t *ppid){
	fid_t dir, fid;

	/* Find the inode of the executable.
	 */
	if (!flookup(exec, False, &dir, &fid)) {
		// fprintf(stderr, "Can't find '%s'\n", exec);
		printf("Can't find '%s'\n", exec);
		return False;
	}

	return spawn_exec(GRASS_ENV->servers[GPID_SPAWN], fid, argb, total,
							interruptable, uid, ppid);
}

bool_t spawn_vexec(const char *exec, int argc, char *const *argv, bool_t interruptable,
												unsigned int uid, gpid_t *ppid){
	char *argb;
	unsigned int size;

	if (!spawn_load_args(GRASS_ENV, argc, argv, &argb, &size)) {
		return False;
	}
	bool_t status = spawn_fexec(exec, argb, size, interruptable, uid, ppid);
	free(argb);
	return status;
}
