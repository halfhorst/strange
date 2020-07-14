# Strange Land

Something entertaining to put on your terminal when you aren't doing anything
with it. Inspired by a daydream of a terminal screensaver.

> They went to the living room; Jill sat at his feet and they applied
> themselves to martinis. Opposite his chair was a stereovision tank disguised
> as an aquarium; he switched it on, guppies and tetras gave way to the face
> of the well-known Winchell Augustus Greaves."
> -- _Stranger in a Strange Land_, Robert Heinlein

Right now, `strangeland` doesn't actually function as a screensaver, meaning it
won't spring in to action after a period of inactivity. I can monitor stdin's
tty to get idle time but I haven't figured out the best
fork-into-the-background or daemonization scheme to come back and get access
to the right tty device.

Of course, a screensaver isn't necessary in most places today. Phosphor burn-in
isn't a risk. Still, they are nostalgic and pretty and the graphics are fun. I
often "turn on" something on my second monitor that is calming and visual.
`strangeland` fits that role.

`strangeland` is heavy on Posix and not strictly c99 so it isn't portable like
c99 wants to be. It's performance is also highly dependent on the terminal
emulator you are using and what it supports.

## Demos

A strangeland demo impelement three functions. An initialization function that
runs once at startup and is useful for initializing global state, an update
function that is run once per frame, and a cleanup function that runs on exit
or `SIGTERM`. Return false from the update function to terminate the demo.
Check out `src/render.h` for more info.

Later, an actual plugin system would be nice (or scripting with a niche
language I want to play with, like Janet or Chibi-Scheme). For this prototype
demos have to be hard-coded in.

* denabase: a DNA visualization inspired by Blade Runner 2049.
* digital_rain: An homage to the digital rain from the Matrix, and Ghost in the Shell before it.

## Building

I provide a makefile for building that has targets for each scene. It also
has debug and benchmark targets. Benchmarking compiles for `gprof` and is
useful for identifying bottlenecks in your update function.

## Similar Projects

`allogic` has a [similar project](https://github.com/allogic/rterm) called
rterm. It was helpful to look at and the raymarched scene is really nice.

## TODO:

* Add some gifs to the README
* Implement a few more scenes.
    * Signed distance fields and raymarching
    * metaballs
* Pursue some sort of plugins for demos
* Play with scripting
