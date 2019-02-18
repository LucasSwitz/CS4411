#include "egos.h"
#include "string.h"

#define BUF_SIZE		4096

int cp(char *from, char *to){
	FILE *f_from, *f_to;

	if ((f_from = fopen(from, "r")) == 0) {
		fprintf(stderr, "cp: can't read %s\n", from);
		return 1;
	}

	if ((f_to = fopen(to, "w")) == 0) {
		fprintf(stderr, "cp: can't create %s\n", to);
		return 1;
	}


	char buf[BUF_SIZE];
	int n;
	while ((n = fread(buf, 1, BUF_SIZE, f_from)) > 0) {
		int m = fwrite(buf, 1, n, f_to);
		if (m != n) {
			fprintf(stderr, "cp: copying error %d %d\n", n, m);
			break;
		}
	}
	fclose(f_from);
	fclose(f_to);

	return 0;
}

int main(int argc, char **argv){
	if (argc < 3) {
		printf("Usage: cp from to\n");
		return 1;
	}
	return cp(argv[1], argv[2]);
}
