/*
  A scrolling DNA visualizer, partly inspired by the Denabase from Bladerunner
  2049.

  Bladerunner 2049 contained several cool scenes showing results from a DNA
  database named "Denabase" put together by territory studio. This demo is
  partially inspired by those scenes.

  This demo is split in half. On the right, a DNA helix scrolls upward. On the
  left, a table shows the sequence around the stretch the helix is passing
  through. The DNA sequence is generated at random.

  The two halves read the same sequence. Each base pair is a rung of the
  helix, labelled with the base on one strand and its complement on the other.
  The rung level with the middle of the screen is the current base: the table
  keeps the row holding it in its focus row and marks its column. When the
  helix has scrolled through every base of that row, the table moves up a row.

  The helix is two sinusoids over the screen row, the second mirrored and a
  little ahead in phase, which is what gives the wide and narrow grooves.

  TODO: A writeup on how to flatten the 3D helix into 2D
  TODO: Use a sequence from a real organizm.
*/
#include "../screensaver_registry.h"

#ifndef DENABASE_H_
#define DENABASE_H_

#define DENABASE_CHAR_WIDTH 1

extern const struct strange_screensaver_descriptor strange_denabase_descriptor;

#endif  // DENABASE_H_
