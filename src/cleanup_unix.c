#include <signal.h>
#include <stdlib.h>

#include "cleanup.h"

void signal_cleanup(int signal);

static void (*cleanup)(void);

void register_cleanup(void (*cleanup_fxn)(void)) {
  cleanup = cleanup_fxn;
  signal(SIGINT, signal_cleanup);
  signal(SIGTERM, signal_cleanup);
}

void signal_cleanup(int signal) {
  // supress unused variable
  (void)signal;

  // FIXME: this is not set correctly
  cleanup();
  exit(EXIT_SUCCESS);
}
