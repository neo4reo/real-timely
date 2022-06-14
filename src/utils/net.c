#include <stdio.h>

/**
 * @brief Receive one line from the stream pointed to, and print it.
 */
void receive_and_print_line(FILE *stream)
{
  char incoming_message[1000];
  fgets(incoming_message, 1000, stream);
  printf(incoming_message);
}
