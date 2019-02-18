#define printf	grass_printf
#define fprintf	grass_fprintf

int printf(const char *fmt, ...);
int vsnprintf(char * restrict str, size_t size, const char * restrict fmt, va_list ap);
int vsprintf(char * restrict str, const char * restrict fmt, va_list ap);
int sprintf(char * restrict str, const char * restrict fmt, ...);
int snprintf(char * restrict str, size_t size, const char * restrict fmt, ...);
int vasprintf(char **strp, const char *fmt, va_list ap);
int asprintf(char **strp, const char *fmt, ...);
