#ifndef UTILS_SYSLOG_H
#define UTILS_SYSLOG_H

void reset_log(const char *log_prefix);
void start_log_timer();
void write_log(const char *format, ...);
void write_log_with_timer(const char *format, ...);

#endif
