#include <stdlib.h>
#include <signal.h>

#include "cleanup.h"

void signal_cleanup(int signal);

static void (*cleanup)(void);

void register_cleanup(void (*cleanup)(void)) {
  cleanup = cleanup;
  signal(SIGINT, signal_cleanup);
  signal(SIGTERM, signal_cleanup);

}

void signal_cleanup(int signal) {
  // FIXME: this is not set correctly
  cleanup();
  exit(EXIT_SUCCESS);
}
