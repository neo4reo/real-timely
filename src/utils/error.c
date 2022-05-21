#include <err.h>
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
  vfprintf(stderr, format, arguments);
  exit(EXIT_FAILURE);
}

void vprint_with_errno_and_exit(const char *format, va_list arguments)
{
  verr(EXIT_FAILURE, format, arguments);
}

/**
 * @brief Print the current error number and exit.
 */
void print_with_errno_and_exit(const char *format, ...)
{
  va_list arguments;
  va_start(arguments, format);
  vprint_with_errno_and_exit(format, arguments);
  va_end(arguments);
}

/**
 * @brief If the first argument resolves to -1, exits with the error message
 * provided. Otherwise returns the result.
 */
int attempt(int result, const char *format, ...)
{
  va_list arguments;
  va_start(arguments, format);
  if (result == -1)
    vprint_with_errno_and_exit(format, arguments);

  return result;
}
