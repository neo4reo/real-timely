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
struct sched_param schedule_parameters;

/**
 * @brief Start the log timer for measuring elapsed time.
 */
void start_log_timer()
{
  get_current_monotonic_raw_time(&start_time);
}

/**
 * @brief Erase the syslog file and open a new log stream.
 */
void reset_log(const char *log_prefix)
{
  static char command[100] = "";
  strcat(command, "echo \"");
  strcat(command, log_prefix);
  strcat(command, " $(uname -a)\" | tee /var/log/syslog");
  system(command);
  openlog(log_prefix, LOG_NDELAY, LOG_DAEMON);
}

/**
 * @brief Generate a log message, prefixed with the CPU and priority of the
 * caller, followed by the given formatted message.
 */
void write_log(const char *format, ...)
{
  // Get the CPU.
  int cpu = attempt(sched_getcpu(), "sched_getcpu()");

  // Get the scheduler priority.
  sched_getparam(0, &schedule_parameters);
  int max_priority = sched_get_priority_max(sched_getscheduler(0));
  int priority_descending = max_priority - schedule_parameters.sched_priority;

  // Prefix the message.
  static char message[500] = "";
  sprintf(message, "CPU: %i, Priority: %i, ", cpu, priority_descending);
  strcat(message, format);

  va_list arguments;
  va_start(arguments, format);
  vsyslog(LOG_INFO, message, arguments);
  va_end(arguments);
}

/**
 * @brief Generate a log message, prefixed with the CPU and priority of the
 * caller, followed by the elapsed time, followed by the given formatted
 * message.
 */
void write_log_with_timer(const char *format, ...)
{
  // Get the elapsed log time.
  get_current_monotonic_raw_time(&current_time);
  double elapsed_time = get_elapsed_time_in_seconds(&start_time, &current_time);

  // Get the CPU.
  int cpu = attempt(sched_getcpu(), "sched_getcpu()");

  // Get the scheduler priority.
  sched_getparam(0, &schedule_parameters);
  int max_priority = sched_get_priority_max(sched_getscheduler(0));
  int priority_descending = max_priority - schedule_parameters.sched_priority;

  // Prefix the message.
  static char message[500] = "";
  sprintf(message, "CPU: %i, Priority: %i, Elapsed: %6.9lf, ", cpu, priority_descending, elapsed_time);
  strcat(message, format);

  va_list arguments;
  va_start(arguments, format);
  vsyslog(LOG_INFO, message, arguments);
  va_end(arguments);
}
