#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "earth.h"
#include "myalloc.h"
#include "clock.h"

static struct timeval clock_start;

/* Get the current time.
 */
unsigned long clock_now(void){
	struct timeval tv;

	gettimeofday(&tv, 0);
	tv.tv_sec -= clock_start.tv_sec;
	if (tv.tv_usec < clock_start.tv_usec) {
		tv.tv_usec += 1000000;
		tv.tv_sec--;
	}
	return 1000 * tv.tv_sec + tv.tv_usec / 1000;
}

/* Start a periodic timer.  Interval in milliseconds.
 */
void clock_start_timer(unsigned int interval){
	struct itimerval it;

	it.it_value.tv_sec = interval / 1000;
	it.it_value.tv_usec = (interval % 1000) * 1000;
	it.it_interval = it.it_value;
	if (setitimer(ITIMER_REAL, &it, 0) != 0) {
		perror("clock_start_timer: setitimer");
		exit(1);
	}
}

void clock_initialize(){
	gettimeofday(&clock_start, 0);
}
