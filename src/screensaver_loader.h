#ifndef STRANGE_SCREENSAVER_LOADER_H_
#define STRANGE_SCREENSAVER_LOADER_H_

#include <stddef.h>

#include "screensaver_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
  Turn a catalog record into a descriptor that can be run. Built-ins are
  returned as they are, shared libraries are opened and stay loaded for the
  life of the process, and Lua scripts are run to obtain their table.

  The descriptor does not depend on the record or its catalog afterwards.
*/
int strange_screensaver_load(
    const struct strange_screensaver_record *record,
    const struct strange_screensaver_descriptor **descriptor,
    char *error_buffer, size_t error_buffer_size);

#ifdef __cplusplus
}
#endif

#endif
