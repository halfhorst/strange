# Strange Land

Something entertaining to put on your terminal when you aren't doing anything
with it. I'd forgotten about screensavers, to be honest, until I read this
passage:

> They went to the living room; Jill sat at his feet and they applied
> themselves to martinis. Opposite his chair was a stereovision tank disguised
> as an aquarium; he switched it on, guppies and tetras gave way to the face of
> the well-known Winchell Augustus Greaves." 
> -- _Stranger in a Strange Land_, Robert Heinlein

Of course, a screensaver isn't necessary in most places today. Modern screens
don't use technology that suffers from phosphor burn-in. Still, they are
nostalgic and fun, so I wanted to bring them back. I often "turn on" something
on my second monitor that is calming and visual. `strange` fits that role.

## Runtime

`strange` is a PTY-backed terminal wrapper. Run it as the terminal session you
want to monitor. It starts a child shell, forwards input and output during
normal use, and starts a screensaver after a period of inactivity. Any key
wakes it and restores the screen; `Ctrl-Q` disables it for the session.

    strange digital_rain
    strange --timeout 120 denabase
    strange --random denabase digital_rain
    strange --now wave
    strange --list

Only keystrokes count as activity, so output from a running job neither wakes
the screensaver nor holds it off; whatever was printed meanwhile appears on
waking. A full-screen program such as `vim`, `less` or `top` does hold it off
while it runs. Pass `--cover-fullscreen` to start over those too.

`strange --status` says whether the current terminal is running under
`strange` and with what screensaver and timeout, and exits 0 if it is. The
wrapped shell also gets `STRANGE_TTY`, `STRANGE_SCREENSAVER`, `STRANGE_TIMEOUT`
and `STRANGE_COVER_FULLSCREEN`, which describe how the session started.
`strange` refuses to start inside a terminal it is already wrapping, so a shell
startup file can launch it with:

    strange --status >/dev/null || exec strange digital_rain

`--now` runs only the screensaver, with no shell underneath, and exits on any
key. It is the quick way to try one while writing it: if the screensaver
fails, the reason is printed and the exit status is 1.

## Writing screensavers

`strange --list` shows the built-in screensavers and any found in
`~/.strange/`. A file there named `NAME.lua`, or `NAME.dylib` on macOS and
`NAME.so` elsewhere, is run with `strange NAME` and takes precedence over a
built-in of the same name.

A screensaver draws into a buffer of character cells, `w` wide and `h` high,
once per frame at about 60 FPS. Each cell holds up to `character_width` bytes,
so set that to 3 or 4 to draw UTF-8 characters. Coordinates start at zero in
the top left. Only cells that change between frames are sent to the terminal.

### Lua

A script returns a table. Every field is optional.

```lua
return {
  name = "hello",
  character_width = 1,
  init = function(buffer) return { column = 0 } end,
  update = function(state, buffer, frame)
    state.column = (state.column + 1) % buffer.w
    buffer:write_string("hello", state.column, buffer.h // 2)
  end,
  cleanup = function(state) end,
}
```

`buffer` has the fields `w`, `h` and `character_width` and the methods
`write(chars, x, y)` for one cell, `write_string(text, x, y)` for a run of
single-byte cells, and `clear()`. `frame` has `frame_count` and `time`, a
monotonic clock in seconds. See `examples/wave.lua`.

### C

A shared library exports a `struct strange_screensaver_descriptor` named
`strange_screensaver_descriptor`, declared in `src/screensaver_registry.h`
along with the drawing calls in `src/renderer.h`. Its `api_version` must be
`STRANGE_SCREENSAVER_API_VERSION`; a library built against a different version
is refused with a message asking for a rebuild. `make examples` builds
`examples/bounce.c`, which shows the whole shape.

A callback that returns -1, or a Lua error, turns the screensaver off for the
rest of the session and prints the reason; the shell carries on.

## Building

Run `make` to build `strange` and `make test` to run the tests, which need
GoogleTest. The code is C99 and uses POSIX.1-2008 plus `SIGWINCH` and the
`TIOCGWINSZ`/`TIOCSWINSZ` ioctls. The Makefile needs GNU make. Lua 5.4 is
built from the sources in `third_party/`.

## Similar Projects

`allogic` has a [similar project](https://github.com/allogic/rterm) called
rterm. It was helpful to look at and the raymarched scene is really nice.

## TODO:
* Implement a few more scenes.
    * SDF Rotating Cube
    * Metaballs
* Reload a screensaver when its file changes
* Optional watermark in the terminal to know you are in "screensaver mode"
