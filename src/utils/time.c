#include <math.h>
#include <stdio.h>
#include <time.h>
#include "error.h"
#include "time.h"

/**
 * @brief Get the current monotonic clock time from the computer.
 */
void get_current_monotonic_raw_time(struct timespec *result)
{
  attempt(clock_gettime(CLOCK_MONOTONIC_RAW, result), "clock_gettime()");
}

/**
 * @brief Get the current monotonic clock time from the computer.
 */
void get_current_realtime_time(struct timespec *result)
{
  attempt(clock_gettime(CLOCK_REALTIME, result), "clock_gettime()");
}

/**
 * @brief Convert a `timespec` into seconds, as a `double`.
 */
double get_time_in_seconds(struct timespec *time)
{
  return ((double)(time->tv_sec)) + (((double)(time->tv_nsec)) / NANOSECONDS_PER_SECOND);
}

/**
 * @brief Normalize a `timespec` by correcting any overflow or underflow in the
 * nanoseconds.
 */
void normalize_timespec(struct timespec *time)
{
  // Account for overflow.
  while (time->tv_nsec > NANOSECONDS_PER_SECOND)
  {
    time->tv_sec += 1;
    time->tv_nsec -= NANOSECONDS_PER_SECOND;
  }
  // Account for underflow.
  while (time->tv_nsec < 0)
  {
    time->tv_sec -= 1;
    time->tv_nsec += NANOSECONDS_PER_SECOND;
  }
}

/**
 * @brief Calculate the difference between two `timespecs`.
 */
void get_elapsed_time(struct timespec *start_time, struct timespec *end_time, struct timespec *result)
{
  result->tv_sec = end_time->tv_sec - start_time->tv_sec;
  result->tv_nsec = end_time->tv_nsec - start_time->tv_nsec;
  normalize_timespec(result);
}

/**
 * @brief Calculate the difference between two `timespec`s and return the
 * result as a double.
 */
double get_elapsed_time_in_seconds(struct timespec *start_time, struct timespec *end_time)
{
  struct timespec elapsed_time;
  get_elapsed_time(start_time, end_time, &elapsed_time);
  return get_time_in_seconds(&elapsed_time);
}

/**
 * @brief Print the difference between two `timespecs`.
 */
void print_elapsed_time(struct timespec *start_time, struct timespec *end_time, const char *prefix_text)
{
  printf("%s%6.9lf seconds.\n", prefix_text, get_elapsed_time_in_seconds(start_time, end_time));
}

/**
 * @brief Get a `timespec` representing a given number of seconds.
 */
void get_timespec_from_seconds(double seconds, struct timespec *result)
{
  result->tv_sec = floor(seconds);
  result->tv_nsec = remainder(seconds, 1) * NANOSECONDS_PER_SECOND;
}
