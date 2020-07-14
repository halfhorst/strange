/*
  A scrolling DNA visualizer, partly inspired by the Denabase from Bladerunner
  2049.

  Bladerunner 2049 contained several cool scenes showing results from a DNA
  database named "Denabase" put together by territory studio. This demo is
  partially inspired by those scenes.

  This demo is split in half. On the right, a DNA helix scrolls upward. On the
  left, a nucleobase block reveals a larger region of the DNA, scrolling in
  time with the helix. The DNA sequence is generated at random.

  The DNA helix is constructed from two sinusoids, one mirrored and offset
  from the other. Those two sinusoids are parametrized by a single parameter,
  as well as a step function that defines where linkages occur. That single
  parameter is tied to the frame count to progress the strand.

  TODO: Index the same sequence with the DNA block and the helix. For now they
        just appear related.
  TODO: A writeup on how to flatten the 3D helix into 2D
  TODO: Use a sequence from a real organizm.
*/
#include <stdbool.h>

#include "../renderer.h"

#ifndef DENABASE_H_
#define DENABASE_H_

#define DENABASE_CHAR_WIDTH 1

void denabase_init(struct ScreenBuffer *sbuffer);
bool denabase_update(struct ScreenBuffer *sbuffer, unsigned long frame_count);
void denabase_cleanup(void);

#endif  // DENABASE_H_
