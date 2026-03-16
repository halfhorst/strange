#ifndef STRANGE_SCREENSAVER_H_
#define STRANGE_SCREENSAVER_H_

#include <time.h>

void enable_raw_mode(void);
void disable_raw_mode(void);
void enter_screensaver(void);
void leave_screensaver(void);
void render_screensaver_frame(const struct timespec *now);

#endif
