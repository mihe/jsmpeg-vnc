#include "timer.hpp"
#include "utilities.hpp"

double timer_frequency = 0.0;

timer_t* timer_create() {
	if (timer_frequency == 0.0) {
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		timer_frequency = double(freq.QuadPart) / 1000.0;
	}

	timer_t* self = new timer_t;
	timer_reset(self);

	return self;
}

void timer_destroy(timer_t* self) {
	delete self;
}

void timer_reset(timer_t* self) {
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);
	self->base_time = time.QuadPart;
}

double timer_elapsed(timer_t* self) {
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);
	return double(time.QuadPart - self->base_time) / timer_frequency;
}

double timer_base(timer_t* self) {
	return double(self->base_time) / timer_frequency;
}

int64_t _timer_measure_start() {
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);
	return time.QuadPart;
}

void _timer_measure_end(int64_t start, double* result) {
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);
	*result = double(time.QuadPart - start) / timer_frequency;
}
