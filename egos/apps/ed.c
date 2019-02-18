#include "egos.h"
#include "string.h"

#define MAX_LINE_SIZE	4096

char line[MAX_LINE_SIZE];
char **lines;
int nlines, curline;
bool_t modified;

static void print(char **lines, int start, int end){
	int line;

	if (curline <= 0) {
		printf("prior to start of file\n");
		return;
	}
	if (curline > nlines) {
		printf("at end of file\n");
		return;
	}
	if (start >= end) {
		printf("no lines to print\n");
		return;
	}
	if (end > nlines) {
		end = nlines;
	}
	for (line = start; line < end; line++) {
		printf("%d	%s", line + 1, lines[line]);
	}
}

void insert(char *line){ 
	int len = strlen(line);
	char *copy = malloc(len + 1);
	strcpy(copy, line);
	lines = realloc(lines, (nlines + 1) * sizeof(char *));
	nlines++;
	memmove(&lines[curline], &lines[curline - 1], (nlines - curline) * sizeof(char *));
	lines[curline - 1] = copy;
}

void save(char *file){
	FILE *output;
	int i;

	if (!modified) {
		printf("no changes\n");
		return;
	}
	if ((output = fopen(file, "w")) == 0) {
		fprintf(stderr, "save: can't open file %s for writing\n", file);
		return;
	}
	for (i = 0; i < nlines; i++) {
		fputs(lines[i], output);
	}
	fclose(output);
	modified = False;
}

int main(int argc, char **argv){
	FILE *fp;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s file\n", argv[0]);
		return 1;
	}

	/* Read the file.
	 */
	if ((fp = fopen(argv[1], "r")) == 0) {
		printf("can't open %s\n", argv[1]);
		modified = True;
	}
	else {
		while (fgets(line, MAX_LINE_SIZE, fp) != 0) {
			curline++;
			insert(line);
		}
		fclose(fp);
	}

	/* Read commands.
	 */
	curline = 0;
    for (;;) {
		printf("-> "); fflush(stdout);
		if (fgets(line, MAX_LINE_SIZE, stdin) == 0) {
			break;
		}
		if (strcmp(line, "p\n") == 0) {
			print(lines, curline - 1, curline);
		}
		else if (strcmp(line, "z\n") == 0) {
			print(lines, curline - 1, curline + 9);
			curline += 10;
			if (curline > nlines + 1) {
				curline = nlines + 1;
			}
		}
		else if (strcmp(line, "\n") == 0) {
			if (curline <= nlines) {
				curline++;
			}
			print(lines, curline - 1, curline);
		}
		else if (strcmp(line, "P\n") == 0) {
			curline = nlines;
			print(lines, 0, nlines);
		}
		else if (strcmp(line, "-\n") == 0) {
			if (curline > 0) {
				curline = curline - 1;
			}
			print(lines, curline - 1, curline);
		}
		else if (strcmp(line, "$\n") == 0) {
			curline = nlines;
			print(lines, curline - 1, curline);
		}
		else if (strcmp(line, "d\n") == 0) {
			if (curline <= 0 || curline > nlines) {
				printf("can't delete outside file\n");
			}
			else {
				free(lines[curline - 1]);
				memmove(&lines[curline - 1], &lines[curline],
								(nlines - curline) * sizeof(char *));
				nlines--;
				modified = True;
			}
		}
		else if (strcmp(line, "i\n") == 0) {
			if (curline <= 0) {
				curline = 1;
			}
			while (fgets(line, MAX_LINE_SIZE, stdin) != 0) {
				insert(line);
				curline++;
				modified = True;
			}
			clearerr(stdin);
		}
		else if (strcmp(line, "c\n") == 0) {
			if (curline <= 0 || curline > nlines) {
				printf("can't change outside file\n");
			}
			else {
				free(lines[curline - 1]);
				memmove(&lines[curline - 1], &lines[curline],
								(nlines - curline) * sizeof(char *));
				nlines--;
				while (fgets(line, MAX_LINE_SIZE, stdin) != 0) {
					insert(line);
					curline++;
					modified = True;
				}
				clearerr(stdin);
			}
		}
		else if (strcmp(line, "a\n") == 0) {
			if (curline > nlines) {
				curline--;
			}
			while (fgets(line, MAX_LINE_SIZE, stdin) != 0) {
				curline++;
				insert(line);
				modified = True;
			}
			clearerr(stdin);
		}
		else if (strcmp(line, "w\n") == 0) {
			save(argv[1]);
		}
		else if (strcmp(line, "q\n") == 0) {
			if (modified) {
				fprintf(stderr, "save first, or use Q\n");
			}
			else {
				break;
			}
		}
		else if (strcmp(line, "Q\n") == 0) {
			break;
		}
		else if ('0' <= *line && *line <= '9') {
			curline = atoi(line);
			print(lines, curline - 1, curline);
		}
		else {
			printf("unknown command\n");
		}
	}

	fclose(fp);

    return 0;
}
