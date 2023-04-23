#ifndef TIMING_H_
#define TIMING_H_

#include <stdint.h>

/* Set timing reference point */
void init_timer();

/* Get elapsed time in ms since the reference point*/
uint64_t get_time_ms();

/* Record a frame duration for FPS measurement*/
void log_frame_time_ms();

/* Get the current, smoothed FPS */
float get_fps();

#endif  // TIMING_H_
