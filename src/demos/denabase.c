#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "renderer.h"
#include "denabase.h"

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
  char *name;
};

struct DnaSequence *allocate_dna_sequence(char *name);
void update_helix(double radius, double pitch, double x_shift, double y_shift,
                  struct Helix *helix);
void compute_helix_coord(double t, struct Helix *helix, struct Coord *coord);
void check_failed_alloc(void *ptr);
void generate_random_sequence(int capacity, bool isDNA, char *sequence_buffer);
void get_sequence_complement(char *sequence, int capacity, bool isDNA,
                             char *complement_buffer);
char get_complement(char nucleotide, bool isDNA);
bool draw_helix(struct ScreenBuffer *sbuffer, unsigned long frame_count,
                int t_min, int t_max);
void draw_linkage(struct ScreenBuffer *sbuffer, int min_x, int max_x, int y,
                  int sequence_index);
void draw_nucleic_acid_block(struct ScreenBuffer *sbuffer, int seq_index);

// Global state
static struct Helix *strand_1;
static struct Helix *strand_2;
static struct DnaSequence *dna_sequence;
static int block_sequence_index;

void denabase_init(struct ScreenBuffer *sbuffer) {
  dna_sequence = allocate_dna_sequence("IDENT: #09817");
  bool isDNA = true;
  generate_random_sequence(dna_sequence->capacity, isDNA, dna_sequence->sequence);
  get_sequence_complement(dna_sequence->sequence, dna_sequence->capacity,
                          isDNA, dna_sequence->complement);

  strand_1 = malloc(sizeof(struct Helix));
  check_failed_alloc(strand_1);
  strand_1->marker = '0';

  strand_2 = malloc(sizeof(struct Helix));
  check_failed_alloc(strand_2);
  strand_2->marker = '0';

  block_sequence_index = 0;
}

struct DnaSequence *allocate_dna_sequence(char *name) {
  struct DnaSequence *dna = malloc(sizeof(struct DnaSequence));
  check_failed_alloc(dna);

  dna->sequence = malloc(sizeof(char) * SEQUENCE_BUFFER_SIZE);
  check_failed_alloc(dna->sequence);

  dna->complement = malloc(sizeof(char) * SEQUENCE_BUFFER_SIZE);
  check_failed_alloc(dna->complement);

  dna->capacity = SEQUENCE_BUFFER_SIZE;
  dna->name = "IDENT #09817 (H. sapiens)";

  return dna;
}

void denabase_cleanup(void) {
  free(dna_sequence->sequence);
  free(dna_sequence->complement);
  free(dna_sequence);
  free(strand_1);
  free(strand_2);
}

bool denabase_update(struct ScreenBuffer *sbuffer, unsigned long frame_count) {
  bool isDNA = true;
  int helix_center = sbuffer->w * 0.75;

  float y_shift = strand_1->y_shift;
  if ((frame_count % HELIX_SCROLL_SPEED) == 0) {
    y_shift--;
  }
  update_helix(STRAND_RADIUS, STRAND_PITCH, helix_center,
               y_shift, strand_1);
  update_helix(-STRAND_RADIUS, STRAND_PITCH, helix_center,
               y_shift + STRAND_OFFSET, strand_2);

  // Calculate the range of t that covers the display.
  // We assume strand 2 is shifted up in y, and so has
  // a strictly greater y-shift. Unequal pitch would
  // wreck this.
  int t_min = ((-strand_2->y_shift) / strand_2->pitch);
  int t_max = ((sbuffer->h - strand_1->y_shift) / strand_1->pitch);

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
  draw_helix(sbuffer, frame_count, t_min, t_max);

  // draw the nucleic acid block
  draw_nucleic_acid_block(sbuffer, block_sequence_index);

  return true;
}

