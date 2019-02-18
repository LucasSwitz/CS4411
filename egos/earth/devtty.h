/* Interface to the terminal device.
 */

struct dev_tty *dev_tty_create(int fd,
					void (*deliver)(void *, char *, int), void *arg);
