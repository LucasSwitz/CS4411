#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "egos.h"
#include "string.h"

#ifdef x86_32
typedef int32_t int_t;
typedef uint32_t uint_t;
#endif
#ifdef x86_64
typedef int64_t int_t;
typedef uint64_t uint_t;
#endif

/* Allocate a "memory channel".
 */
struct mem_chan *mc_alloc(){
	return (struct mem_chan *) calloc(1, sizeof(struct mem_chan));
}

/* Release a memory channel.
 */
void mc_free(struct mem_chan *mc){
	free(mc->buf);
	free(mc);
}

/* Append the given buffer of the given size to the memory channel.
 */
void mc_put(struct mem_chan *mc, char *buf, unsigned int size){
	if (mc->offset + size > mc->size) {
		mc->size = mc->size * 2;
		if (mc->offset + size > mc->size) {
			mc->size = mc->offset + size;
		}
		mc->buf = realloc(mc->buf, mc->size);
	}
	memcpy(mc->buf + mc->offset, buf, size);
	mc->offset += size;
}

/* Append a character to the memory channel.
 */
void mc_putc(struct mem_chan *mc, char c){
	mc_put(mc, &c, 1);
}

/* Append a null-terminated string (sans null character).
  */
void mc_puts(struct mem_chan *mc, char *s){
	while (*s != 0) {
		mc_putc(mc, *s++);
	}
}

/* Helper function for printing an unsigned integer in a particular base.
 * caps is true is we should use upper case hex characters.
 */
static void mc_unsigned_int(struct mem_chan *mc, uint_t d, unsigned int base, bool_t caps){
	char buf[64], *p = &buf[64];
	char *chars = caps ? "0123456789ABCDEF" : "0123456789abcdef";

	if (base < 2 || base > 16) {
		mc_puts(mc, "<bad base>");
		return;
	}
	if (d == 0) {
		mc_putc(mc, '0');
		return;
	}
	*--p = 0;
	while (d != 0) {
		*--p = chars[d % base];
		d /= base;
	}
	mc_puts(mc, p);
}

/* Helper function for printing a signed integer in a particular base.
 * caps is true is we should use upper case hex characters.
 */
static void mc_signed_int(struct mem_chan *mc, int_t d, unsigned int base, bool_t caps){
	char buf[64], *p = &buf[64];
	char *chars = caps ? "0123456789ABCDEF" : "0123456789abcdef";

	if (base < 2 || base > 16) {
		mc_puts(mc, "<bad base>");
		return;
	}
	if (d == 0) {
		mc_putc(mc, '0');
		return;
	}
	if (d < 0) {
		mc_putc(mc, '-');
		d = -d;
	}
	*--p = 0;
	while (d != 0) {
		*--p = chars[d % base];
		d /= base;
	}
	mc_puts(mc, p);
}

/* Version of vprintf() that appends to a memory channel.
 */
void mc_vprintf(struct mem_chan *mc, const char *fmt, va_list ap){
	while (*fmt != 0) {
		if (*fmt != '%') {
			mc_putc(mc, *fmt++);
			continue;
		}
		if (*++fmt == 0) {
			break;
		}
		switch(*fmt) {
		case 'c':
			mc_putc(mc, va_arg(ap, int));
			break;
		case 'd': case 'i':
			mc_signed_int(mc, (int_t) va_arg(ap, int), 10, False);
			break;
		case 's':
			mc_puts(mc, va_arg(ap, char *));
			break;
		case 'u':
			mc_unsigned_int(mc, (uint_t) va_arg(ap, unsigned int), 10, False);
			break;
		case 'x':
			mc_unsigned_int(mc, (uint_t) va_arg(ap, unsigned int), 16, False);
			break;
		case 'X':
			mc_unsigned_int(mc, (uint_t) va_arg(ap, unsigned long), 16, True);
			break;
		default:
			mc_putc(mc, *fmt);
		}
		fmt++;
	}
}

/* Version of printf() that adds to a memory channel.
 */
void mc_printf(struct mem_chan *mc, const char *fmt, ...){
	va_list ap;

	va_start(ap, fmt);
	mc_vprintf(mc, fmt, ap);
	va_end(ap);
}
