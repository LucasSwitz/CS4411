/* Interface to the clock device.
 */

unsigned long clock_now(void);						// returns time in msec
void clock_start_timer(unsigned int interval);		// in msec
void clock_initialize();
