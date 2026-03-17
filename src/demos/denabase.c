#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

#include "../renderer.h"
#include "./denabase.h"

#define SEQUENCE_BUFFER_SIZE 100000

// Helix geometry constants
#define STRAND_RADIUS 10
#define STRAND_PITCH 4
#define STRAND_OFFSET -2.5
#define LINKAGE_PARAMETER 5

// Nucleobase block constants
#define BASES_PER_ROW(w) (w / 2) - 3
#define TOTAL_BASES(w, h) (BASES_PER_ROW(w) * (h - 5))

// discretized rendering makes things look bad at slow speeds
#define HELIX_SCROLL_SPEED 4


struct Coord {
  int x;
  int y;
};

struct Helix {
  double radius;
  double pitch;
  double x_shift;
  double y_shift;
  char marker;
};

struct DnaSequence {
  char *sequence;
  char *complement;
  size_t capacity;
  const char *name;
};

struct denabase_state {
  struct Helix strand_1;
  struct Helix strand_2;
  struct DnaSequence dna_sequence;
  int block_sequence_index;
};

static int allocate_dna_sequence(struct DnaSequence *dna);
static void free_dna_sequence(struct DnaSequence *dna);
static void update_helix(double radius, double pitch, double x_shift,
                         double y_shift, struct Helix *helix);
static void compute_helix_coord(double t, const struct Helix *helix,
                                struct Coord *coord);
static int generate_random_sequence(size_t capacity, bool is_dna,
                                    char *sequence_buffer);
static void get_sequence_complement(const char *sequence, size_t capacity,
                                    bool is_dna, char *complement_buffer);
static char get_complement(char nucleotide, bool is_dna);
static int draw_helix(struct denabase_state *state, struct ScreenBuffer *buffer,
                      int t_min, int t_max);
static void draw_nucleic_acid_block(struct denabase_state *state,
                                    struct ScreenBuffer *buffer,
                                    int seq_index);

static int denabase_init(void **state, struct ScreenBuffer *buffer) {
  struct denabase_state *denabase = NULL;
  bool isDNA = true;

  (void)buffer;
  if (state == NULL) {
    errno = EINVAL;
    return -1;
  }

  denabase = calloc(1, sizeof(*denabase));
  if (denabase == NULL) {
    return -1;
  }
  if (allocate_dna_sequence(&denabase->dna_sequence) == -1 ||
      generate_random_sequence(denabase->dna_sequence.capacity, isDNA,
                               denabase->dna_sequence.sequence) == -1) {
    free_dna_sequence(&denabase->dna_sequence);
    free(denabase);
    return -1;
  }
  get_sequence_complement(denabase->dna_sequence.sequence,
                          denabase->dna_sequence.capacity, isDNA,
                          denabase->dna_sequence.complement);

  denabase->strand_1.marker = '0';
  denabase->strand_2.marker = '0';
  denabase->block_sequence_index = 0;
  *state = denabase;
  return 0;
}

static int allocate_dna_sequence(struct DnaSequence *dna) {
  if (dna == NULL) {
    errno = EINVAL;
    return -1;
  }

  memset(dna, 0, sizeof(*dna));
  dna->sequence = malloc(sizeof(char) * SEQUENCE_BUFFER_SIZE);
  if (dna->sequence == NULL) {
    return -1;
  }
  dna->complement = malloc(sizeof(char) * SEQUENCE_BUFFER_SIZE);
  if (dna->complement == NULL) {
    free(dna->sequence);
    dna->sequence = NULL;
    return -1;
  }

  dna->capacity = SEQUENCE_BUFFER_SIZE;
  dna->name = "IDENT #09817 (H. sapiens)";
  return 0;
}

static void free_dna_sequence(struct DnaSequence *dna) {
  if (dna == NULL) {
    return;
  }

  free(dna->sequence);
  free(dna->complement);
  dna->sequence = NULL;
  dna->complement = NULL;
  dna->capacity = 0;
  dna->name = NULL;
}

