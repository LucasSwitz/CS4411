#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "earth.h"
#include "myalloc.h"
#include "intr.h"
#include "devtty.h"

#define MAX_TTY_LEN		8192

struct dev_tty {
	int fd;
	void (*deliver)(void *, char *buf, int n);
	void *arg;
};

/* Input is available on a TTY socket.
 */
static void tty_read_avail(void *arg){
	struct dev_tty *dt = arg;
	char *buf = malloc(MAX_TTY_LEN);

	int n = read(dt->fd, buf, MAX_TTY_LEN);
	if (n < 0) {
		perror("tty_read_avail: read");
	}
	else {
		(*dt->deliver)(dt->arg, buf, n);
	}
	free(buf);
}

/* Create a "TTY device" to receive data on the given file descriptor.
 */
struct dev_tty *dev_tty_create(int fd, void (*deliver)(void *, char *, int), void *arg){
	/* Set ASYNC so that processes get interrupted when I/O is possible.
	 */

	/* If you set the O_ASYNC status flag on a file descriptor by using
     *  the F_SETFL command of fcntl(), a SIGIO signal is sent  whenever
     *  input  or  output  becomes  possible  on  that  file descriptor.
	 */
	if (fcntl(fd, F_SETFL, O_ASYNC) != 0) {
		perror("dev_tty_create: fcntl F_SETFL");
		return 0;
	}

#ifndef MACOSX
	/* Set the process ID or process group ID that will receive SIGIO
     *  and SIGURG signals for events on file descriptor fd to the ID
     *  given in arg.
     */
	if (fcntl(fd, F_SETOWN, getpid()) != 0) {
		perror("dev_tty_create: fcntl F_SETOWN");
		return 0;
	}
#endif

	struct dev_tty *dt = new_alloc(struct dev_tty);
	dt->fd = fd;
	dt->deliver = deliver;
	dt->arg = arg;

	/* Register the device with the interrupt handler.
	 */
	intr_register_dev(fd, tty_read_avail, dt);

	return dt;
}
