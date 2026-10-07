#ifndef STRANGE_RUNTIME_H_
#define STRANGE_RUNTIME_H_

#define STRANGE_DEFAULT_TIMEOUT_SECONDS 30

struct strange_screensaver_descriptor;

struct strange_options {
  int timeout_seconds;
  // Start the screensaver even while a full-screen program is running.
  int cover_fullscreen;
  // The control byte that turns the screensaver off for the session, or
  // STRANGE_NO_KEY to reserve none.
  int disable_key;
  const struct strange_screensaver_descriptor *screensaver_descriptor;
};

int strange_run(const struct strange_options *options);

/*
  Run only the screensaver, with no shell underneath, until a key is pressed.
  Returns 1 if the screensaver fails, after printing why.
*/
int strange_preview(const struct strange_screensaver_descriptor *descriptor);

#endif
