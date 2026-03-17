#ifndef STRANGE_RUNTIME_H_
#define STRANGE_RUNTIME_H_

#define STRANGE_DEFAULT_TIMEOUT_SECONDS 30

struct strange_screensaver_descriptor;

struct strange_options {
  int timeout_seconds;
  const struct strange_screensaver_descriptor *screensaver_descriptor;
};

int strange_run(const struct strange_options *options);

#endif
