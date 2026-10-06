/*
  A ball that bounces around the screen, as a native screensaver.

  Build it with `make examples` and copy the resulting library into
  ~/.strange/, then run `strange bounce`.
*/
#include <stdlib.h>

#include "src/screensaver_registry.h"

struct bounce_state {
  int x;
  int y;
  int step_x;
  int step_y;
};

static int bounce_init(void **state, struct ScreenBuffer *buffer) {
  struct bounce_state *bounce = calloc(1, sizeof(*bounce));

  if (bounce == NULL) {
    strange_screensaver_set_error("bounce: out of memory");
    return -1;
  }

  bounce->x = buffer->w / 2;
  bounce->y = buffer->h / 2;
  bounce->step_x = 1;
  bounce->step_y = 1;
  *state = bounce;
  return 0;
}

static int bounce_update(void *state, struct ScreenBuffer *buffer,
                         const struct strange_screensaver_frame *frame) {
  struct bounce_state *bounce = state;

  if (frame->frame_count % 3 == 0) {
    if (bounce->x + bounce->step_x < 0 || bounce->x + bounce->step_x >= buffer->w) {
      bounce->step_x = -bounce->step_x;
    }
    if (bounce->y + bounce->step_y < 0 || bounce->y + bounce->step_y >= buffer->h) {
      bounce->step_y = -bounce->step_y;
    }
    bounce->x += bounce->step_x;
    bounce->y += bounce->step_y;
  }

  write_to_buffer(buffer, "O", 1, bounce->x, bounce->y);
  return 0;
}

static void bounce_cleanup(void *state) { free(state); }

const struct strange_screensaver_descriptor strange_screensaver_descriptor = {
    .api_version = STRANGE_SCREENSAVER_API_VERSION,
    .name = "bounce",
    .character_width = 1,
    .init = bounce_init,
    .update = bounce_update,
    .cleanup = bounce_cleanup,
};
