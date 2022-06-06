#ifndef UTILS_TIME_H
#define UTILS_TIME_H

#include <time.h>

#define NANOSECONDS_PER_SECOND (1000000000)
#define NANOSECONDS_PER_MILLSECOND (1000000)
#define MICROSECONDS_PER_SECOND (1000000)

void get_current_monotonic_raw_time(struct timespec *result);
void get_current_realtime_time(struct timespec *result);
double get_time_in_seconds(struct timespec *time);
void normalize_timespec(struct timespec *time);
void get_elapsed_time(struct timespec *start_time, struct timespec *end_time, struct timespec *result);
double get_elapsed_time_in_seconds(struct timespec *start_time, struct timespec *end_time);
void print_elapsed_time(struct timespec *start_time, struct timespec *end_time, const char *prefix_text);
void get_timespec_from_seconds(double seconds, struct timespec *result);

#endif
