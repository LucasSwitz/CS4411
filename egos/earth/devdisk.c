#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "earth.h"
#include "myalloc.h"
#include "intr.h"
#include "devdisk.h"

struct dev_disk {
	int fd;
	unsigned int nblocks;
	bool_t sync;
};

struct dd_event {
	struct dev_disk *dd;
	void (*completion)(void *arg, bool_t success);
	void *arg;
	bool_t success;
};

/* Create a "disk device", simulated on a file.
 */
struct dev_disk *dev_disk_create(char *file_name, unsigned int nblocks, bool_t sync){
	struct dev_disk *dd = new_alloc(struct dev_disk);

	/* Open the disk.  Create if non-existent.
	 */
	dd->fd = open(file_name, O_RDWR, 0600);
	if (dd->fd < 0) {
		dd->fd = open(file_name, O_RDWR | O_CREAT, 0600);

		if (dd->fd < 0) {
			perror(file_name);
			free(dd);
			return 0;
		}
	}
	dd->nblocks = nblocks;
	dd->sync = sync;
	return dd;
}

/* Simulated disk completion event.
 */
static void dev_disk_complete(void *arg){
	struct dd_event *ddev = arg;

	(*ddev->completion)(ddev->arg, ddev->success);
	free(ddev);
}

/* Simulate a disk operation completion event.
 */
static void dev_disk_make_event(struct dev_disk *dd,
			void (*completion)(void *arg, bool_t success), void *arg, bool_t success){
	struct dd_event *ddev = new_alloc(struct dd_event);

	ddev->dd = dd;
	ddev->completion = completion;
	ddev->arg = arg;
	ddev->success = success;
	intr_sched_event(dev_disk_complete, ddev);
}

static void disk_seek(struct dev_disk *dd, unsigned int offset){
	assert(offset < dd->nblocks);
	lseek(dd->fd, (off_t) offset * BLOCK_SIZE, SEEK_SET);
}

/* Write a block.  Invoke completion() when done.
 */
void dev_disk_write(struct dev_disk *dd, unsigned int offset, const char *data,
				void (*completion)(void *arg, bool_t success), void *arg){
	bool_t success;

	disk_seek(dd, offset);

	int n = write(dd->fd, data, BLOCK_SIZE);
	if (n < 0) {
		perror("dev_disk_write");
		success = False;
	}
	else if (n != BLOCK_SIZE) {
		fprintf(stderr, "disk_write: wrote only %d bytes\n", n);
		success = False;
	}
	else {
		if (dd->sync) {
			fsync(dd->fd);
		}
		success = True;
	}

	dev_disk_make_event(dd, completion, arg, success);
}

/* Read a block.  Invoke completion() when done.
 */
void dev_disk_read(struct dev_disk *dd, unsigned int offset, char *data,
				void (*completion)(void *arg, bool_t success), void *arg){
	bool_t success;

	disk_seek(dd, offset);

	int n = read(dd->fd, data, BLOCK_SIZE);
	if (n < 0) {
		perror("dev_disk_read");
		success = False;
	}
	else {
		if (n < BLOCK_SIZE) {
			memset((char *) data + n, 0, BLOCK_SIZE - n);
		}
		success = True;
	}

	dev_disk_make_event(dd, completion, arg, success);
}
