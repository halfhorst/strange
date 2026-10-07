#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../renderer.h"
#include "./denabase.h"

#define SEQUENCE_LENGTH 100000
#define SEQUENCE_NAME "IDENT #09817 (H. sapiens)"

// Helix geometry. The pitch is in screen rows per radian, and the second
// strand runs ahead of the first by the phase.
#define STRAND_RADIUS 10
#define STRAND_PITCH 4.0
#define STRAND_PHASE 0.625
#define STRAND_MARKER '0'
#define STRAND_SAMPLES_PER_ROW 10
#define ROWS_PER_BASE 2

// discretized rendering makes things look bad at slow speeds
#define FRAMES_PER_ROW 4

struct denabase_state {
  char *sequence;
  long scroll;  // screen rows the helix has moved up by
};

// Where things go for the current buffer size.
struct layout {
  int table_right;
  int bases_per_row;
  int focus_row;
  int helix_center;
  int helix_radius;
  long sequence_rows;
};

static void generate_random_sequence(char *sequence, size_t length);
static char complement(char base);
static int compute_layout(const struct ScreenBuffer *buffer,
                          struct layout *layout);
static void draw_table(const struct denabase_state *state,
                       struct ScreenBuffer *buffer, const struct layout *layout);
static void draw_helix(const struct denabase_state *state,
                       struct ScreenBuffer *buffer, const struct layout *layout);

static int denabase_init(void **state, struct ScreenBuffer *buffer) {
  struct denabase_state *denabase = NULL;

  (void)buffer;
  if (state == NULL) {
    errno = EINVAL;
    return -1;
  }

  denabase = calloc(1, sizeof(*denabase));
  if (denabase == NULL) {
    return -1;
  }
  denabase->sequence = malloc(SEQUENCE_LENGTH);
  if (denabase->sequence == NULL) {
    free(denabase);
    return -1;
  }

  generate_random_sequence(denabase->sequence, SEQUENCE_LENGTH);
  *state = denabase;
  return 0;
}

static int denabase_update(
    void *state, struct ScreenBuffer *sbuffer,
    const struct strange_screensaver_frame *frame) {
  struct denabase_state *denabase = state;
  struct layout layout;

  if (denabase == NULL || sbuffer == NULL) {
    errno = EINVAL;
    return -1;
  }
  if (frame != NULL && (frame->frame_count % FRAMES_PER_ROW) == 0) {
    denabase->scroll++;
  }
  if (compute_layout(sbuffer, &layout) == -1) {
    return 0;
  }

  // The sequence is treated as a loop of whole table rows.
  denabase->scroll %= layout.sequence_rows * layout.bases_per_row * ROWS_PER_BASE;

  draw_helix(denabase, sbuffer, &layout);
  draw_table(denabase, sbuffer, &layout);
  return 0;
}

static void denabase_cleanup(void *state) {
  struct denabase_state *denabase = state;

  if (denabase == NULL) {
    return;
  }

  free(denabase->sequence);
  free(denabase);
}

const struct strange_screensaver_descriptor strange_denabase_descriptor = {
    .api_version = STRANGE_SCREENSAVER_API_VERSION,
    .name = "denabase",
    .character_width = DENABASE_CHAR_WIDTH,
    .init = denabase_init,
    .update = denabase_update,
    .cleanup = denabase_cleanup,
};

static void put(struct ScreenBuffer *buffer, char character, int x, int y) {
  write_to_buffer(buffer, &character, 1, x, y);
}

static long floor_divide(long value, long divisor) {
  long quotient = value / divisor;

  return (value % divisor != 0 && value < 0) ? quotient - 1 : quotient;
}

static long wrap(long value, long modulus) {
  long remainder = value % modulus;

  return remainder < 0 ? remainder + modulus : remainder;
}

// The base pair that the screen row `y` falls on. The one level with the
// focus row is the current base.
static long base_at_row(const struct denabase_state *state,
                        const struct layout *layout, int y) {
  long base = floor_divide(state->scroll + y - layout->focus_row, ROWS_PER_BASE);

  return wrap(base, layout->sequence_rows * layout->bases_per_row);
}

/*
  The table takes the left half: a border, the name, and rows of sequence with
  the focus row set apart in the middle. Returns -1 if the buffer is too small
  to draw anything sensible.
*/
static int compute_layout(const struct ScreenBuffer *buffer,
                          struct layout *layout) {
  int right_half = 0;

  layout->table_right = buffer->w / 2;
  layout->bases_per_row = layout->table_right - 3;
  layout->focus_row = buffer->h / 2;
  if (layout->bases_per_row < 1 || buffer->h < 7) {
    return -1;
  }

  right_half = buffer->w - layout->table_right - 1;
  layout->helix_center = layout->table_right + 1 + (right_half / 2);
  layout->helix_radius = (right_half / 2) - 5;
  if (layout->helix_radius > STRAND_RADIUS) {
    layout->helix_radius = STRAND_RADIUS;
  }

  layout->sequence_rows = SEQUENCE_LENGTH / layout->bases_per_row;
  return 0;
}

