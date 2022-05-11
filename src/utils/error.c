#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

/**
 * @brief Print the given string and exit.
 *
 * Takes the same arguments as `printf()`.
 */
void print_error_and_exit(const char *format, ...)
{
  va_list arguments;
  va_start(arguments, format);
  fprintf(stderr, format, arguments);
  exit(EXIT_FAILURE);
}

/**
 * @brief Print the current error number and exit.
 */
void print_error_number_and_exit(const char *prefix_message)
{
  fprintf(stderr, "%s error: %d, %s\n", prefix_message, errno, strerror(errno));
  exit(EXIT_FAILURE);
}
