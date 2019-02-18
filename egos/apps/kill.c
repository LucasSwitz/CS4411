#include "egos.h"
#include "string.h"
#include "spawn.h"

static void kill(gpid_t pid){
	spawn_kill(GRASS_ENV->servers[GPID_SPAWN], pid, STAT_KILL);
}

int main(int argc, char **argv){
	int i;

	for (i = 1; i < argc; i++) {
		kill((gpid_t) atoi(argv[i]));
	}
	return 0;
}