static int denabase_update(
    void *state, struct ScreenBuffer *sbuffer,
    const struct strange_screensaver_frame *frame) {
  struct denabase_state *denabase = state;
  unsigned long frame_count = 0;
  int helix_center = sbuffer->w * 0.75;
  double y_shift = 0.0;

  if (denabase == NULL || sbuffer == NULL) {
    errno = EINVAL;
    return -1;
  }
  if (frame != NULL) {
    frame_count = frame->frame_count;
  }

  y_shift = denabase->strand_1.y_shift;
  if ((frame_count % HELIX_SCROLL_SPEED) == 0) {
    y_shift--;
  }
  update_helix(STRAND_RADIUS, STRAND_PITCH, helix_center,
               y_shift, &denabase->strand_1);
  update_helix(-STRAND_RADIUS, STRAND_PITCH, helix_center,
               y_shift + STRAND_OFFSET, &denabase->strand_2);

  // Calculate the range of t that covers the display.
  // We assume strand 2 is shifted up in y, and so has
  // a strictly greater y-shift. Unequal pitch would
  // wreck this.
  int t_min = ((-denabase->strand_2.y_shift) / denabase->strand_2.pitch);
  int t_max =
      ((sbuffer->h - denabase->strand_1.y_shift) / denabase->strand_1.pitch);

  // t min to to max defines a set of linkages
  // t % linkage_param defines where a linkage is
  // those t's also define y's





  // int t_index = t_min % SEQUENCE_BUFFER_SIZE;

  // // if necessary, shift the sequence
  // if (block_sequence_index + BASES_PER_ROW(sbuffer->w) < t_index) {
  //   block_sequence_index += BASES_PER_ROW(sbuffer->w);
  // }

  // int helix_sequence_index = t_index + ((sbuffer->w / 2) - 3) * (sbuffer->h / 2);

  // draw the helix from t_min to t_max
  draw_helix(denabase, sbuffer, t_min, t_max);

  // draw the nucleic acid block
  draw_nucleic_acid_block(denabase, sbuffer, denabase->block_sequence_index);

  return 0;
}

static void denabase_cleanup(void *state) {
  struct denabase_state *denabase = state;

  if (denabase == NULL) {
    return;
  }

  free_dna_sequence(&denabase->dna_sequence);
  free(denabase);
}

const struct strange_screensaver_descriptor strange_denabase_descriptor = {
    .name = "denabase",
    .character_width = DENABASE_CHAR_WIDTH,
    .init = denabase_init,
    .update = denabase_update,
    .cleanup = denabase_cleanup,
};

/*
  Draw a helix in the center of the right half of the window defined by
  W x H. The helix is parameterized over a variable t. The range of t
  necessary to cover the window is calculated, and the helix is shifted
  by offsetting t by time or frame count.
*/
static int draw_helix(struct denabase_state *state, struct ScreenBuffer *sbuffer,
                      int t_min, int t_max) {
  float t_resolution = 10;
  struct Coord s1_coord, s2_coord;
  const struct Helix *strand_1 = &state->strand_1;
  const struct Helix *strand_2 = &state->strand_2;

  for (int t_fine = (t_min * t_resolution);
       t_fine <= (t_max * t_resolution); t_fine++) {
    // we calculated a range to cover the display. We want to draw the
    // helices at the same y-coordinate rather than the same parameter value
    // because it will make drawing the linakges easier. This entails
    // drawing just a bit extra, but it shouldn't be a problem and saves an
    // inverse t -> y calculation.
    float param_1 = t_fine / (float) t_resolution;
    // This assumes equal pitch
    float param_2 = param_1 + ((strand_1->y_shift - strand_2->y_shift)
                               / STRAND_PITCH);

    compute_helix_coord(param_1, strand_1, &s1_coord);
    if ((s1_coord.y > 0) && (s1_coord.y < sbuffer->h)) {
      write_to_buffer(sbuffer, &strand_1->marker, 1, s1_coord.x, s1_coord.y);
    }

    compute_helix_coord(param_2, strand_2, &s2_coord);
    if ((s2_coord.y > 0) && (s2_coord.y < sbuffer->h)) {
      write_to_buffer(sbuffer, &strand_2->marker, 1, s2_coord.x, s2_coord.y);
    }

    // decide if this t corresponds to a linkage
    // if it does, render it
    // t_min % LINKAGE_PARAMETER;

    // based on one of the parameters, draw the linkage
    // decide if linkage is here. if so, get boundaries and draw
    // if ((t_fine % LINKAGE_PARAMETER) == 0) {
    //   int min_x = (s1_coord.x < s2_coord.x) ? s1_coord.x : s2_coord.x;
    //   int max_x = (s1_coord.x == min_x) ? s2_coord.x : s1_coord.x;
    // //   // y coordinates should be the same
    //   int y = s1_coord.y;

    // //   // TODO: properly index the dna sequence
    //   int sequence_index = (int) param_1 % SEQUENCE_BUFFER_SIZE;
    //   draw_linkage(sbuffer, min_x, max_x, y, sequence_index + linkage_counter);
    //   linkage_counter++;
    // }
  }
  return 0;
}


