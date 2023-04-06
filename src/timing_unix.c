#include <time.h>
#include <stdbool.h>

static struct timespec before;
static struct timespec after;
static bool started = false;


void start_clock() {
    clock_gettime(CLOCK_REALTIME, &before);
    started = true;
}

long end_clock() {
    if (false == started) {
        return 0;
    }
    clock_gettime(CLOCK_REALTIME, &after);
    started = false;
    return (after.tv_nsec - before.tv_nsec) / 1000;
}

int get_fps() {
    // return smoothed fps

    return 0;
}
