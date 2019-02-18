#include "egos.h"
#include "string.h"

#define BUF_SIZE		1024

void copy(FILE *fp){
	char buf[BUF_SIZE];

	for (;;) {
		int n = fread(buf, 1, BUF_SIZE, fp);
		if (n < 0) {
			sys_exit(1);
		}
		if (n == 0) {
			break;
		}
		(void) fwrite(buf, 1, n, stdout);
	}
}

bool_t cat(char *file){
	FILE *fp;

	if (strcmp(file, "-") == 0) {
		copy(stdin);
		return True;
	}
	if ((fp = fopen(file, "r")) != 0) {
		copy(fp);
		fclose(fp);
		return True;
	}
	else {
		printf("cat: can't open '%s'\n", file);
		return False;
	}
}

int main(int argc, char **argv){
	int i;
	bool_t status = True;

	if (argc < 2) {
		copy(stdin);
	}
	for (i = 1; i < argc; i++) {
		if (!cat(argv[i])) {
			status = False;
		}
	}
	return status ? 0 : 1;
}
