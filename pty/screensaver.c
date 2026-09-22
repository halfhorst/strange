#include "screensaver.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "src/renderer.h"
#include "watermark.h"

static struct termios orig_termios;
static int raw_mode_enabled = 0;
static int screensaver_visible = 0;
static struct strange_render_context screensaver_context;
static int screensaver_context_ready = 0;
static const struct strange_screensaver_descriptor *selected_descriptor = NULL;
static struct strange_screensaver_instance screensaver_instance;
static int screensaver_instance_ready = 0;

static void destroy_render_context(void) {
  if (!screensaver_context_ready) {
    return;
  }

  strange_render_context_destroy(&screensaver_context);
  memset(&screensaver_context, 0, sizeof(screensaver_context));
  screensaver_context_ready = 0;
}

static void cleanup_screensaver_instance(void) {
  if (!screensaver_instance_ready) {
    return;
  }

  strange_screensaver_instance_cleanup(&screensaver_instance);
  memset(&screensaver_instance, 0, sizeof(screensaver_instance));
  screensaver_instance_ready = 0;
}

static int ensure_render_context(void) {
  int character_width = 1;

  if (screensaver_context_ready) {
    return 0;
  }
  if (selected_descriptor == NULL) {
    errno = EINVAL;
    return -1;
  }

  character_width = strange_screensaver_character_width(selected_descriptor);

  if (strange_render_context_init(&screensaver_context, STDOUT_FILENO, stdout,
                                  character_width) == -1) {
    return -1;
  }

  screensaver_context_ready = 1;
  return 0;
}

int strange_set_screensaver_descriptor(
    const struct strange_screensaver_descriptor *descriptor) {
  if (screensaver_visible || screensaver_instance_ready) {
    errno = EBUSY;
    return -1;
  }
  if (strange_screensaver_descriptor_validate(descriptor) == -1) {
    return -1;
  }

  selected_descriptor = descriptor;
  return 0;
}

void disable_raw_mode(void) {
  if (!raw_mode_enabled) {
    return;
  }

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  raw_mode_enabled = 0;
}

void enable_raw_mode(void) {
  if (raw_mode_enabled) {
    return;
  }

  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disable_raw_mode);

  struct termios raw = orig_termios;
  cfmakeraw(&raw);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  raw_mode_enabled = 1;
}

int enter_screensaver(void) {
  if (screensaver_visible) {
    return 0;
  }
  if (selected_descriptor == NULL) {
    errno = EINVAL;
    return -1;
  }
  if (ensure_render_context() == -1) {
    return -1;
  }
  if (strange_screensaver_instance_init(&screensaver_instance,
                                        selected_descriptor,
                                        &screensaver_context.buffer) == -1) {
    destroy_render_context();
    return -1;
  }

  screensaver_instance_ready = 1;
  screensaver_visible = 1;

  printf("\033[2J\033[H");
  printf("\033[?25l");
  fflush(stdout);
  return 0;
}

void leave_screensaver(void) {
  if (!screensaver_visible) {
    return;
  }

  screensaver_visible = 0;
  cleanup_screensaver_instance();

  printf("\033[?25h");
  fflush(stdout);
  destroy_render_context();
}

int render_screensaver_frame(const struct timespec *now) {
  struct strange_screensaver_frame frame = {
      .now = now,
      .frame_count = 0,
  };

  if (!screensaver_visible) {
    return 0;
  }
  if (ensure_render_context() == -1) {
    return -1;
  }
  if (strange_render_context_refresh_size(&screensaver_context) == -1) {
    return -1;
  }

  strange_render_context_begin_frame(&screensaver_context);
  frame.frame_count = screensaver_context.frame_count;
  if (!screensaver_instance_ready ||
      strange_screensaver_instance_update(&screensaver_instance,
                                          &screensaver_context.buffer,
                                          &frame) == -1) {
    return -1;
  }
  render_screensaver_watermark(&screensaver_context.buffer);
  return strange_render_context_present(&screensaver_context);
}