/*
  Draw a helix in the center of the right half of the window defined by
  W x H. The helix is parameterized over a variable t. The range of t
  necessary to cover the window is calculated, and the helix is shifted
  by offsetting t by time or frame count.
*/
bool draw_helix(struct ScreenBuffer *sbuffer, unsigned long frame_count,
                int t_min, int t_max) {

  float t_resolution = 10;
  struct Coord s1_coord, s2_coord;
  int linkage_counter = 0;
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
    if ((s1_coord.y > 0) & (s1_coord.y < sbuffer->h)) {
      write_to_buffer(sbuffer, &strand_1->marker, 1, s1_coord.x, s1_coord.y);
    }

    compute_helix_coord(param_2, strand_2, &s2_coord);
    if ((s2_coord.y > 0) & (s2_coord.y < sbuffer->h)) {
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
  return true;
}

void draw_linkage(struct ScreenBuffer *sbuffer, int min_x, int max_x, int y,
                  int sequence_index) {
    // the sequence index has to be tied to the parameter, t
    char base = dna_sequence->sequence[sequence_index];
    char complement = dna_sequence->complement[sequence_index];
    char linkage = '=';
    for (int i = min_x + 1; i < max_x; i++) {
      write_to_buffer(sbuffer, &linkage, 1, i, y);
    }

    int midpoint = min_x + ((max_x - min_x) / 2);
    char h_bond = '-';
    write_to_buffer(sbuffer, &base, 1, midpoint - 1, y);
    write_to_buffer(sbuffer, &h_bond, 1, midpoint, y);
    write_to_buffer(sbuffer, &complement, 1, midpoint + 1, y);
}


/* Update `helix` with the parameters provided. */
void update_helix(double radius, double pitch, double x_shift, double y_shift,
                  struct Helix *helix) {
  helix->radius = radius;
  helix->pitch = pitch;
  helix->x_shift = x_shift;
  helix->y_shift = y_shift;
}

/* Compute the (x, y) coordinates of the helix at parameter `t`. */
void compute_helix_coord(double t, struct Helix *helix, struct Coord *coord) {
  int x = (helix->radius * cos(t)) + helix->x_shift;
  int y = (helix->pitch * t) + helix->y_shift;
  coord->x = x;
  coord->y = y;
}

void generate_random_sequence(int capacity, bool isDNA, char *sequence_buffer) {
  /*  G-C
      A-T/U
      humans are roughly 30% AT and 20% GC
  */

  int rand_int;
  float selection;
  for (int i = 0; i < capacity; i++) {
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
}

void get_sequence_complement(char *sequence, int capacity, bool isDNA, char *complement_buffer) {
  for (int i = 0; i < capacity; i++) {
    complement_buffer[i] = get_complement(sequence[i], isDNA);
  }
}

char get_complement(char nucleotide, bool isDNA) {
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
void draw_nucleic_acid_block(struct ScreenBuffer *sbuffer, int seq_index) {
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
  memcpy(sbuffer->buffer + (sbuffer->w) + 2, dna_sequence->name,
         strlen(dna_sequence->name));

  // copy by row. padding for border and a space is given
  // TODO: This indexing is off by one I think
  for (int i = 2; i < sbuffer->h - 1; i++) {
    memcpy(sbuffer->buffer + (i * sbuffer->w) + 2,
           (dna_sequence->sequence + seq_index) + (i * num_bases_per_row),
           window_middle - 3);
  }

  // blank the delineating rows
  memset(sbuffer->buffer + ((focus_row - 1) * sbuffer->w) + 1,
         SL_SPACE_CHAR, window_middle - 1);
  memset(sbuffer->buffer + ((focus_row + 1) * sbuffer->w) + 1,
         SL_SPACE_CHAR, window_middle - 1);
}

// TODO: needs to be better handling.
// this could leave stuff hanging around.
void check_failed_alloc(void *ptr) {
  if (ptr == NULL) {
    fprintf(stderr, "Failed to allocate memory.");
    exit(EXIT_FAILURE);
  }
}
