#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "log.h"

/**
 * Erase the syslog file and open a new log stream.
 */
void initialize_log(const char *log_prefix)
{
  char command[100] = "";
  strcat(command, "echo \"");
  strcat(command, log_prefix);
  strcat(command, " $(uname -a)\" | tee /var/log/syslog");
  system(command);
  openlog(log_prefix, LOG_NDELAY, LOG_DAEMON);
}
