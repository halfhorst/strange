#include "lua_screensaver.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#define BUFFER_METATABLE "strange.buffer"

static struct {
  lua_State *lua;
  int table_reference;
  int state_reference;
  char *name;
  struct strange_screensaver_descriptor descriptor;
} loaded = {NULL, LUA_NOREF, LUA_NOREF, NULL, {NULL, 0, NULL, NULL, NULL}};

static void set_error(char *buffer, size_t buffer_size, const char *format, ...) {
  va_list args;

  if (buffer == NULL || buffer_size == 0) {
    return;
  }

  va_start(args, format);
  vsnprintf(buffer, buffer_size, format, args);
  va_end(args);
}

static struct ScreenBuffer *check_buffer(lua_State *lua) {
  struct ScreenBuffer **handle = luaL_checkudata(lua, 1, BUFFER_METATABLE);

  if (*handle == NULL) {
    luaL_error(lua, "buffer used outside the call it was passed to");
  }

  return *handle;
}

static int buffer_write(lua_State *lua) {
  struct ScreenBuffer *buffer = check_buffer(lua);
  size_t length = 0;
  const char *chars = luaL_checklstring(lua, 2, &length);
  int x = (int)luaL_checkinteger(lua, 3);
  int y = (int)luaL_checkinteger(lua, 4);

  luaL_argcheck(lua, length <= (size_t)buffer->character_width, 2,
                "longer than character_width bytes");
  write_to_buffer(buffer, chars, (int)length, x, y);
  return 0;
}

static int buffer_write_string(lua_State *lua) {
  struct ScreenBuffer *buffer = check_buffer(lua);
  const char *text = luaL_checkstring(lua, 2);
  int x = (int)luaL_checkinteger(lua, 3);
  int y = (int)luaL_checkinteger(lua, 4);

  write_string_to_buffer(buffer, text, x, y);
  return 0;
}

static int buffer_clear(lua_State *lua) {
  strange_screen_buffer_clear(check_buffer(lua));
  return 0;
}

static int buffer_index(lua_State *lua) {
  struct ScreenBuffer *buffer = check_buffer(lua);
  const char *key = luaL_checkstring(lua, 2);

  if (strcmp(key, "w") == 0) {
    lua_pushinteger(lua, buffer->w);
  } else if (strcmp(key, "h") == 0) {
    lua_pushinteger(lua, buffer->h);
  } else if (strcmp(key, "character_width") == 0) {
    lua_pushinteger(lua, buffer->character_width);
  } else {
    lua_getfield(lua, lua_upvalueindex(1), key);
  }

  return 1;
}

static void register_buffer_type(lua_State *lua) {
  static const luaL_Reg methods[] = {
      {"write", buffer_write},
      {"write_string", buffer_write_string},
      {"clear", buffer_clear},
      {NULL, NULL},
  };

  luaL_newmetatable(lua, BUFFER_METATABLE);
  luaL_newlib(lua, methods);
  lua_pushcclosure(lua, buffer_index, 1);
  lua_setfield(lua, -2, "__index");
  lua_pop(lua, 1);
}

static struct ScreenBuffer **push_buffer(lua_State *lua,
                                         struct ScreenBuffer *buffer) {
  struct ScreenBuffer **handle = lua_newuserdata(lua, sizeof(*handle));

  *handle = buffer;
  luaL_setmetatable(lua, BUFFER_METATABLE);
  return handle;
}

// Pushes the named function from the script's table, or returns 0 if the
// script does not define it.
static int push_callback(lua_State *lua, const char *field) {
  lua_rawgeti(lua, LUA_REGISTRYINDEX, loaded.table_reference);
  lua_getfield(lua, -1, field);
  lua_remove(lua, -2);
  if (lua_isfunction(lua, -1)) {
    return 1;
  }

  lua_pop(lua, 1);
  return 0;
}

static int call_callback(lua_State *lua, const char *field, int argument_count,
                         int result_count) {
  if (lua_pcall(lua, argument_count, result_count, 0) == LUA_OK) {
    return 0;
  }

  strange_screensaver_set_error("%s in %s: %s", field, loaded.name,
                                lua_tostring(lua, -1));
  lua_pop(lua, 1);
  return -1;
}

static int lua_screensaver_init(void **state, struct ScreenBuffer *buffer) {
  lua_State *lua = loaded.lua;
  struct ScreenBuffer **handle = NULL;
  int result = 0;

  if (lua == NULL || state == NULL) {
    return -1;
  }

  *state = &loaded;
  if (!push_callback(lua, STRANGE_LUA_DESCRIPTOR_INIT_FIELD)) {
    return 0;
  }

  handle = push_buffer(lua, buffer);
  result = call_callback(lua, STRANGE_LUA_DESCRIPTOR_INIT_FIELD, 1, 1);
  *handle = NULL;
  if (result == 0) {
    loaded.state_reference = luaL_ref(lua, LUA_REGISTRYINDEX);
  }

  return result;
}

