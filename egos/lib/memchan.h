/* A memory channel is a printable memory object that can be easily
 * appended to.
 */

struct mem_chan {
	char *buf;
	unsigned int size, offset;
};

struct mem_chan *mc_alloc();
void mc_free(struct mem_chan *mc);
void mc_put(struct mem_chan *mc, char *buf, unsigned int size);
void mc_putc(struct mem_chan *mc, char c);
void mc_puts(struct mem_chan *mc, char *s);
void mc_vprintf(struct mem_chan *mc, const char *fmt, va_list ap);
void mc_printf(struct mem_chan *mc, const char *fmt, ...);
