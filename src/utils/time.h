#ifndef  UTILS_TIME_H
#define  UTILS_TIME_H

#include <time.h>

#define NANOSECONDS_PER_SECOND (1000000000)

void get_current_time(struct timespec *result);
void normalize_timespec(struct timespec *time);
void get_elapsed_time(struct timespec *start_time, struct timespec *end_time, struct timespec *result);
double get_elapsed_time_in_seconds(struct timespec *start_time, struct timespec *end_time);
double get_seconds_from_timespec(struct timespec *time);
void print_elapsed_time(struct timespec *start_time, struct timespec *end_time, const char *prefix_text);

#endif
