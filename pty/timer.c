#include "timer.h"

#if defined(__APPLE__) || defined(__linus__)
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static bool is_running = false;
static struct timespec start_time;
static struct timespec check_time;

void timer_start(void) {
  int err = clock_gettime(CLOCK_MONOTONIC, &start_time);

  if (err != 0) {
    perror("clock_gettime");
    exit(EXIT_FAILURE);
  }
  is_running = true;
}

double timer_elapsed_ms(void) {
  if (!is_running) {
    fprintf(stderr, "Timer is not running. Call timer_start() first.\n");
    return -1;
  }

  int err = clock_gettime(CLOCK_MONOTONIC, &check_time);
  if (err != 0) {
    perror("clock_gettime");
    exit(EXIT_FAILURE);
  }

  return (check_time.tv_sec - start_time.tv_sec) +
         (check_time.tv_nsec - start_time.tv_nsec) / 1e6;
}

void stop_timer(void) { is_running = false; }
#endif
