#ifndef SL_TIMING_H_
#define SL_TIMING_H_


/*
Start time measurement
*/
void start_clock();


/*
Get time elapsed in ms since start_clock(). Returns 0 if start_clock was never called
*/
long end_clock();


/*
Get the current smoothed FPS
*/
int get_fps();

#endif  // SL_TIMING_H_
