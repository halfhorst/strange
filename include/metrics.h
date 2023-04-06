#ifndef SL_METRICS_H_
#define SL_METRICS_H_

long calculate_smoothed_fps(long current_duration_micros);

void draw_fps(struct ScreenBuffer *buffer, long fps);

#endif  // SL_METRICS_H_
