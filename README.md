# Strange

Something entertaining to put on your terminal when you aren't doing anything
with it: a terminal screensaver.

> They went to the living room; Jill sat at his feet and they applied
> themselves to martinis. Opposite his chair was a stereovision tank disguised
> as an aquarium; he switched it on, guppies and tetras gave way to the face
> of the well-known Winchell Augustus Greaves."
> -- _Stranger in a Strange Land_, Robert Heinlein

Right now, `strange` doesn't actually function as a screensaver, meaning it
won't spring in to action after a period of inactivity. Of course, a screensaver 
isn't necessary in most places today. Phosphor burn-in isn't a risk. Still, they 
are nostalgic and fun. I often "turn on" something on my second monitor that is 
calming and visual. `strange` fits that role.
  
## Demos

Specific screensA strangeland demo impelement three functions. An initialization function that
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

### Platform Support

Right now, `strange` is unix-specific. MacOS and Windows support is planned but
there is no timeline, sorry!


## Similar Projects

`allogic` has a [similar project](https://github.com/allogic/rterm) called
rterm. It was helpful to look at and the raymarched scene is really nice.

https://github.com/TimeToogo/remote-pty
https://github.com/Rezmason/matrix

clock info: https://stackoverflow.com/questions/12392278/measure-time-in-linux-time-vs-clock-vs-getrusage-vs-clock-gettime-vs-gettimeof

## BUGS
- [ ] speeds up if screen is smaller?
- [ ] screen not cleared after ctrl + c



## Future Work
- [ ] pty screensaver functionality
- [ ] FPS measurement
- [ ] plugin system for screensavers
    [ref](https://eli.thegreenplace.net/2012/08/24/plugins-in-c)
- [ ] scripting system for screensavers
- more scenes
    - [ ] SDF raymarching?
        [SDF lib](https://mercury.sexy/hg_sdf/)
    - [ ] metaballs
- [ ] add gifs to README
- [ ] Meson build system?