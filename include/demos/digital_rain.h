/*
  The iconic digital rain effect, characteristic of the Matrix and preceded by
  a similar effect in the original Ghost in the Shell movie.

  The effect changed a bit from movie to movie, and has been reproduced many
  times. I prefer the look from the original movie, and it also suits the
  limitations of a terminal bettwe as well. Key features:

  * Less of a rain-like quality, uniform width
  * Characters stream (quite quickly) down from the top of the screen
      - the streams start at random times and scroll at random speeds
      - some streams persist for a long time while others start and stop,
        generating small blank gaps
  * The leading edge of a stream is brighter
  * The characters are half-width katakana and numbers, both normal and
    mirrored. Apparently this was a custom typeface.
  * Individual characters in the streams shuffle randomly.

  This demo is organized around vertical streams of characters that are
  created at initialization. Each stream defines a visible window in terms of
  a start and end index that is rendered during each update. The indices
  are incremented at the same rate, giving a scrolling effect. Characters
  within the region are candidates to be shuffled.

  True to the original effect, the demo renders UTF-8 katakana characters
  (not mirrored) but respects the request for ASCII.

  TODO: Consider trying to switch to row-oriented memory copies
*/
#ifndef DIGITAL_RAIN_H_
#define DIGITAL_RAIN_H_

#include <stdbool.h>
#include <stdint.h>

#include "../renderer.h"

/*
  We render fixed-width katakana. 3 bytes are needed for the UTF-8
  representation.
*/
#define DIGITAL_RAIN_CHAR_WIDTH 3

void digital_rain_init(void);
bool digital_rain_update(struct ScreenBuffer *buffer, uint64_t time, uint32_t dt);
void digital_rain_cleanup(void);

#endif  // DIGITAL_RAIN_H_
