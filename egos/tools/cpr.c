#include <stdio.h>
#include <string.h>

#define MAX_LINE_SIZE	1024

int main(int argc, char **argv){
	if (argc < 3) {
		fprintf(stderr, "Usage: %s src dst [keys ...]\n", argv[0]);
		return 1;
	}

	FILE *src = fopen(argv[1], "r");
	if (src == 0) {
		fprintf(stderr, "%s: can't open %s\n", argv[0], argv[1]);
		return 1;
	}
	FILE *dst = fopen(argv[2], "w");
	if (dst == 0) {
		fprintf(stderr, "%s: can't create %s\n", argv[0], argv[2]);
		return 1;
	}

	char buf[MAX_LINE_SIZE], key[MAX_LINE_SIZE];
	int remove_key = 0;
	for (;;) {
		char *line = fgets(buf, MAX_LINE_SIZE, src);
		if (line == 0) {
			break;
		}
		if (remove_key > 0) {
			/* See if this is the last line to remove.
			 */
			if (strstr(line, key)) {
				remove_key = 0;
			}
		}
		else {
			/* See if we should remove this line.
			 */
			int i;
			for (i = 3; i < argc; i++) {
				sprintf(key, "//<><>%s", argv[i]);
				if (strstr(line, key)) {
					break;
				}
			}
			if (i < argc) {
				continue;
			}

			/* See if this is the first line to remove of several.
			 */
			for (i = 3; i < argc; i++) {
				sprintf(key, "//<<<<%s", argv[i]);
				if (strstr(line, key)) {
					sprintf(key, "//>>>>%s", argv[i]);
					remove_key = i;
					break;
				}
			}
			if (remove_key == 0) {
				fputs(line, dst);
			}
		}
	}
	fclose(src);
	fclose(dst);
	return 0;
}
