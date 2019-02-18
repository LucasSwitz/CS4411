#include "egos.h"
#include "string.h"
#include "spawn.h"

/* This is the 'init' process---the very first process that is started
 * by the kernel itself.  Currently, it launches a shell and then it
 * just waits.
 */
int main(int argc, char **argv){
	printf("INIT: pid = %u\n", sys_getpid());

	/* Start the password server.
	 */
	char *args[2];
	gpid_t pid;

	args[0] = "pwdsvr";
	args[1] = 0;
	(void) spawn_vexec("/etc/pwdsvr.exe", 1, args, False, 0, &GRASS_ENV->servers[GPID_PWD]);

	/* Start the login process.
	 */
	args[0] = "login";
	args[1] = 0;
	(void) spawn_vexec("/etc/login.exe", 1, args, False, 0, &pid);

	for (;;) {
		struct msg_event mev;
		int size = sys_recv(MSG_EVENT, 0, &mev, sizeof(mev), 0);
		if (size < 0) {
			printf("init: terminating\n");
			return 1;
		}
		if (size != sizeof(mev)) {
			continue;
		}
		printf("init: process %u died\n", mev.pid);
	}
}
