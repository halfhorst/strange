#include <argp.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "renderer.h"
#include "demos/denabase.h"
#include "demos/digital_rain.h"
#include "screensaver.h"

/* Program documentation. After \v is a long description that follows options. */
static char doc[] = "\nA terminal screensaver.\v"
                    "Starts a screensaver rendering in the terminal. "
                    "DEMO_NAME refers to a particular screensaver.  Currently "
                    "supported demos are \"denabase\", and \"digital_rain\".\n"

                    "\n-> denabase is a DNA visualization inspired by the DNA "
                    "database from Blade Runner 2049."
                    "\n-> digital_rain is an homage to the digital rain from "
                    "the Matrix, and Ghost in the Shell before it."

                    "\n\nThis program manipulates your tty. If you find it "
                    "left things in a bad state for any reason, try using "
                    "`tset` or `stty sane` to restore it.";

static char args_doc[] = "DEMO_NAME";

static struct argp_option options[] = {
  // {"delay",       'd',  "milliseconds", 0, "Delay in milliseconds imposed before "
  //                                     "redraw. The default is 16, yielding "
  //                                     "approximately 60 FPS." },
  // {"screensaver", 's',  "seconds", 0, "Run strangeland in screensaver mode. "
  //                                     "The demo will start after n seconds of "
  //                                     "inactivity on the tty."},
  {"ascii",       'a',  0,         0, "Request the demo use ASCII characters "
                                      "instead of UTF-8."},
  { 0 }
};

struct arguments {
  char *args[1];
  char *delay;
};

/* Parse a single option. */
static error_t
parse_opt (int key, char *arg, struct argp_state *state) {
  struct arguments *arguments = state->input;

  /* key is either an option key or one of several special
  values related to arguments */
  switch (key) {
    case 'd':
      arguments->delay = arg;
      break;
    case ARGP_KEY_ARG:
      if (state->arg_num > 0) {
        argp_usage (state);
      }
      arguments->args[state->arg_num] = arg;
      break;
    case ARGP_KEY_END:
      if (state->arg_num < 1) {
        argp_usage (state);
      }
      break;
    default:
      return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char **argv) {
  struct arguments arguments;
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  char *scene = arguments.args[0];
  struct ScreenSaver screensaver;
  if (strncmp(scene, "denabase", 8) == 0) {
    init_screensaver(denabase_init, denabase_update, denabase_cleanup, DENABASE_CHAR_WIDTH, &screensaver);
  } else if (strncmp(scene, "digital_rain", 3) == 0) {
    init_screensaver(digital_rain_init, digital_rain_update, digital_rain_cleanup, DIGITAL_RAIN_CHAR_WIDTH, &screensaver);
  } else {
    fprintf(stderr, "Unknown scene %s\n", scene);
    return EXIT_FAILURE;
  }

  render(screensaver);

  return EXIT_SUCCESS;
}
