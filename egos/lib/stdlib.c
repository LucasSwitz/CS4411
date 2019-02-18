#include <stdarg.h>
#include <inttypes.h>
#include "egos.h"
#include "string.h"
#include "ctype.h"

int abs(int i){
	return i < 0 ? -i : i;
}

long labs(long i){
	return i < 0 ? -i : i;
}

#ifdef notdef
char *getenv(const char *name){
	extern char **environ;
	int i;
	const char *e, *p;

	if (environ == 0) {
		return 0;
	}
	for (i = 0; (e = environ[i]) != 0; i++) {
		p = name;
		while (*p == *e) {
			if (*p == 0) {
				break;
			}
			p++, e++;
		}
		if (*p == 0 && *e == '=') {
			return (char *) e + 1;
		}
	}
	return 0;
}
#endif

long strtol(const char *restrict str, char **restrict endptr, int base){
	long sign = 1, n = 0;

	while (isspace(*str)) {
		str++;
	}
	if (*str == '+') {
		str++;
	}
	else if (*str == '-') {
		str++;
		sign = -1;
	}
	while (isspace(*str)) {
		str++;
	}
	while (isalnum(*str)) {
		n *= base;
		if (isdigit(*str)) {
			n += *str - '0';
		}
		else if (islower(*str)) {
			n += *str - 'a';
		}
		else if (isupper(*str)) {
			n += *str - 'A';
		}
	}
	if (endptr != 0) {
		*endptr = (char *) str;
	}
	return sign * n;
}

unsigned long strtoul(const char *restrict str, char **restrict endptr, int base){
	return strtol(str, endptr, base);
}

static int randstate;

void srand(unsigned int seed){
	randstate = (long) seed;
}

int rand(void){
	return ((randstate = randstate * 214013L + 2531011L) >> 16) & 0x7fff;
}
