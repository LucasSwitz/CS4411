struct ema_state {
	bool_t first;
	double alpha, ema, last_sample;
	unsigned long last_time;
};

void ema_init(struct ema_state *es, double alpha);
void ema_update(struct ema_state *es, double sample);
double ema_avg(struct ema_state *es);