static void draw_table(const struct denabase_state *state,
                       struct ScreenBuffer *buffer, const struct layout *layout) {
  long current_base = base_at_row(state, layout, layout->focus_row);
  long focus_sequence_row = current_base / layout->bases_per_row;
  int current_column = 2 + (int)(current_base % layout->bases_per_row);
  int bottom = buffer->h - 1;

  for (int x = 1; x < layout->table_right; x++) {
    put(buffer, '_', x, 0);
    put(buffer, '_', x, bottom);
  }
  for (int y = 1; y <= bottom; y++) {
    put(buffer, '|', 0, y);
    put(buffer, '|', layout->table_right, y);
  }
  put(buffer, '<', 0, layout->focus_row);
  put(buffer, '>', layout->table_right, layout->focus_row);

  for (int x = 0; x < layout->bases_per_row && SEQUENCE_NAME[x] != '\0'; x++) {
    put(buffer, SEQUENCE_NAME[x], 2 + x, 1);
  }

  for (int y = 2; y < bottom; y++) {
    int offset = y - layout->focus_row;
    long sequence_row = 0;

    if (offset == -1 || offset == 1) {
      continue;
    }

    // The blank rows either side of the focus row are not sequence rows.
    sequence_row = focus_sequence_row + (offset < 0 ? offset + 1 : 0) +
                   (offset > 0 ? offset - 1 : 0);
    sequence_row = wrap(sequence_row, layout->sequence_rows);
    for (int x = 0; x < layout->bases_per_row; x++) {
      put(buffer, state->sequence[sequence_row * layout->bases_per_row + x],
          2 + x, y);
    }
  }

  put(buffer, 'v', current_column, layout->focus_row - 1);
  put(buffer, '^', current_column, layout->focus_row + 1);
}

static int strand_column(const struct layout *layout, double angle) {
  return layout->helix_center + (int)floor(layout->helix_radius * cos(angle) + 0.5);
}

/*
  Draws the base and its complement between the strands as `--A====T--`: each
  base hangs off the strand it belongs to and the two are linked. The pair is
  left out where the strands are too close on this row to fit it.
*/
static void draw_base_pair(struct ScreenBuffer *buffer, int y, char base,
                           int base_strand_min, int base_strand_max,
                           int other_strand_min, int other_strand_max) {
  int base_is_left = base_strand_min < other_strand_min;
  int left = (base_is_left ? base_strand_max : other_strand_max) + 1;
  int right = (base_is_left ? other_strand_min : base_strand_min) - 1;

  if (right - left < 6) {
    return;
  }

  put(buffer, '-', left, y);
  put(buffer, '-', left + 1, y);
  put(buffer, base_is_left ? base : complement(base), left + 2, y);
  for (int x = left + 3; x < right - 2; x++) {
    put(buffer, '=', x, y);
  }
  put(buffer, base_is_left ? complement(base) : base, right - 2, y);
  put(buffer, '-', right - 1, y);
  put(buffer, '-', right, y);
}

static void draw_helix(const struct denabase_state *state,
                       struct ScreenBuffer *buffer, const struct layout *layout) {
  int current_row =
      layout->focus_row - (int)wrap(state->scroll, ROWS_PER_BASE);

  if (layout->helix_radius < 3) {
    return;
  }

  for (int y = 0; y < buffer->h; y++) {
    long helix_row = state->scroll + y - layout->focus_row;
    int strand_1_min = buffer->w;
    int strand_1_max = -1;
    int strand_2_min = buffer->w;
    int strand_2_max = -1;

    // A strand can cross several columns within one row, so sample it more
    // finely than the rows to leave no gaps.
    for (int sample = 0; sample < STRAND_SAMPLES_PER_ROW; sample++) {
      double angle = (helix_row + (sample / (double)STRAND_SAMPLES_PER_ROW)) /
                     STRAND_PITCH;
      int strand_1 = strand_column(layout, angle);
      int strand_2 = strand_column(layout, angle + STRAND_PHASE + 3.14159265358979);

      put(buffer, STRAND_MARKER, strand_1, y);
      put(buffer, STRAND_MARKER, strand_2, y);
      strand_1_min = strand_1 < strand_1_min ? strand_1 : strand_1_min;
      strand_1_max = strand_1 > strand_1_max ? strand_1 : strand_1_max;
      strand_2_min = strand_2 < strand_2_min ? strand_2 : strand_2_min;
      strand_2_max = strand_2 > strand_2_max ? strand_2 : strand_2_max;
    }

    if (wrap(helix_row, ROWS_PER_BASE) == 0) {
      draw_base_pair(buffer, y, state->sequence[base_at_row(state, layout, y)],
                     strand_1_min, strand_1_max, strand_2_min, strand_2_max);
    }

    // Point at the current base from either side, on the row its pair is on.
    if (y == current_row) {
      int left = strand_1_min < strand_2_min ? strand_1_min : strand_2_min;
      int right = strand_1_max > strand_2_max ? strand_1_max : strand_2_max;

      if (left - 2 > layout->table_right) {
        put(buffer, '>', left - 2, y);
      }
      for (int x = right + 2; x < right + 5; x++) {
        put(buffer, '<', x, y);
      }
    }
  }
}

/*
  G pairs with C and A with T. Humans are roughly 30% each of A and T and 20%
  each of G and C.
*/
static void generate_random_sequence(char *sequence, size_t length) {
  for (size_t i = 0; i < length; i++) {
    double selection = rand() / (double)RAND_MAX;

    if (selection < 0.2) {
      sequence[i] = 'G';
    } else if (selection < 0.4) {
      sequence[i] = 'C';
    } else if (selection < 0.7) {
      sequence[i] = 'A';
    } else {
      sequence[i] = 'T';
    }
  }
}

static char complement(char base) {
  switch (base) {
  case 'A':
    return 'T';
  case 'T':
    return 'A';
  case 'G':
    return 'C';
  case 'C':
    return 'G';
  default:
    return ' ';
  }
}
