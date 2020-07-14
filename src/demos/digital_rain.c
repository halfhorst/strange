#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "./digital_rain.h"
#include "../renderer.h"

// The rate at which individual characters are shuffled
#define CHAR_SHUFFLE_RATE 0.02

// The rate at which streams are toggled `on`
#define STREAM_ACTIVATE_RATE 0.002

// The rate at which streams are toggled `off`
#define STREAM_SHUTDOWN_RATE 0.006

// The rate in frames at which the visible window scrolls
# define STREAM_SCROLL_RATE 3 + (rand() % 10)

// The minimum char length of a stream. Streams less than this length cannot be
// turned off.
#define MIN_STREAM_LENGTH 15

// The number and length of allocated streams. Bigger than any reasonable
// terminal (I hope)
#define STREAM_BUFFER_SIZE 250
#define STREAM_BUFFER_NUM  750

/*
  A vertical stream of characters. This stream is visible over a defined
  window.

  A stream can be turned `off` by setting end_visible to -1, which means the
  stream should not be rendered. Any value >= 0 will result in the stream
  being considered `on` and rendering, and non-negative `start_visible` and
  `end_visible` will be iterated.
*/
struct Stream {
  int x;              // the horizontal position of the stream
  int start_visible;  // the start of the visible region of the stream
  int end_visible;    // the end of the visible region of the stream
  char *characters;   // the stream of characters itself
  int buffer_size;    // the length of the `characters` buffer
  int scroll_rate;    // the rate at which the visible window shifts. This is
                      // modded against frame # to determine when increments
                      // occur
};

/*
  Allocates streams on the heap, returning NULL on failure. The streams are
  parametrized by several define statements above.
*/
struct Stream *allocate_streams();

/*
  Place a random character in `buffer`. Charcters are drawn from UTF-8
  half-width katakana and the digits [0, 9].
*/
void random_character(char *buff);

/*
  Allocate a character stream of `n` random characters on the heap. Returns
  NULL on failure.
*/
char *get_character_stream(int n);

/*
  Modifies global stream state.

  Initiates scrolling of the beginning and end of streams [0, `w`] according
  to their activation and shutdown rates and other criteria.

  Streams begin with their window indices set to -1. Streams are "started" by
  incrementing the end of the visible to 0, whereby the shifting function will
  begin incrementing it. The end of a stream  is initiated when the start of
  the visible window is incremented to 0. At that point, the start and end
  increment until they both exceed the height of the terminal and are reset to
  -1.

  Streams are only eligible to be toggled on if the end index is -1, and
  elgigible to be toggled off if the start index is -1 and the length of the
  stream exceeds `minimum_stream_length`.
*/
void toggle_streams(int w, int minimum_stream_length);

/*
  Modifies global stream state.

  Increment the visible window of streams [0, `w`] according to their
  individual scroll rates. Reset streams once both ends of the visible window
  exceed `h`.

  A window endpoint is incremented by one and only if
  scroll_rate % frame_count == 0 and if it is greater than -1 and less than or
  equal to `h`.
*/
void shift_visible_window(int w, int h, unsigned long frame_count);


// Digital Rain global state
static struct Stream *streams;


void digital_rain_init(struct ScreenBuffer *sbuffer) {
  streams = allocate_streams(STREAM_BUFFER_NUM, STREAM_BUFFER_SIZE);
}

bool digital_rain_update(struct ScreenBuffer *sbuffer,
                         unsigned long frame_count) {
  if ((sbuffer->h > STREAM_BUFFER_SIZE) || (sbuffer->w > STREAM_BUFFER_NUM)) {
    return false;
  }

  int minimum_stream_length = sbuffer->h / 3;
  toggle_streams(sbuffer->w, minimum_stream_length);

  shift_visible_window(sbuffer->w, sbuffer->h, frame_count);

  // for each stream in scope, print its visible region
  for (int i = 0; i < sbuffer->w; i++) {
    for (int j = streams[i].start_visible;
         j < fmin(sbuffer->h, streams[i].end_visible);
         j++) {
      if (j >= 0) {

        // shuffle visisble characters according to our rate
        if (((float) rand() / RAND_MAX) < CHAR_SHUFFLE_RATE) {
          char character[DIGITAL_RAIN_CHAR_WIDTH];
          random_character(character);
          memcpy(streams[i].characters + (j * DIGITAL_RAIN_CHAR_WIDTH),
                 character, DIGITAL_RAIN_CHAR_WIDTH);
        }

        write_to_buffer(sbuffer,
                        streams[i].characters + (j * DIGITAL_RAIN_CHAR_WIDTH),
                        DIGITAL_RAIN_CHAR_WIDTH, i, j);
      }
    }
  }
  return true;
}

