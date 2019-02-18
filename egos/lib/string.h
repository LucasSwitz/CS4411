void *memchr(const void *s, int c, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memset(void *b, int c, size_t len);

char *strcat(char *restrict s1, const char *restrict s2);
char *strncat(char *restrict s1, const char *restrict s2, size_t n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
char *strchr(const char *s, int c);
char *strstr(const char *s1, const char *s2);
size_t strxfrm(char *restrict s1, const char *restrict s2, size_t n);
char *strerror(int errnum);

char *index(const char *s, int c);
char *rindex(const char *s, int c);

int atoi(const char *s);
long atol(const char *str);
double atof(const char *str);

void qsort(void *base, size_t nel, size_t width,
		           int (*compar)(const void *, const void *));
