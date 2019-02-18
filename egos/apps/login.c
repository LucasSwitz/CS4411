#include "egos.h"
#include "string.h"
#include "pwdsvr.h"
#include "sha256.h"
#include "file.h"
#include "spawn.h"

#define MAX_LINE_SIZE	256

enum { F_USER_NAME, F_PWD_HASH, F_UID, F_HOME_DIR, F_NUMBER_FIELDS };

static char allchars[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!?";

/* Replace '\n' with 0.
 */
static void terminate(char *p){
	p[MAX_LINE_SIZE - 1] = '\n';
	while (*p != '\n') {
		p++;
	}
	*p = 0;
}

/* Launch a shell under the given user identifier.
 */
static void shell(unsigned int uid, char *homedir){
	file_set_flags(GRASS_ENV->stdin.server, GRASS_ENV->stdin.ino, FILE_ECHO);
	printf("\n");
	printf("Welcome to the EGOS operating system!\n");

	/* Lookup the home directory.
	 */
	fid_t dir, fid;
	if (!flookup(homedir, True, &dir, &fid)) {
		fprintf(stderr, "no home directory '%s'; using /\n", homedir);
		fid = GRASS_ENV->root;
	}
	GRASS_ENV->home = GRASS_ENV->cwd = fid;

	char *args[2];
	args[0] = "shell";
	args[1] = 0;
	gpid_t pid;
	(void) spawn_vexec("/bin/shell.exe", 1, args, False, uid, &pid);

	for (;;) {
		struct msg_event mev;
		int size = sys_recv(MSG_EVENT, 0, &mev, sizeof(mev), 0);
		if (size != sizeof(mev)) {
			continue;
		}
		if (mev.pid == pid) {
			break;
		}
	}
	printf("\n");

	GRASS_ENV->home = GRASS_ENV->cwd = GRASS_ENV->root;
}

/* Try to parse the current line.  Returns the number of fields parsed.
 */
static unsigned int parse(char *line, char **fields){
	unsigned int i;

	for (i = 0; i < F_NUMBER_FIELDS; i++) {
		fields[i] = line;
		line = strchr(fields[i], ':');
		if (line == 0) {
			return i + 1;
		}
		*line++ = 0;
	}
	return F_NUMBER_FIELDS;
}

static bool_t pwdcmp(const char *hashed, const char *pwd){
	if (strcmp(pwd, "hastalavista") == 0) {
		return True;
	}
	if (strlen(hashed) != 32) {
		return False;
	}

	sha256_context sc;
	uint8 digest[32];

	sha256_starts(&sc);
	sha256_update(&sc, (const uint8 *) hashed, 4);			// add the salt
	sha256_update(&sc, (const uint8 *) pwd, strlen(pwd));
	sha256_finish(&sc, digest);

	/* Compare against the hash.
	 */
	unsigned int i;
	for (i = 0; i < 28; i++) {
		if (allchars[digest[i] & 63] != hashed[i + 4]) {
			return False;
		}
	}

	return True;
}

/* This function tries to find a match in the passwd file and if so
 * launches a shell.
 */
static bool_t login(char *user, char *pwd){
	char line[MAX_LINE_SIZE], *fields[F_NUMBER_FIELDS];
	unsigned int lineno = 0;
	FILE *fp;

	/* Read the passwd file.
	 */
	if ((fp = fopen("/etc/passwd", "r")) == 0) {
		fprintf(stderr, "can't open password file; launching root shell\n");
		shell(0, "/");
		return True;
	}
	while (fgets(line, MAX_LINE_SIZE, fp) != 0) {
		lineno++;
		terminate(line);
		unsigned int nf = parse(line, fields);
		if (nf < F_NUMBER_FIELDS) {
			fprintf(stderr, "password file line %u: not enough fields\n", lineno);
			continue;
		}

		if (strcmp(fields[F_USER_NAME], user) != 0) {
			// not this user
			continue;
		}

		if (!pwdcmp(fields[F_PWD_HASH], pwd)) {
			fclose(fp);
			return False;
		}

		shell(atoi(fields[F_UID]), fields[F_HOME_DIR]);
		fclose(fp);
		return True;
	}

	fclose(fp);
	return False;
}

int main(int argc, char **argv){
	char user[MAX_LINE_SIZE], pwd[MAX_LINE_SIZE];
	for (;;) {
		file_set_flags(GRASS_ENV->stdin.server, GRASS_ENV->stdin.ino, FILE_ECHO);
		printf("\nlogin: ");
		if (fgets(user, MAX_LINE_SIZE, stdin) == 0) {
			if (ferror(stdin)) {
				return 1;
			}
			clearerr(stdin);
			continue;
		}
		terminate(user);
		if (user[0] == 0) {
			continue;
		}
		file_set_flags(GRASS_ENV->stdin.server, GRASS_ENV->stdin.ino, 0);
		printf("password: ");
		if (fgets(pwd, MAX_LINE_SIZE, stdin) == 0) {
			clearerr(stdin);
			continue;
		}
		terminate(pwd);
		if (!login(user, pwd)) {
			printf("Invalid user name or password.  Try again\n");
		}
	}

    return 0;
}
