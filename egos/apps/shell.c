#include "ctype.h"
#include "egos.h"
#include "string.h"
#include "spawn.h"

#define BUF_SIZE		4096
#define MAX_ARGS		 256

struct proc {
	struct proc *next;
	gpid_t pid;
} *procs;

/* Try various path names for the given executable name.
 */
bool_t path_exec(char *exec, int argc, char **argv, bool_t background, gpid_t *ppid){
	char *argb;
	unsigned int total;

	/* In the background, don't read from the terminal.
	 */
	if (background) {
		fid_t stdin = GRASS_ENV->stdin;
		GRASS_ENV->stdin = GRASS_ENV->stdnull;
		spawn_load_args(GRASS_ENV, argc, argv, &argb, &total);
		GRASS_ENV->stdin = stdin;
	}
	else {
		spawn_load_args(GRASS_ENV, argc, argv, &argb, &total);
	}

	char *path;
	asprintf(&path, "/bin/%s.exe", exec);
	bool_t r = spawn_fexec(path, argb, total, !background, 0, ppid);
	free(path);

	free(argb);
	return r;
}

/* Wait for the given process id, which may be 0 if waiting for all.
 */
static void wait(gpid_t pid, bool_t interactive){
	struct proc *proc;

	/* First make sure the pid is one of my processes.
	 */
	if (pid != 0) {
		for (proc = procs; proc != 0; proc = proc->next) {
			if (proc->pid == pid) {
				break;
			}
		}
		if (proc == 0) {
			printf("shell wait: no process %u\n", pid);
			return;
		}
	}

	while (pid != 0 || procs != 0) {
		struct msg_event mev;
		int size = sys_recv(MSG_EVENT, 0, &mev, sizeof(mev), 0);
		if (size != sizeof(mev)) {
			continue;
		}
		if (mev.pid != pid || mev.status != 0) {
			if (interactive) {
				printf("shell: process %u terminated with status %d\n", mev.pid, mev.status);
			}
		}

		/* Remove from list of processes.
		 */
		struct proc **gp;
		for (gp = &procs; (proc = *gp) != 0; gp = &proc->next) {
			if (proc->pid == mev.pid) {
				*gp = proc->next;
				free(proc);
				break;
			}
		}

		if (mev.pid == pid) {
			break;
		}
	}
}

void parse(char *cmdline, int argc, char **argv, bool_t interactive){
	char *new_argv[MAX_ARGS + 1];
	int new_argc = 0;
	bool_t background = False;

	/* Split command line into tokens.
	 */
	for (;;) {
		/* Skip blanks.
		 */
		while (isblank(*cmdline)) {
			cmdline++;
		}

		/* See if we're at the end of a line.
		 */
		if (*cmdline == 0 || *cmdline == '\n' || *cmdline == '#') {
			break;
		}
		if (*cmdline == '&') {
			background = True;
			break;
		}

		if (new_argc == MAX_ARGS) {
			fprintf(stderr, "shell: too many arguments\n");
			return;
		}
		new_argv[new_argc++] = cmdline;

		/* Find the end of the command.
		 */
		while (*cmdline != 0 && *cmdline != '\n' && *cmdline != '#' && *cmdline != '&' && !isblank(*cmdline)) {
			cmdline++;
		}
		if (*cmdline == 0) {
			break;
		}
		if (*cmdline == '\n' || *cmdline == '#') {
			*cmdline++ = 0;
			break;
		}
		if (*cmdline == '&') {
			background = True;
			*cmdline++ = 0;
			break;
		}
		*cmdline++ = 0;
	}

	/* Check for empty line.
	 */
	if (new_argc == 0) {
		return;
	}

	/* Macro expansion.
	 */
	int j;
	for (j = 0; j < new_argc; j++) {
		char *s = new_argv[j];    // , *e;

		if (s[0] != '$') {
			continue;
		}
		if (isdigit(s[1]) && s[2] == 0) {
			int x = s[1] - '0';
			new_argv[j] = x < argc ? argv[x] : "";
		}
//		else if ((e = getenv(&s[1])) != 0) {
//			new_argv[j] = e;
//		}
		else {
			new_argv[j] = "";
		}
	}

	/* Test for built-in 'exit' command.
	 */
	if (strcmp(new_argv[0], "exit") == 0) {
		sys_exit(new_argc == 1 ? 0 : atoi(new_argv[1]));
	}

	if (strcmp(new_argv[0], "wait") == 0) {
		wait(new_argc == 1 ? 0 : atoi(new_argv[1]), interactive);
		return;
	}

	/* Test for built-in 'cd' (change directory) command.
	 */
	if (strcmp(new_argv[0], "cd") == 0) {
		/* If there is no argument, go back to the home directory.
		 */
		if (new_argc == 1) {
			GRASS_ENV->cwd = GRASS_ENV->home;
		}
		else {
			fid_t dir, fid;
			if (!flookup(new_argv[1], True, &dir, &fid)) {
				fprintf(stderr, "shell: bad directory: '%s'\n", new_argv[1]);
			}
			else {
				GRASS_ENV->cwd = fid;
			}
		}
		return;
	}

	gpid_t client_pid;
	if (!path_exec(new_argv[0], new_argc, new_argv, background, &client_pid)) {
		return;
	}

	/* Add pid to the list of running processes.
	 */
	struct proc *proc = calloc(1, sizeof(*proc));
	proc->pid = client_pid;
	proc->next = procs;
	procs = proc;

	if (background) {
		if (interactive) {
			printf("shell: started process %u in the background\n",
											(unsigned int) client_pid);
		}
	}
	else {
		wait(client_pid, interactive);
	}
}

int main(int argc, char **argv){
	FILE *input = stdin;
	char buf[BUF_SIZE];
	bool_t interactive = True;
	gpid_t pid = sys_getpid();

	if (argc > 1) {
		if ((input = fopen(argv[1], "r")) == 0) {
			fprintf(stderr, "%s: can't open %s\n", argv[0], argv[1]);
			return 1;
		}
		interactive = False;
	}

	for (;;) {
		if (interactive) {
			printf("%u$ ", (unsigned int) pid);
		}
		if (fgets(buf, BUF_SIZE, input) == 0) {
			break;
		}
		parse(buf, argc - 1, &argv[1], interactive);
	}

	return 0;
}
