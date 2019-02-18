#include "assert.h"
#include "egos.h"
#include "string.h"
#include "file.h"
#include "pwdsvr.h"
#include "sha256.h"

#define MAX_LINE_SIZE	256

/* Replace '\n' with 0.
 */
static void terminate(char *p){
	p[MAX_LINE_SIZE - 1] = '\n';
	while (*p != '\n') {
		p++;
	}
	*p = 0;
} 

/* Change the password for the given user.
 */
bool_t pwdsvr_change(gpid_t svr, char *uname, char *opwd, char *npwd){
	unsigned int uname_size = strlen(uname) + 1;
	unsigned int opwd_size = strlen(opwd) + 1;
	unsigned int npwd_size = strlen(npwd) + 1;

	struct pwdsvr_request *req;
	unsigned int total = sizeof(*req) + uname_size + opwd_size + npwd_size;
	req = malloc(total);
	memset(req, 0, sizeof(*req));
	req->uname_size = uname_size;
	req->opwd_size = opwd_size;
	req->npwd_size = npwd_size;
	char *p = (char *) &req[1];
	strcpy(p, uname); p += uname_size;
	strcpy(p, opwd); p += opwd_size;
	strcpy(p, npwd); p += npwd_size;
	assert(p - (char *) req == total);

	/* Do the RPC.
	 */
	struct pwdsvr_reply reply;
	int r = sys_rpc(svr, req, total, &reply, sizeof(reply));
	free(req);
	return r == sizeof(reply) && reply.status == PWDSVR_OK;
}

static void change_pwd(char *uname, char *opwd, char *npwd){
	bool_t r = pwdsvr_change(GRASS_ENV->servers[GPID_PWD], uname, opwd, npwd);
	if (r) {
		printf("Password updated\n");
	}
	else {
		fprintf(stderr, "passwd: failure.  Password unchanged\n");
		exit(1);
	}
}

int main(int argc, char **argv){
	char user[MAX_LINE_SIZE], pwd[3][MAX_LINE_SIZE];

	file_set_flags(GRASS_ENV->stdin.server, GRASS_ENV->stdin.ino, FILE_ECHO);
	printf("login: ");
	if (fgets(user, MAX_LINE_SIZE, stdin) == 0) {
		return 1;
	}
	file_set_flags(GRASS_ENV->stdin.server, GRASS_ENV->stdin.ino, 0);
	printf("old password: ");
	if (fgets(pwd[0], MAX_LINE_SIZE, stdin) == 0) {
		file_set_flags(GRASS_ENV->stdin.server, GRASS_ENV->stdin.ino, FILE_ECHO);
		return 1;
	}
	printf("\nnew password: ");
	if (fgets(pwd[1], MAX_LINE_SIZE, stdin) == 0) {
		file_set_flags(GRASS_ENV->stdin.server, GRASS_ENV->stdin.ino, FILE_ECHO);
		return 1;
	}
	printf("\nretype password: ");
	if (fgets(pwd[2], MAX_LINE_SIZE, stdin) == 0) {
		file_set_flags(GRASS_ENV->stdin.server, GRASS_ENV->stdin.ino, FILE_ECHO);
		return 1;
	}
	file_set_flags(GRASS_ENV->stdin.server, GRASS_ENV->stdin.ino, FILE_ECHO);
	printf("\n");
	terminate(user);
	terminate(pwd[0]);
	terminate(pwd[1]);
	terminate(pwd[2]);

	if (strcmp(pwd[1], pwd[2]) != 0) {
		fprintf(stderr, "New passwords don't match\n");
		return 1;
	}

	change_pwd(user, pwd[0], pwd[1]);

    return 0;
}
