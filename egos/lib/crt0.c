#include "egos.h"
#include "string.h"

void _start(void){
	extern int main(int argc, char **argv, char **envp);

	stdio_finit();
	int status = main(GRASS_ENV->argc, GRASS_ENV->argv, GRASS_ENV->envp);
	sys_exit(status);
}
