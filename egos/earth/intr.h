/* It seems that SIGSTKSZ on Linux is really a little too small.
 */
#ifdef LINUX
#define SIGNAL_STACK_SIZE		(3 * SIGSTKSZ)
#else
#define SIGNAL_STACK_SIZE		SIGSTKSZ
#endif

/* Types of emulated interrupts.  Should all be self-explanatory.
 */
enum intr_type {
	INTR_PAGE_FAULT,
	INTR_CLOCK,
	INTR_SYSCALL,
	INTR_IO,
	INTR_SIZE					// this should be the last one
};

/* Interrupt context passed to interrupt handler.
 */
struct intr_context {
	enum intr_type type;	// type of interrupt
	void (*ip)(void);		// instruction pointer (program counter)
	void *sp;				// stack pointer
};

/* Type of interrupt handler.
 */
typedef void (*intr_handler_t)(struct intr_context *, void *);

/* Interface to the Earth interrupt module.
 */
void intr_initialize(intr_handler_t handler);
void intr_suspend(unsigned int maxtime);
void intr_register_dev(int fd, void (*read_avail)(void *), void *arg);
void intr_save(void *p, void *sp);
void intr_restore(void *p, void *sp);
void intr_sched_event(void (*handler)(void *arg), void *arg);
