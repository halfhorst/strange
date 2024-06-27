#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cleanup.h"
#include "demos/denabase.h"
#include "demos/digital_rain.h"
#include "renderer.h"
#include "screensaver.h"
#include "tty.h"

void print_usage(void) { printf("Usage: strange -s SCREENSAVER -d DELAY\n"); }

void print_help(void) {
  print_usage();
  printf(
      "\nstrange is a terminal screensaver."
      "\n\nSCREEN_SAVER  refers to a particular screensaver.  Currently "
      "supported demos are \"denabase\", and \"digital_rain\".\n"

      "\n-> denabase is a DNA visualization inspired by the DNA "
      "database from Blade Runner 2049."
      "\n-> digital_rain is an homage to the digital rain from "
      "the Matrix, and Ghost in the Shell before it."

      "\n\nThis program manipulates your tty. If you find it "
      "left things in a bad state for any reason, try using "
      "`tset` or `stty sane` to restore it.");
}

int main(int argc, char** argv) {
  int opt;
  char* name = NULL;
  int delay = 0;

  while ((opt = getopt(argc, argv, ":s:d:h")) != -1) {
    switch (opt) {
      case 's':
        name = optarg;
        break;
      case 'd':
        delay = atoi(optarg);
        break;
      case 'h':
        print_help();
        return EXIT_SUCCESS;
      default:
        print_usage();
        return EXIT_FAILURE;
    }
  }

  if (name == NULL || delay == 0) {
    print_usage();
    return EXIT_FAILURE;
  }

  struct ScreenSaver screensaver;
  if (strcmp(name, "denabase") == 0) {
    init_screensaver(denabase_init, denabase_update, denabase_cleanup,
                     DENABASE_CHAR_WIDTH, &screensaver);
  } else if (strncmp(name, "digital_rain", 3) == 0) {
    init_screensaver(digital_rain_init, digital_rain_update,
                     digital_rain_cleanup, DIGITAL_RAIN_CHAR_WIDTH,
                     &screensaver);
  } else {
    fprintf(stderr, "Unknown scene %s\n", name);
    return EXIT_FAILURE;
  }

  register_cleanup(screensaver.cleanup);

  if (prepare_tty()) {
    return EXIT_FAILURE;
  }

  screensaver.init();
  render(screensaver);

  return EXIT_SUCCESS;
}