void digital_rain_cleanup(void) {
  for (int i = 0; i < STREAM_BUFFER_SIZE; i++) {
    free(streams[i].characters);
  }
  free(streams);
}

struct Stream *allocate_streams() {
  struct Stream *streams = malloc(sizeof(struct Stream) * STREAM_BUFFER_NUM);
  if (streams == NULL) {
    return NULL;
  }

  for (int i = 0; i < STREAM_BUFFER_NUM; i++) {
    streams[i].x = i;
    streams[i].start_visible = -1;
    streams[i].end_visible = -1;
    streams[i].characters = get_character_stream(STREAM_BUFFER_SIZE);
    streams[i].buffer_size = STREAM_BUFFER_SIZE;
    streams[i].scroll_rate = STREAM_SCROLL_RATE;
  }
  return streams;
}

void random_character(char *buffer) {
  float draw = (float) rand() / RAND_MAX;
  if (draw < 0.45) {
    buffer[0] = (char) 0xEF;
    buffer[1] = (char) 0xBD;
    buffer[2] = (char) 0xA5 + (rand() % 27);
  } else if (draw < 0.9) {
    buffer[0] = (char) 0xEF;
    buffer[1] = (char) 0xBE;
    buffer[2] = (char) 0x80 + (rand() % 30);
  } else {
    buffer[0] = (char) 0x30 + (rand() % 10);
    buffer[1] = (char) SL_PAD_CHAR;
    buffer[2] = (char) SL_PAD_CHAR;
  }
}

char *get_character_stream(int n) {
  char *char_stream = malloc(sizeof(char) * n * DIGITAL_RAIN_CHAR_WIDTH);
  if (char_stream == NULL) {
    return NULL;
  }
  for (int i = 0;
        i < (n * DIGITAL_RAIN_CHAR_WIDTH);
          i += DIGITAL_RAIN_CHAR_WIDTH) {
    char kana[DIGITAL_RAIN_CHAR_WIDTH];
    random_character(kana);
    memcpy(char_stream + i, kana, DIGITAL_RAIN_CHAR_WIDTH);
  }
  return char_stream;
}

void toggle_streams(int w, int minimum_stream_length) {
  for (int i = 0; i < w; i++) {
    // off is defined as end_visible == -1
    // on and eligible is defined as end_visible > the minimum desired stream length
    if (streams[i].end_visible == -1) {
      if (((float) rand() / RAND_MAX) < STREAM_ACTIVATE_RATE) {
        streams[i].end_visible++; // begin end of window incrementing
      }
    } else if (streams[i].end_visible > minimum_stream_length) {
      if (((float) rand() / RAND_MAX) < STREAM_SHUTDOWN_RATE) {
        streams[i].start_visible++; // begin start of window incrementing
      }
    }

  }
}

void shift_visible_window(int w, int h, unsigned long frame_count) {
  for (int i = 0; i < w; i++) {
    bool trace = (frame_count % streams[i].scroll_rate) == 0;
    if (trace) {
      if ((streams[i].end_visible > -1) && (streams[i].end_visible <= h)) {
        streams[i].end_visible++;
      }
      if ((streams[i].start_visible > -1) && (streams[i].start_visible <= h)) {
        streams[i].start_visible++;
      }
    }

    if ((streams[i].start_visible > h) && (streams[i].end_visible > h)) {
      streams[i].start_visible = -1;
      streams[i].end_visible = -1;
    }
  }
}
