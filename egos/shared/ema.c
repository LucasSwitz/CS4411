/* Code for exponential moving averaging.  Works with uneven intersample
 * periods.  Based on Section 4.3 of "Algorithms for Unevenly Spaced Time
 * Series: Moving Averages and Other Rolling Operators" by Andreas Eckner.
 */

#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "assert.h"
#include "egos.h"
#include "ema.h"

/* Initialize an exponential moving average tracker.  alpha determines
 * how agressive the average is being updated.
 */
void ema_init(struct ema_state *es, double alpha){
	memset(es, 0, sizeof(*es));
	es->first = True;
	es->alpha = alpha;
}

/* Update the moving average.
 */
void ema_update(struct ema_state *es, double sample){
	unsigned long now = sys_gettime();

	if (es->first) {
		es->first = False;
		es->ema = sample;
	}
	else {
		// es->ema = es->alpha * sample + (1 - es->alpha) * es->ema;

		if (now == es->last_time) {
			return;
		}
		double tmp = (now - es->last_time) / 1000.0 / (1 - es->alpha);
		double w = exp(-tmp);
		double w2 = (1 - w) / tmp;
		es->ema = w * es->ema + (w2 - w) * es->last_sample + (1 - w2) * sample;
	}

	es->last_time = now;
	es->last_sample = sample;
}

/* Return the moving average.
 */
double ema_avg(struct ema_state *es){
	assert(!es->first);
	return es->ema;
}
