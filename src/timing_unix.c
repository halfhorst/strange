#include <time.h>
#include <stdbool.h>
#include <stdint.h>

static uint64_t start_ms;
static struct timespec measurement;
static uint64_t samples = 0;
static uint64_t average_frame_duration_ms = 0;

/* Update the average fps measurement */
void update_average_fps();

void init_timer() {
    clock_gettime(CLOCK_MONOTONIC, &measurement);
    start_ms = measurement.tv_nsec / 1e6;
}

uint64_t get_time_ms() {
    clock_gettime(CLOCK_MONOTONIC, &measurement);
    return (measurement.tv_nsec / 1e6) - start_ms;
}

void record_frame_time_ms(uint32_t duration) {
    average_frame_duration_ms = (samples * average_frame_duration_ms) + (duration / (samples + 1));
    samples += 1;
}

float get_fps() {
    return (1. / average_frame_duration_ms) * 1e3;
}