static int lua_screensaver_update(
    void *state, struct ScreenBuffer *buffer,
    const struct strange_screensaver_frame *frame) {
  lua_State *lua = loaded.lua;
  struct ScreenBuffer **handle = NULL;
  int result = 0;

  (void)state;
  if (lua == NULL) {
    return -1;
  }
  if (!push_callback(lua, STRANGE_LUA_DESCRIPTOR_UPDATE_FIELD)) {
    return 0;
  }

  lua_rawgeti(lua, LUA_REGISTRYINDEX, loaded.state_reference);
  handle = push_buffer(lua, buffer);
  lua_createtable(lua, 0, 2);
  if (frame != NULL) {
    lua_pushinteger(lua, (lua_Integer)frame->frame_count);
    lua_setfield(lua, -2, "frame_count");
    if (frame->now != NULL) {
      lua_pushnumber(lua, (lua_Number)frame->now->tv_sec +
                              (lua_Number)frame->now->tv_nsec / 1e9);
      lua_setfield(lua, -2, "time");
    }
  }

  result = call_callback(lua, STRANGE_LUA_DESCRIPTOR_UPDATE_FIELD, 3, 0);
  *handle = NULL;
  return result;
}

static void lua_screensaver_cleanup(void *state) {
  lua_State *lua = loaded.lua;

  (void)state;
  if (lua == NULL) {
    return;
  }

  if (push_callback(lua, STRANGE_LUA_DESCRIPTOR_CLEANUP_FIELD)) {
    lua_rawgeti(lua, LUA_REGISTRYINDEX, loaded.state_reference);
    call_callback(lua, STRANGE_LUA_DESCRIPTOR_CLEANUP_FIELD, 1, 0);
  }

  luaL_unref(lua, LUA_REGISTRYINDEX, loaded.state_reference);
  loaded.state_reference = LUA_NOREF;
}

static char *copy_string(const char *text) {
  size_t length = strlen(text) + 1;
  char *copy = malloc(length);

  if (copy != NULL) {
    memcpy(copy, text, length);
  }

  return copy;
}

// Reads the name and character width from the script's table at the top of
// the stack.
static int read_table_fields(lua_State *lua, const char *path,
                             const char *default_name, char *error_buffer,
                             size_t error_buffer_size) {
  lua_Integer character_width = 1;
  const char *name = default_name;

  lua_getfield(lua, -1, STRANGE_LUA_DESCRIPTOR_NAME_FIELD);
  if (lua_type(lua, -1) == LUA_TSTRING) {
    name = lua_tostring(lua, -1);
  } else if (!lua_isnil(lua, -1)) {
    set_error(error_buffer, error_buffer_size, "%s: `name` must be a string",
              path);
    return -1;
  }
  if (name == NULL || name[0] == '\0') {
    set_error(error_buffer, error_buffer_size, "%s: missing `name`", path);
    return -1;
  }
  loaded.name = copy_string(name);
  lua_pop(lua, 1);
  if (loaded.name == NULL) {
    set_error(error_buffer, error_buffer_size, "out of memory");
    return -1;
  }

  lua_getfield(lua, -1, STRANGE_LUA_DESCRIPTOR_CHARACTER_WIDTH_FIELD);
  if (!lua_isnil(lua, -1)) {
    int is_integer = 0;

    character_width = lua_tointegerx(lua, -1, &is_integer);
    if (!is_integer || character_width < 1 || character_width > 16) {
      set_error(error_buffer, error_buffer_size,
                "%s: `character_width` must be an integer from 1 to 16", path);
      return -1;
    }
  }
  lua_pop(lua, 1);

  loaded.descriptor.name = loaded.name;
  loaded.descriptor.character_width = (int)character_width;
  loaded.descriptor.init = lua_screensaver_init;
  loaded.descriptor.update = lua_screensaver_update;
  loaded.descriptor.cleanup = lua_screensaver_cleanup;
  return 0;
}

void strange_lua_screensaver_unload(void) {
  if (loaded.lua != NULL) {
    lua_close(loaded.lua);
  }

  free(loaded.name);
  memset(&loaded, 0, sizeof(loaded));
  loaded.table_reference = LUA_NOREF;
  loaded.state_reference = LUA_NOREF;
}

int strange_lua_screensaver_load(
    const char *path, const char *default_name,
    const struct strange_screensaver_descriptor **descriptor,
    char *error_buffer, size_t error_buffer_size) {
  lua_State *lua = NULL;

  if (path == NULL || descriptor == NULL) {
    set_error(error_buffer, error_buffer_size, "invalid Lua screensaver request");
    return -1;
  }

  *descriptor = NULL;
  strange_lua_screensaver_unload();

  lua = luaL_newstate();
  if (lua == NULL) {
    set_error(error_buffer, error_buffer_size, "out of memory");
    return -1;
  }
  loaded.lua = lua;
  luaL_openlibs(lua);
  register_buffer_type(lua);

  if (luaL_loadfile(lua, path) != LUA_OK || lua_pcall(lua, 0, 1, 0) != LUA_OK) {
    set_error(error_buffer, error_buffer_size, "%s", lua_tostring(lua, -1));
    strange_lua_screensaver_unload();
    return -1;
  }
  if (!lua_istable(lua, -1)) {
    set_error(error_buffer, error_buffer_size, "%s: must return a table", path);
    strange_lua_screensaver_unload();
    return -1;
  }
  if (read_table_fields(lua, path, default_name, error_buffer,
                        error_buffer_size) == -1) {
    strange_lua_screensaver_unload();
    return -1;
  }

  loaded.table_reference = luaL_ref(lua, LUA_REGISTRYINDEX);
  *descriptor = &loaded.descriptor;
  return 0;
}
