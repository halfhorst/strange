#ifndef STRANGE_SCREENSAVER_H_
#define STRANGE_SCREENSAVER_H_

#include <time.h>

#include "src/screensaver_registry.h"

void strange_terminal_enable_raw_mode(void);
void strange_terminal_disable_raw_mode(void);
int strange_screensaver_set_descriptor(
    const struct strange_screensaver_descriptor *descriptor);
void strange_screensaver_set_preview(int preview);
void strange_screensaver_set_disable_key(int key);
int strange_screensaver_enter(int use_alternate_screen);
void strange_screensaver_leave(int show_cursor);
int strange_screensaver_render_frame(const struct timespec *now);

#endif
