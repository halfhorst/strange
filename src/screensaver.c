#include "screensaver.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

void init_screensaver(void (*ss_init)(void),
                      bool (*ss_update)(struct ScreenBuffer* sbuffer,
                                        uint64_t time, uint32_t dt),
                      void (*ss_cleanup)(void), u_int16_t character_width,
                      struct ScreenSaver* ret) {
  ret->init = ss_init;
  ret->update = ss_update;
  ret->cleanup = ss_cleanup;
  ret->character_width = character_width;
}
