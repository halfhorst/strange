#ifndef STRANGE_LUA_SCREENSAVER_H_
#define STRANGE_LUA_SCREENSAVER_H_

#include <stddef.h>

#include "screensaver_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
  Run the script at `path` and wrap the table it returns in a descriptor. The
  table may hold these fields, all optional:

    name             string, defaults to `default_name`
    character_width  integer, defaults to 1
    init             function(buffer) -> state
    update           function(state, buffer, frame)
    cleanup          function(state)

  `buffer` mirrors struct ScreenBuffer and the C drawing calls, with the same
  zero-based coordinates:

    buffer.w, buffer.h, buffer.character_width
    buffer:write(chars, x, y)
    buffer:write_string(text, x, y)
    buffer:clear()

  It is only usable during the call it was passed to. `frame` holds
  `frame_count` and `time`, the monotonic clock in seconds.

  One script is held at a time: loading another releases the previous one and
  its descriptor.
*/
int strange_lua_screensaver_load(
    const char *path, const char *default_name,
    const struct strange_screensaver_descriptor **descriptor,
    char *error_buffer, size_t error_buffer_size);
void strange_lua_screensaver_unload(void);

#ifdef __cplusplus
}
#endif

#endif
