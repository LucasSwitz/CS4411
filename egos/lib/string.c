#include "egos.h"
#include "string.h"

void *memchr(const void *s, int c, size_t n){
	const unsigned char *u = s;

	while (n-- > 0) {
		if (*u == c) {
			return (void *) u;
		}
		u++;
	}
	return 0;
}

int memcmp(const void *s1, const void *s2, size_t n){
	const unsigned char *u1 = s1, *u2 = s2;

	while (n-- > 0) {
		if (*u1 != *u2) {
			return *u1 - *u2;
		}
		u1++; u2++;
	}
	return 0;
}

void *memset(void *b, int c, size_t len){
	unsigned char *s = b;

	c &= 0xFF;
	while (len-- > 0) {
		*s++ = c;
	}
	return b;
}

void *memmove(void *dst, const void *src, size_t n){
	char *d = dst;
	const char *s = src;

	if (dst < src) {
		while (n-- > 0) {
			*d++ = *s++;
		}
	}
	else {
		d += n;
		s += n;
		while (n-- > 0) {
			*--d = *--s;
		}
	}
	return dst;
}

void *memcpy(void *dst, const void *src, size_t n){
	return memmove(dst, src, n);
}

char *strcat(char *restrict s1, const char *restrict s2){
	char *orig = s1;

	while (*s1 != 0) {
		s1++;
	}
	while ((*s1++ = *s2++) != 0)
		;
	return orig;
}

char *strncat(char *restrict s1, const char *restrict s2, size_t n){
	char *orig = s1;

	while (*s1 != 0) {
		s1++;
	}
	while (n--) {
		*s1++ = *s2;
		if (*s2++ == 0) {
			return orig;
		}
	}
	*s1 = 0;
	return orig;
}

char *strcpy(char *dst, const char *src){
	char *d = dst;

	while ((*dst++ = *src++) != 0)
		;
	return d;
}

char *strncpy(char *dst, const char *src, size_t n){
	char *d = dst;

	while (n-- > 0) {
		if ((*dst++ = *src++) == 0) {
			while (n-- > 0) {
				*dst++ = 0;
			}
			break;
		}
	}
	return d;
}

int strcmp(const char *s1, const char *s2){
	const unsigned char *u1 = (unsigned char *) s1, *u2 = (unsigned char *) s2;

	while (*u1 != 0 && *u2 != 0) {
		if (*u1 != *u2) {
			return *u1 - *u2;
		}
		u1++; u2++;
	}
	if (*u1 == 0 && *u2 == 0) {
		return 0;
	}
	return *u1 == 0 ? -1 : 1;
}

int strncmp(const char *s1, const char *s2, size_t n){
	const unsigned char *u1 = (unsigned char *) s1, *u2 = (unsigned char *) s2;

	while (n > 0) {
		if (*u1 == 0 && *u2 == 0) {
			return 0;
		}
		if (*u1 == 0) {
			return 1;
		}
		if (*u2 == 0) {
			return -1;
		}
		if (*u1 < *u2) {
			return -1;
		}
		if (*u1 > *u2) {
			return 1;
		}
		u1++; u2++; n--;
	}
	return 0;
}

size_t strlen(const char *s){
	size_t n = 0;

	while (*s++ != 0) {
		n++;
	}
	return n;
}

size_t strnlen(const char *s, size_t maxlen) {
	size_t n = 0;

	while (n < maxlen && *s++ != 0) {
		n++;
	}
	return n;
}

int atoi(const char *s){
	bool_t minus = False;

	/* Skip blanks.
	 */
	while (*s == ' ' || *s == '\t') {
		s++;
	}

	/* Parse optional sign.
	 */
	if (*s == '-') {
		minus = True;
		s++;
	}
	else if (*s == '+') {
		s++;
	}

	int total = 0;
	while ('0' <= *s && *s <= '9') {
		total *= 10;
		total += *s - '0';
		s++;
	}
	return minus ? -total : total;
}

long atol(const char *s){
	bool_t minus = False;

	/* Skip blanks.
	 */
	while (*s == ' ' || *s == '\t') {
		s++;
	}

	/* Parse optional sign.
	 */
	if (*s == '-') {
		minus = True;
		s++;
	}
	else if (*s == '+') {
		s++;
	}

	long total = 0;
	while ('0' <= *s && *s <= '9') {
		total *= 10;
		total += *s - '0';
		s++;
	}
	return minus ? -total : total;
}

char *strchr(const char *s, int c){
	while (*s != 0) {
		if (*s == c) {
			return (char *) s;
		}
		s++;
	}
	return 0;
}

char *strstr(const char *s1, const char *s2){
	int n = strlen(s2);

	while (*s1 != 0) {
		if (strncmp(s1, s2, n) == 0) {
			return (char *) s1;
		}
		s1++;
	}
	return 0;
}

char *index(const char *s, int c){
	const char *p = s;

	while (*p != 0) {
		if (*p == c) {
			return (char *) p;
		}
		p++;
	}
	return c == 0 ? (char *) p : 0;
}

char *rindex(const char *s, int c){
	const char *p = s, *found = 0;

	while (*p != 0) {
		if (*p == c) {
			found = p;
		}
		p++;
	}
	return (char *) (c == 0 ? p : found);
}

#ifdef notdef

double atof(const char *str){
	fprintf(stderr, "atof: not yet implemented\n");
	return 0;
}

size_t strxfrm(char *restrict s1, const char *restrict s2, size_t n){
	fprintf(stderr, "strxfrm: not yet implemented\n");
	return 0;
}

void qsort(void *base, size_t nel, size_t width,
					   int (*compar)(const void *, const void *)){
	fprintf(stderr, "qsort: not yet implemented\n");
}

char *strerror(int errnum){
	fprintf(stderr, "strerror: not yet implemented\n");
	return "strerror: not yet implemented";
}

#endif // notdef
