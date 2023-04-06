#include <math.h>
# include <stdio.h>

#include "./renderer.h"

static float smooth_factor = 0.9;
static long previous_duration_micros = 0.0;
static char formatted_fps[20];

long calculate_smoothed_fps(long current_duration_micros) {
    long smoothed_fps;
    if (previous_duration_micros == 0.0) {
        previous_duration_micros = current_duration_micros;
        smoothed_fps = (1.0 / current_duration_micros) * 1.e6;
    } else {
        long smoothed_duration = (current_duration_micros * smooth_factor) 
                    + (previous_duration_micros * (1.0 - smooth_factor));

        previous_duration_micros = current_duration_micros;
        smoothed_fps = (1.0 / smoothed_duration) * 1.e6;
    }

    return round(smoothed_fps);
    // return current_duration_micros;
}

void draw_fps(struct ScreenBuffer *buffer, long fps) {
    int x_chars_required = 9;
    int y_chars_required = 5;

    if (buffer->w < x_chars_required || buffer->h < y_chars_required) {
        return;
    }

    // char formatted_fps[9];
    snprintf(formatted_fps, sizeof(formatted_fps), " | %.3i | ", (int) fps);

    // write_to_buffer(buffer, " ───── ", 7, 0, 1);
    write_to_buffer(buffer, formatted_fps, 9, 0, 2);
    // write_to_buffer(buffer, " ───── ", 7, 0, 3);



}


