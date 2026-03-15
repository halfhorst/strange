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
normal use, and activates a screensaver path after inactivity.

The renderer code under `src/` is retained as implementation material for the
next integration steps, but it is no longer presented as a supported standalone
runtime.

## Building

Run `make` to build the supported `strange` binary.

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
