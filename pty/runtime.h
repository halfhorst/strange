#ifndef STRANGE_RUNTIME_H_
#define STRANGE_RUNTIME_H_

#define STRANGE_DEFAULT_TIMEOUT_SECONDS 30

struct strange_screensaver_descriptor;

struct strange_options {
  int timeout_seconds;
  // Start the screensaver even while a full-screen program is running.
  int cover_fullscreen;
  // Show the screensaver at startup instead of waiting for the first timeout.
  int start_now;
  const struct strange_screensaver_descriptor *screensaver_descriptor;
};

int strange_run(const struct strange_options *options);

#endif
