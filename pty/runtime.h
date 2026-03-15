#ifndef STRANGE_RUNTIME_H_
#define STRANGE_RUNTIME_H_

#include <stdio.h>

#define STRANGE_DEFAULT_TIMEOUT_SECONDS 30

struct strange_options {
  int timeout_seconds;
};

void strange_print_usage(FILE *stream, const char *prog_name);
int strange_run(const struct strange_options *options);

#endif
