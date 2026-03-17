#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "./digital_rain.h"
#include "../renderer.h"

#define CHAR_SHUFFLE_RATE 0.02
#define STREAM_ACTIVATE_RATE 0.002
#define STREAM_SHUTDOWN_RATE 0.006
#define STREAM_SCROLL_RATE() (3 + (rand() % 10))
#define MIN_STREAM_LENGTH 15
#define STREAM_BUFFER_SIZE 250
#define STREAM_BUFFER_NUM 750

struct Stream {
  int x;
  int start_visible;
  int end_visible;
  char *characters;
  int buffer_size;
  int scroll_rate;
};

struct digital_rain_state {
  struct Stream *streams;
};

static int allocate_streams(struct digital_rain_state *state);
static void free_streams(struct digital_rain_state *state);
static void random_character(char *buffer);
static char *get_character_stream(int n);
static void toggle_streams(struct digital_rain_state *state, int w,
                           int minimum_stream_length);
static void shift_visible_window(struct digital_rain_state *state, int w, int h,
                                 unsigned long frame_count);

static int digital_rain_init(void **state, struct ScreenBuffer *buffer) {
  struct digital_rain_state *rain = NULL;

  (void)buffer;
  if (state == NULL) {
    return -1;
  }

  rain = calloc(1, sizeof(*rain));
  if (rain == NULL) {
    return -1;
  }
  if (allocate_streams(rain) == -1) {
    free(rain);
    return -1;
  }

  *state = rain;
  return 0;
}

static int digital_rain_update(
    void *state, struct ScreenBuffer *sbuffer,
    const struct strange_screensaver_frame *frame) {
  struct digital_rain_state *rain = state;
  unsigned long frame_count = 0;
  int minimum_stream_length = 0;

  if (rain == NULL || sbuffer == NULL) {
    return -1;
  }
  if ((sbuffer->h > STREAM_BUFFER_SIZE) || (sbuffer->w > STREAM_BUFFER_NUM)) {
    return -1;
  }

  if (frame != NULL) {
    frame_count = frame->frame_count;
  }

  minimum_stream_length = sbuffer->h / 3;
  if (minimum_stream_length < MIN_STREAM_LENGTH) {
    minimum_stream_length = MIN_STREAM_LENGTH;
  }

  toggle_streams(rain, sbuffer->w, minimum_stream_length);

  shift_visible_window(rain, sbuffer->w, sbuffer->h, frame_count);

  for (int i = 0; i < sbuffer->w; i++) {
    int visible_end = rain->streams[i].end_visible;

    if (visible_end > sbuffer->h) {
      visible_end = sbuffer->h;
    }

    for (int j = rain->streams[i].start_visible; j < visible_end; j++) {
      if (j >= 0) {
        if (((float)rand() / RAND_MAX) < CHAR_SHUFFLE_RATE) {
          char character[DIGITAL_RAIN_CHAR_WIDTH];

          random_character(character);
          memcpy(rain->streams[i].characters + (j * DIGITAL_RAIN_CHAR_WIDTH),
                 character, DIGITAL_RAIN_CHAR_WIDTH);
        }

        write_to_buffer(sbuffer,
                        rain->streams[i].characters +
                            (j * DIGITAL_RAIN_CHAR_WIDTH),
                        DIGITAL_RAIN_CHAR_WIDTH, i, j);
      }
    }
  }
  return 0;
}

static void digital_rain_cleanup(void *state) {
  struct digital_rain_state *rain = state;

  if (rain == NULL) {
    return;
  }

  free_streams(rain);
  free(rain);
}

const struct strange_screensaver_descriptor strange_digital_rain_descriptor = {
    .name = "digital_rain",
    .character_width = DIGITAL_RAIN_CHAR_WIDTH,
    .init = digital_rain_init,
    .update = digital_rain_update,
    .cleanup = digital_rain_cleanup,
};

static int allocate_streams(struct digital_rain_state *state) {
  if (state == NULL) {
    return -1;
  }

  state->streams = calloc(STREAM_BUFFER_NUM, sizeof(*state->streams));
  if (state->streams == NULL) {
    return -1;
  }

  for (int i = 0; i < STREAM_BUFFER_NUM; i++) {
    state->streams[i].x = i;
    state->streams[i].start_visible = -1;
    state->streams[i].end_visible = -1;
    state->streams[i].characters = get_character_stream(STREAM_BUFFER_SIZE);
    if (state->streams[i].characters == NULL) {
      free_streams(state);
      return -1;
    }
    state->streams[i].buffer_size = STREAM_BUFFER_SIZE;
    state->streams[i].scroll_rate = STREAM_SCROLL_RATE();
  }

  return 0;
}

static void free_streams(struct digital_rain_state *state) {
  if (state == NULL || state->streams == NULL) {
    return;
  }

  for (int i = 0; i < STREAM_BUFFER_NUM; i++) {
    free(state->streams[i].characters);
    state->streams[i].characters = NULL;
  }

  free(state->streams);
  state->streams = NULL;
}

static void random_character(char *buffer) {
  float draw = (float)rand() / RAND_MAX;

  if (draw < 0.45) {
    buffer[0] = (char)0xEF;
    buffer[1] = (char)0xBD;
    buffer[2] = (char)0xA5 + (rand() % 27);
  } else if (draw < 0.9) {
    buffer[0] = (char)0xEF;
    buffer[1] = (char)0xBE;
    buffer[2] = (char)0x80 + (rand() % 30);
  } else {
    buffer[0] = (char)0x30 + (rand() % 10);
    buffer[1] = (char)SL_PAD_CHAR;
    buffer[2] = (char)SL_PAD_CHAR;
  }
}

static char *get_character_stream(int n) {
  char *char_stream = malloc(sizeof(char) * n * DIGITAL_RAIN_CHAR_WIDTH);
  char kana[DIGITAL_RAIN_CHAR_WIDTH];

  if (char_stream == NULL) {
    return NULL;
  }

  for (int i = 0; i < (n * DIGITAL_RAIN_CHAR_WIDTH);
       i += DIGITAL_RAIN_CHAR_WIDTH) {
    random_character(kana);
    memcpy(char_stream + i, kana, DIGITAL_RAIN_CHAR_WIDTH);
  }

  return char_stream;
}

static void toggle_streams(struct digital_rain_state *state, int w,
                           int minimum_stream_length) {
  for (int i = 0; i < w; i++) {
    if (state->streams[i].end_visible == -1) {
      if (((float)rand() / RAND_MAX) < STREAM_ACTIVATE_RATE) {
        state->streams[i].end_visible++;
      }
    } else if (state->streams[i].end_visible > minimum_stream_length) {
      if (((float)rand() / RAND_MAX) < STREAM_SHUTDOWN_RATE) {
        state->streams[i].start_visible++;
      }
    }
  }
}

static void shift_visible_window(struct digital_rain_state *state, int w, int h,
                                 unsigned long frame_count) {
  for (int i = 0; i < w; i++) {
    int trace = (frame_count % (unsigned long)state->streams[i].scroll_rate) == 0;

    if (trace) {
      if ((state->streams[i].end_visible > -1) &&
          (state->streams[i].end_visible <= h)) {
        state->streams[i].end_visible++;
      }
      if ((state->streams[i].start_visible > -1) &&
          (state->streams[i].start_visible <= h)) {
        state->streams[i].start_visible++;
      }
    }

    if ((state->streams[i].start_visible > h) &&
        (state->streams[i].end_visible > h)) {
      state->streams[i].start_visible = -1;
      state->streams[i].end_visible = -1;
    }
  }
}
