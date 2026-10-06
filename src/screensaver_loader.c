#include "screensaver_loader.h"

#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>

#include "lua_screensaver.h"

static void set_error(char *buffer, size_t buffer_size, const char *format, ...) {
  va_list args;

  if (buffer == NULL || buffer_size == 0) {
    return;
  }

  va_start(args, format);
  vsnprintf(buffer, buffer_size, format, args);
  va_end(args);
}

static int load_shared_library(
    const struct strange_screensaver_record *record,
    const struct strange_screensaver_descriptor **descriptor,
    char *error_buffer, size_t error_buffer_size) {
  const struct strange_screensaver_descriptor *loaded = NULL;
  void *library = dlopen(record->path, RTLD_NOW | RTLD_LOCAL);

  if (library == NULL) {
    set_error(error_buffer, error_buffer_size, "failed to load %s: %s",
              record->path, dlerror());
    return -1;
  }

  loaded = dlsym(library, STRANGE_DYNAMIC_DESCRIPTOR_SYMBOL);
  if (loaded == NULL) {
    set_error(error_buffer, error_buffer_size, "%s does not export `%s`",
              record->path, STRANGE_DYNAMIC_DESCRIPTOR_SYMBOL);
    dlclose(library);
    return -1;
  }
  if (strange_screensaver_descriptor_validate(loaded) == -1) {
    set_error(error_buffer, error_buffer_size,
              "%s exports a descriptor without a name", record->path);
    dlclose(library);
    return -1;
  }

  *descriptor = loaded;
  return 0;
}

int strange_screensaver_load(
    const struct strange_screensaver_record *record,
    const struct strange_screensaver_descriptor **descriptor,
    char *error_buffer, size_t error_buffer_size) {
  if (record == NULL || descriptor == NULL) {
    set_error(error_buffer, error_buffer_size, "invalid screensaver record");
    return -1;
  }

  *descriptor = NULL;
  switch (record->source) {
  case STRANGE_SCREENSAVER_RECORD_SOURCE_BUILTIN:
    *descriptor = record->descriptor;
    return 0;
  case STRANGE_SCREENSAVER_RECORD_SOURCE_DYNAMIC:
    return load_shared_library(record, descriptor, error_buffer,
                               error_buffer_size);
  case STRANGE_SCREENSAVER_RECORD_SOURCE_LUA:
    return strange_lua_screensaver_load(record->path, record->name, descriptor,
                                        error_buffer, error_buffer_size);
  }

  set_error(error_buffer, error_buffer_size, "unknown screensaver source");
  return -1;
}
