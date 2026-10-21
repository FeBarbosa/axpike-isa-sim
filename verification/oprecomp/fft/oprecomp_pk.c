// SPDX-License-Identifier: BSD-3-Clause

#include "oprecomp.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

enum measurement_state {
  MEASUREMENT_UNINITIALIZED,
  MEASUREMENT_STARTED,
  MEASUREMENT_STOPPED,
};

static enum measurement_state state = MEASUREMENT_UNINITIALIZED;
static struct timeval start_time;
static int duration_seconds;
static int iterations;

static struct timeval current_time(void) {
  struct timeval now;

  if (gettimeofday(&now, NULL) != 0) {
    perror("gettimeofday");
    exit(EXIT_FAILURE);
  }
  return now;
}

static double elapsed_seconds(void) {
  struct timeval now = current_time();
  return (double)(now.tv_sec - start_time.tv_sec) +
         (double)(now.tv_usec - start_time.tv_usec) * 1.0e-6;
}

void oprecomp_start(void) {
  const char *duration = getenv("AMESTER_DURATION");

  assert(state == MEASUREMENT_UNINITIALIZED);
  state = MEASUREMENT_STARTED;
  duration_seconds = duration == NULL ? 0 : atoi(duration);
  start_time = current_time();
}

int oprecomp_iterate(void) {
  assert(state == MEASUREMENT_STARTED);
  ++iterations;
  return duration_seconds > 0 && elapsed_seconds() < duration_seconds;
}

void oprecomp_stop(void) {
  double elapsed;

  assert(state == MEASUREMENT_STARTED);
  state = MEASUREMENT_STOPPED;
  elapsed = elapsed_seconds();

  if (iterations == 0)
    iterations = 1;

  printf("# time_total: %f\n", elapsed);
  printf("# iterations: %d\n", iterations);
  printf("# time_avg: %f\n", elapsed / iterations);
}
