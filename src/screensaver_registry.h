#ifndef STRANGE_SCREENSAVER_REGISTRY_H_
#define STRANGE_SCREENSAVER_REGISTRY_H_

#include <stddef.h>
#include <time.h>

#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
  Identifies the layout of the descriptor, the frame and the screen buffer and
  the meaning of the callbacks. It changes whenever a library built against the
  old ones would no longer work.
*/
#define STRANGE_SCREENSAVER_API_VERSION 1

#define STRANGE_DYNAMIC_DESCRIPTOR_SYMBOL "strange_screensaver_descriptor"
#define STRANGE_LUA_DESCRIPTOR_NAME_FIELD "name"
#define STRANGE_LUA_DESCRIPTOR_CHARACTER_WIDTH_FIELD "character_width"
#define STRANGE_LUA_DESCRIPTOR_INIT_FIELD "init"
#define STRANGE_LUA_DESCRIPTOR_UPDATE_FIELD "update"
#define STRANGE_LUA_DESCRIPTOR_CLEANUP_FIELD "cleanup"

struct strange_screensaver_frame {
  const struct timespec *now;
  unsigned long frame_count;
};

typedef int (*strange_screensaver_init_fn)(void **state,
                                           struct ScreenBuffer *buffer);
typedef int (*strange_screensaver_update_fn)(
    void *state, struct ScreenBuffer *buffer,
    const struct strange_screensaver_frame *frame);
typedef void (*strange_screensaver_cleanup_fn)(void *state);

struct strange_screensaver_descriptor {
  int api_version;  // always first, so it can be read whatever follows
  const char *name;
  int character_width;
  strange_screensaver_init_fn init;
  strange_screensaver_update_fn update;
  strange_screensaver_cleanup_fn cleanup;
};

struct strange_screensaver_instance {
  const struct strange_screensaver_descriptor *descriptor;
  void *state;
};

enum strange_screensaver_record_source {
  STRANGE_SCREENSAVER_RECORD_SOURCE_BUILTIN = 0,
  STRANGE_SCREENSAVER_RECORD_SOURCE_LUA,
  STRANGE_SCREENSAVER_RECORD_SOURCE_DYNAMIC,
};

struct strange_screensaver_record {
  const char *name;
  enum strange_screensaver_record_source source;
  const struct strange_screensaver_descriptor *descriptor;
  const char *path;
};

struct strange_screensaver_catalog {
  struct strange_screensaver_record *records;
  size_t record_count;
  size_t record_capacity;
};

/*
  Native shared libraries export a descriptor with the name in
  STRANGE_DYNAMIC_DESCRIPTOR_SYMBOL. They set `api_version` to
  STRANGE_SCREENSAVER_API_VERSION and are refused if it differs from the
  version strange was built with. Besides that only `name` is required, and
  `character_width` defaults to 1 when it is left at zero. Lua screensavers
  return a table using the field names above; see lua_screensaver.h.

  Callbacks return -1 to report failure, which ends the screensaver for the
  session. They can describe the failure with strange_screensaver_set_error.
*/
void strange_screensaver_set_error(const char *format, ...);
const char *strange_screensaver_error(void);
int strange_screensaver_descriptor_validate(
    const struct strange_screensaver_descriptor *descriptor);
int strange_screensaver_character_width(
    const struct strange_screensaver_descriptor *descriptor);
int strange_screensaver_instance_init(
    struct strange_screensaver_instance *instance,
    const struct strange_screensaver_descriptor *descriptor,
    struct ScreenBuffer *buffer);
int strange_screensaver_instance_update(
    struct strange_screensaver_instance *instance, struct ScreenBuffer *buffer,
    const struct strange_screensaver_frame *frame);
void strange_screensaver_instance_cleanup(
    struct strange_screensaver_instance *instance);
int strange_screensaver_catalog_init(struct strange_screensaver_catalog *catalog,
                                     char *error_buffer,
                                     size_t error_buffer_size);
void strange_screensaver_catalog_free(struct strange_screensaver_catalog *catalog);
const struct strange_screensaver_record *strange_screensaver_catalog_records(
    const struct strange_screensaver_catalog *catalog, size_t *count);
const struct strange_screensaver_record *strange_screensaver_catalog_find(
    const struct strange_screensaver_catalog *catalog, const char *name);
const struct strange_screensaver_descriptor *const *strange_builtin_screensavers(
    size_t *count);
const struct strange_screensaver_descriptor *strange_builtin_screensaver_find(
    const char *name);

#ifdef __cplusplus
}
#endif

#endif
