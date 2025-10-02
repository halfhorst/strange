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
nostalgic and fun, so I wanted to bring them back.I often "turn on" something
on my second monitor that is calming and visual. `strangeland` fits that role.

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
* digital_rain: An homage to the digital rain from The Matrix and Ghost in the
  Shell.

## Building

I provide a makefile for building that has targets for each scene. It also has
debug and benchmark targets. Benchmarking compiles for `gprof` and is useful
for identifying bottlenecks in your update function.

## Similar Projects

`allogic` has a [similar project](https://github.com/allogic/rterm) called
rterm. It was helpful to look at and the raymarched scene is really nice.

## TODO:
* Implement a few more scenes.
    * SDF Rotating Cube
    * Metaballs
* Add lua bindings and embed an interpreter
* Hot reload screensavers from home directory
    * either screensaver SOs or lua scripts
* Optional watermark in the terminal to know you are in "screensaver mode"
* Watermark during screensaver indicating any key will resume, and special key to quit altogether