/* Update `helix` with the parameters provided. */
static void update_helix(double radius, double pitch, double x_shift,
                         double y_shift, struct Helix *helix) {
  helix->radius = radius;
  helix->pitch = pitch;
  helix->x_shift = x_shift;
  helix->y_shift = y_shift;
}

/* Compute the (x, y) coordinates of the helix at parameter `t`. */
static void compute_helix_coord(double t, const struct Helix *helix,
                                struct Coord *coord) {
  int x = (helix->radius * cos(t)) + helix->x_shift;
  int y = (helix->pitch * t) + helix->y_shift;
  coord->x = x;
  coord->y = y;
}

static int generate_random_sequence(size_t capacity, bool isDNA,
                                    char *sequence_buffer) {
  /*  G-C
      A-T/U
      humans are roughly 30% AT and 20% GC
  */

  if (sequence_buffer == NULL) {
    errno = EINVAL;
    return -1;
  }

  int rand_int;
  float selection;
  for (size_t i = 0; i < capacity; i++) {
    rand_int = rand();
    selection = (float) rand_int / RAND_MAX;
    if (selection < 0.2) {
      sequence_buffer[i] = 'G';
    } else if (0.2 <= selection && selection < 0.4) {
      sequence_buffer[i] = 'C';
    } else if (0.4 <= selection && selection < 0.7) {
      sequence_buffer[i] = 'A';
    } else {
      if (isDNA) {
        sequence_buffer[i] = 'T';
      } else {
        sequence_buffer[i] = 'U';
      }
    }
  }

  return 0;
}

static void get_sequence_complement(const char *sequence, size_t capacity,
                                    bool isDNA, char *complement_buffer) {
  for (size_t i = 0; i < capacity; i++) {
    complement_buffer[i] = get_complement(sequence[i], isDNA);
  }
}

static char get_complement(char nucleotide, bool isDNA) {
  switch (nucleotide) {
    case 'A':
      if (isDNA) {
        return 'T';
      } else {
        return 'U';
      }
    case 'T':
      return 'A';
    case 'U':
      return 'A';
    case 'G':
      return 'C';
    case 'C':
      return 'G';
    default:
      return ' ';
  }
}

// // fill the DNA block starting at sequence->current
static void draw_nucleic_acid_block(struct denabase_state *state,
                                    struct ScreenBuffer *sbuffer,
                                    int seq_index) {
  int window_middle = sbuffer->w / 2;
  int num_bases_per_row = window_middle - 3;
  int focus_row = sbuffer->h / 2;

  char top_border = '_';
  char side_border = '|';
  char bottom_border = '_';
  char focus_left = '<';
  char focus_right = '>';

  // top border
  memset(sbuffer->buffer + 1, top_border, window_middle - 1);

  // side borders
  for (int i = 1; i < sbuffer->h; i++) {
    write_to_buffer(sbuffer, &side_border, 1, 0, i);
    write_to_buffer(sbuffer, &side_border, 1, window_middle, i);
  }
  write_to_buffer(sbuffer, &focus_left, 1, 0, focus_row);
  write_to_buffer(sbuffer, &focus_right, 1, window_middle, focus_row);

  // bottom border
  memset(sbuffer->buffer + ((sbuffer->h - 1) * sbuffer->w) + 1, bottom_border, window_middle - 1);

  // print the name seqence
  memcpy(sbuffer->buffer + (sbuffer->w) + 2, state->dna_sequence.name,
         strlen(state->dna_sequence.name));

  // copy by row. padding for border and a space is given
  // TODO: This indexing is off by one I think
  for (int i = 2; i < sbuffer->h - 1; i++) {
    memcpy(sbuffer->buffer + (i * sbuffer->w) + 2,
           (state->dna_sequence.sequence + seq_index) + (i * num_bases_per_row),
           window_middle - 3);
  }

  // blank the delineating rows
  memset(sbuffer->buffer + ((focus_row - 1) * sbuffer->w) + 1,
         SL_SPACE_CHAR, window_middle - 1);
  memset(sbuffer->buffer + ((focus_row + 1) * sbuffer->w) + 1,
         SL_SPACE_CHAR, window_middle - 1);
}
