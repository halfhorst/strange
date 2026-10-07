#ifndef STRANGE_SCREENSAVER_H_
#define STRANGE_SCREENSAVER_H_

#include <time.h>

#include "src/screensaver_registry.h"

void enable_raw_mode(void);
void disable_raw_mode(void);
int strange_set_screensaver_descriptor(
    const struct strange_screensaver_descriptor *descriptor);
void strange_set_screensaver_preview(int preview);
int enter_screensaver(int use_alternate_screen);
void leave_screensaver(int show_cursor);
int render_screensaver_frame(const struct timespec *now);

#endif
