#pragma once

typedef struct timer_t {
	int64_t base_time = 0;
} timer_t;

timer_t* timer_create();
void timer_destroy(timer_t* self);
void timer_reset(timer_t* self);
double timer_elapsed(timer_t* self);
double timer_base(timer_t* self);

int64_t _timer_measure_start();
void _timer_measure_end(int64_t start, double* result);

// double elapsed = timer_measure(elapsed) { <statements to measure> }
#define timer_measure(TIME)                                                             \
	for (int64_t TIME##_timer_start = 0;                                                \
		 TIME##_timer_start == 0 && (TIME##_timer_start = _timer_measure_start()) != 0; \
		 _timer_measure_end(TIME##_timer_start, &TIME))
