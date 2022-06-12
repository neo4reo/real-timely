#ifndef UTILS_SYSLOG_H
#define UTILS_SYSLOG_H

void reset_log();
void start_log_timer();
void write_log(const char *format, ...);
void write_log_with_timer(const char *format, ...);
void write_assignment_log_with_timer(unsigned int frame_number);

#endif
