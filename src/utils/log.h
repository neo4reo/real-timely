#ifndef UTILS_SYSLOG_H
#define UTILS_SYSLOG_H

void start_log(const char *log_prefix);
void write_log(const char *format, ...);

#endif
