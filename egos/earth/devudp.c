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
#include "devudp.h"

#define MAX_UDP_LEN		8192

// skt is short for socket
static int udp_skt = -1;				// used for sending

struct dev_udp {
	int skt;
	void (*deliver)(void *, char *buf, unsigned int n);
	void *arg;
};

/* Input is available on a UDP socket.
 */
static void read_avail(void *arg){
	struct dev_udp *du = arg;
	socklen_t len = sizeof(struct sockaddr_in);
	char *buf = malloc(MAX_UDP_LEN);

	int n = recvfrom(du->skt, buf + len, MAX_UDP_LEN - len, 0,
							(struct sockaddr *) buf, &len);
	if (n < 0) {
		perror("recvfrom");
	}
	else {
		(*du->deliver)(du->arg, buf, len + n);
	}
	free(buf);
}

/* Create a "UDP device" which listens to the given port.
 */
struct dev_udp *dev_udp_create(uint16_t port,
					void (*deliver)(void *, char *, unsigned int), void *arg){
	int skt;

	if ((skt = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("dev_udp_create: socket");
		return 0;
	}

	struct sockaddr_in sin;
	memset((char *) &sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(port);

	if (bind(skt, (struct sockaddr *) &sin, sizeof(sin)) != 0) {
		perror("dev_udp_create: bind");
		close(skt);
		return 0;
	}

	/* Set ASYNC so that processes get interrupted when I/O is possible.
	 */

	if (fcntl(skt, F_SETOWN, getpid()) != 0) {
		perror("dev_udp_create: fcntl F_SETOWN");
		close(skt);
		return 0;
	}

	if (fcntl(skt, F_SETFL, O_ASYNC | O_NONBLOCK) != 0) {
		perror("dev_udp_create: fcntl F_SETFL");
		close(skt);
		return 0;
	}

	struct dev_udp *du = new_alloc(struct dev_udp);
	du->skt = skt;
	du->deliver = deliver;
	du->arg = arg;

	/* Register the socket with the interrupt handler.
	 */
	intr_register_dev(skt, read_avail, du);

	return du;
}

/* Send a UDP packet to the given destination.
 */
void dev_udp_send(struct sockaddr_in *dst, char *buf, unsigned int size){
	/* Create a socket for sending.
	 */
	if (udp_skt < 0 && (udp_skt = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("dev_udp_send: socket");
		return;
	}

	if (sendto(udp_skt, buf, size, 0,
					(struct sockaddr *) dst, sizeof(*dst)) < 0) {
		perror("dev_udp_send: sendto");
		return;
	}
}
