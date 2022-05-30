#ifndef UTILS_SYSLOG_H
#define UTILS_SYSLOG_H

void reset_log(const char *log_prefix);
void start_log_timer();
void write_log(const char *format, ...);

#endif
