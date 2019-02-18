/* Interface to the UDP device.
 */

struct dev_udp *dev_udp_create(uint16_t port,
					void (*deliver)(void *, char *, unsigned int), void *arg);
void dev_udp_send(struct sockaddr_in *dest, char *buf, int unsigned size);
