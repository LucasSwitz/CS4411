#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "earth.h"
#include "myalloc.h"
#include "clock.h"
#include "log.h"

static FILE *log_fd;

void panic(const char *fmt, ...){
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "Unrecoverable error: ");
	fprintf(stderr, fmt, ap);
	fprintf(stderr, "\n\r");
	va_end(ap);
	exit(1);
}

void log_p(const char *fmt, ...){
	va_list ap;

	va_start(ap, fmt);
	fprintf(log_fd, "%lu,", clock_now());
	vfprintf(log_fd, fmt, ap);
	fprintf(log_fd, "\n");
	va_end(ap);
	fflush(log_fd);
}

void log_init(void){
	log_fd = fopen("log.txt", "w");
	setlinebuf(log_fd);
}
