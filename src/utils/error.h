#ifndef ERROR_UTILS_H
#define ERROR_UTILS_H

void print_error_and_exit(const char *format, ...);
void print_with_errno_and_exit(const char *format, ...);

#endif
