#include "screensaver_registry.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "demos/denabase.h"
#include "demos/digital_rain.h"

static const struct strange_screensaver_descriptor *const builtin_descriptors[] =
    {
        &strange_denabase_descriptor,
        &strange_digital_rain_descriptor,
};

static void set_error(char *buffer, size_t buffer_size, const char *format, ...) {
  va_list args;

  if (buffer == NULL || buffer_size == 0) {
    return;
  }

  va_start(args, format);
  vsnprintf(buffer, buffer_size, format, args);
  va_end(args);
}

static const char *native_extension(void) {
#ifdef __APPLE__
  return ".dylib";
#else
  return ".so";
#endif
}

static int ensure_catalog_capacity(struct strange_screensaver_catalog *catalog,
                                   size_t minimum_capacity) {
  struct strange_screensaver_record *records = NULL;
  size_t new_capacity = 0;

  if (catalog == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (catalog->record_capacity >= minimum_capacity) {
    return 0;
  }

  new_capacity = catalog->record_capacity == 0 ? 4 : catalog->record_capacity;
  while (new_capacity < minimum_capacity) {
    new_capacity *= 2;
  }

  records = realloc(catalog->records, new_capacity * sizeof(*records));
  if (records == NULL) {
    return -1;
  }

  catalog->records = records;
  catalog->record_capacity = new_capacity;
  return 0;
}

static void free_record(struct strange_screensaver_record *record) {
  if (record == NULL) {
    return;
  }

  if (record->source != STRANGE_SCREENSAVER_RECORD_SOURCE_BUILTIN) {
    free((char *)record->name);
    free((char *)record->path);
  }

  memset(record, 0, sizeof(*record));
}

static int append_builtin_records(struct strange_screensaver_catalog *catalog) {
  size_t count = 0;
  const struct strange_screensaver_descriptor *const *descriptors =
      strange_builtin_screensavers(&count);

  if (ensure_catalog_capacity(catalog, count) == -1) {
    return -1;
  }

  for (size_t index = 0; index < count; ++index) {
    catalog->records[index].name = descriptors[index]->name;
    catalog->records[index].source = STRANGE_SCREENSAVER_RECORD_SOURCE_BUILTIN;
    catalog->records[index].descriptor = descriptors[index];
    catalog->records[index].path = NULL;
  }

  catalog->record_count = count;
  return 0;
}

static int append_user_record(struct strange_screensaver_catalog *catalog,
                              const char *name,
                              enum strange_screensaver_record_source source,
                              const char *path) {
  struct strange_screensaver_record *record = NULL;

  if (catalog == NULL || name == NULL || path == NULL ||
      source == STRANGE_SCREENSAVER_RECORD_SOURCE_BUILTIN) {
    errno = EINVAL;
    return -1;
  }

  if (ensure_catalog_capacity(catalog, catalog->record_count + 1) == -1) {
    return -1;
  }

  record = &catalog->records[catalog->record_count];
  memset(record, 0, sizeof(*record));
  record->name = strdup(name);
  if (record->name == NULL) {
    return -1;
  }

  record->path = strdup(path);
  if (record->path == NULL) {
    free_record(record);
    return -1;
  }

  record->source = source;
  catalog->record_count += 1;
  return 0;
}

static const struct strange_screensaver_record *find_user_record(
    const struct strange_screensaver_catalog *catalog, const char *name) {
  if (catalog == NULL || name == NULL || name[0] == '\0') {
    return NULL;
  }

  for (size_t index = 0; index < catalog->record_count; ++index) {
    const struct strange_screensaver_record *record = &catalog->records[index];
    if (record->source != STRANGE_SCREENSAVER_RECORD_SOURCE_BUILTIN &&
        strcmp(record->name, name) == 0) {
      return record;
    }
  }

  return NULL;
}

static char *join_path(const char *left, const char *right) {
  size_t left_length = 0;
  size_t right_length = 0;
  size_t needs_separator = 0;
  char *path = NULL;

  if (left == NULL || right == NULL) {
    errno = EINVAL;
    return NULL;
  }

  left_length = strlen(left);
  right_length = strlen(right);
  needs_separator = left_length > 0 && left[left_length - 1] != '/';
  path = malloc(left_length + needs_separator + right_length + 1);
  if (path == NULL) {
    return NULL;
  }

  memcpy(path, left, left_length);
  if (needs_separator != 0) {
    path[left_length] = '/';
    left_length += 1;
  }
  memcpy(path + left_length, right, right_length);
  path[left_length + right_length] = '\0';
  return path;
}

static int classify_user_artifact(
    const char *filename, enum strange_screensaver_record_source *source,
    char **name) {
  const char *extension = NULL;
  size_t filename_length = 0;
  size_t extension_length = 0;
  size_t name_length = 0;

  if (filename == NULL || source == NULL || name == NULL) {
    errno = EINVAL;
    return -1;
  }

  *name = NULL;
  filename_length = strlen(filename);
  if (filename_length == 0) {
    return 1;
  }

  extension = ".lua";
  extension_length = strlen(extension);
  if (filename_length > extension_length &&
      strcmp(filename + filename_length - extension_length, extension) == 0) {
    *source = STRANGE_SCREENSAVER_RECORD_SOURCE_LUA;
    name_length = filename_length - extension_length;
  } else {
    extension = native_extension();
    extension_length = strlen(extension);
    if (filename_length > extension_length &&
        strcmp(filename + filename_length - extension_length, extension) == 0) {
      *source = STRANGE_SCREENSAVER_RECORD_SOURCE_DYNAMIC;
      name_length = filename_length - extension_length;
    } else {
      return 1;
    }
  }

  if (name_length == 0) {
    return 1;
  }

  *name = malloc(name_length + 1);
  if (*name == NULL) {
    return -1;
  }
  memcpy(*name, filename, name_length);
  (*name)[name_length] = '\0';
  return 0;
}

static int discover_user_screensavers(struct strange_screensaver_catalog *catalog,
                                      char *error_buffer,
                                      size_t error_buffer_size) {
  DIR *directory = NULL;
  struct dirent *entry = NULL;
  const char *home = getenv("HOME");
  char *screensaver_dir = NULL;
  int result = 0;

  if (catalog == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (home == NULL || home[0] == '\0') {
    return 0;
  }

  screensaver_dir = join_path(home, ".strange");
  if (screensaver_dir == NULL) {
    return -1;
  }

  directory = opendir(screensaver_dir);
  if (directory == NULL) {
    if (errno == ENOENT || errno == ENOTDIR) {
      free(screensaver_dir);
      return 0;
    }

    set_error(error_buffer, error_buffer_size,
              "failed to read screensaver directory: %s", screensaver_dir);
    free(screensaver_dir);
    return -1;
  }

  while ((entry = readdir(directory)) != NULL) {
    struct stat entry_stat = {0};
    char *path = NULL;
    char *name = NULL;
    const struct strange_screensaver_record *existing = NULL;
    enum strange_screensaver_record_source source =
        STRANGE_SCREENSAVER_RECORD_SOURCE_BUILTIN;
    int classify_result = 0;

    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    path = join_path(screensaver_dir, entry->d_name);
    if (path == NULL) {
      result = -1;
      break;
    }

    if (stat(path, &entry_stat) == -1) {
      set_error(error_buffer, error_buffer_size,
                "failed to inspect screensaver artifact: %s", path);
      free(path);
      result = -1;
      break;
    }

    if (!S_ISREG(entry_stat.st_mode)) {
      free(path);
      continue;
    }

    classify_result = classify_user_artifact(entry->d_name, &source, &name);
    if (classify_result == 1) {
      free(path);
      continue;
    }
    if (classify_result == -1) {
      free(path);
      result = -1;
      break;
    }

    existing = find_user_record(catalog, name);
    if (existing != NULL && existing->source != source) {
      set_error(error_buffer, error_buffer_size,
                "conflicting user screensaver artifacts for `%s`: %s and %s",
                name, existing->path, path);
      free(name);
      free(path);
      result = -1;
      break;
    }

    if (append_user_record(catalog, name, source, path) == -1) {
      free(name);
      free(path);
      result = -1;
      break;
    }

    free(name);
    free(path);
  }

  closedir(directory);
  free(screensaver_dir);
  return result;
}

int strange_screensaver_descriptor_validate(
    const struct strange_screensaver_descriptor *descriptor) {
  if (descriptor == NULL || descriptor->name == NULL ||
      descriptor->name[0] == '\0') {
    errno = EINVAL;
    return -1;
  }

  return 0;
}

int strange_screensaver_character_width(
    const struct strange_screensaver_descriptor *descriptor) {
  if (descriptor == NULL || descriptor->character_width < 1) {
    return 1;
  }

  return descriptor->character_width;
}

int strange_screensaver_instance_init(
    struct strange_screensaver_instance *instance,
    const struct strange_screensaver_descriptor *descriptor,
    struct ScreenBuffer *buffer) {
  void *state = NULL;

  if (instance == NULL ||
      strange_screensaver_descriptor_validate(descriptor) == -1 ||
      (descriptor->init != NULL && buffer == NULL)) {
    errno = EINVAL;
    return -1;
  }

  memset(instance, 0, sizeof(*instance));
  if (descriptor->init != NULL && descriptor->init(&state, buffer) == -1) {
    return -1;
  }

  instance->descriptor = descriptor;
  instance->state = state;
  return 0;
}

int strange_screensaver_instance_update(
    struct strange_screensaver_instance *instance, struct ScreenBuffer *buffer,
    const struct strange_screensaver_frame *frame) {
  if (instance == NULL || instance->descriptor == NULL || buffer == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (instance->descriptor->update == NULL) {
    return 0;
  }

  return instance->descriptor->update(instance->state, buffer, frame);
}

void strange_screensaver_instance_cleanup(
    struct strange_screensaver_instance *instance) {
  if (instance == NULL) {
    return;
  }

  if (instance->descriptor != NULL && instance->descriptor->cleanup != NULL) {
    instance->descriptor->cleanup(instance->state);
  }

  instance->descriptor = NULL;
  instance->state = NULL;
}

int strange_screensaver_catalog_init(struct strange_screensaver_catalog *catalog,
                                     char *error_buffer,
                                     size_t error_buffer_size) {
  if (catalog == NULL) {
    errno = EINVAL;
    return -1;
  }

  memset(catalog, 0, sizeof(*catalog));
  if (append_builtin_records(catalog) == -1) {
    strange_screensaver_catalog_free(catalog);
    return -1;
  }

  if (discover_user_screensavers(catalog, error_buffer, error_buffer_size) ==
      -1) {
    strange_screensaver_catalog_free(catalog);
    return -1;
  }

  return 0;
}

void strange_screensaver_catalog_free(struct strange_screensaver_catalog *catalog) {
  if (catalog == NULL) {
    return;
  }

  for (size_t index = 0; index < catalog->record_count; ++index) {
    free_record(&catalog->records[index]);
  }

  free(catalog->records);
  memset(catalog, 0, sizeof(*catalog));
}

const struct strange_screensaver_record *strange_screensaver_catalog_records(
    const struct strange_screensaver_catalog *catalog, size_t *count) {
  if (count != NULL) {
    *count = catalog != NULL ? catalog->record_count : 0;
  }

  return catalog != NULL ? catalog->records : NULL;
}

const struct strange_screensaver_record *strange_screensaver_catalog_find(
    const struct strange_screensaver_catalog *catalog, const char *name) {
  if (catalog == NULL || name == NULL || name[0] == '\0') {
    return NULL;
  }

  for (size_t index = catalog->record_count; index > 0; --index) {
    const struct strange_screensaver_record *record =
        &catalog->records[index - 1];
    if (strcmp(record->name, name) == 0) {
      return record;
    }
  }

  return NULL;
}

const struct strange_screensaver_descriptor *const *strange_builtin_screensavers(
    size_t *count) {
  if (count != NULL) {
    *count = sizeof(builtin_descriptors) / sizeof(builtin_descriptors[0]);
  }

  return builtin_descriptors;
}

const struct strange_screensaver_descriptor *strange_builtin_screensaver_find(
    const char *name) {
  size_t count = 0;
  const struct strange_screensaver_descriptor *const *descriptors =
      strange_builtin_screensavers(&count);

  if (name == NULL || name[0] == '\0') {
    return NULL;
  }

  for (size_t index = 0; index < count; ++index) {
    if (strcmp(descriptors[index]->name, name) == 0) {
      return descriptors[index];
    }
  }

  return NULL;
}
