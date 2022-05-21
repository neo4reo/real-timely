#define _GNU_SOURCE
#include <sched.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include "error.h"
#include "log.h"
#include "time.h"

struct timespec start_time, current_time;

/**
 * Erase the syslog file and open a new log stream, and record logging start
 * time.
 */
void start_log(const char *log_prefix)
{
  static char command[100] = "";
  strcat(command, "echo \"");
  strcat(command, log_prefix);
  strcat(command, " $(uname -a)\" | tee /var/log/syslog");
  system(command);
  openlog(log_prefix, LOG_NDELAY, LOG_DAEMON);

  get_current_monotonic_raw_time(&start_time);
}

/**
 * @brief Generate a log message, prefixed with the elapsed time and the CPU of
 * the caller, followed by the given formatted message.
 */
void write_log(const char *format, ...)
{
  // Get the elapsed log time.
  get_current_monotonic_raw_time(&current_time);
  double elapsed_time = get_elapsed_time_in_seconds(&start_time, &current_time);

  // Get the CPU.
  int cpu = attempt(sched_getcpu(), "sched_getcpu()");

  // Prefix the message.
  static char message[500] = "";
  sprintf(message, "Elapsed: %6.9lf, CPU: %i, ", elapsed_time, cpu);
  strcat(message, format);

  va_list arguments;
  va_start(arguments, format);
  vsyslog(LOG_INFO, message, arguments);
  va_end(arguments);
}