#include <stdio.h>
#include "test_functions.h"

void print_beans_setup()
{
  printf("Ready to print \"BEANS!\"\n");
}

void print_beans_teardown()
{
  printf("All done printing \"BEANS!\"\n");
}

void print_beans()
{
  printf("BEANS!\n");
}

void print_cornbread_setup()
{
  printf("Ready to print \"CORNBREAD!\"\n");
}

void print_cornbread_teardown()
{
  printf("All done printing \"CORNBREAD!\"\n");
}

void print_cornbread()
{
  printf("CORNBREAD!\n");
}

void print_pickles_setup()
{
  printf("Ready to print \"PICKLES!\"\n");
}

void print_pickles_teardown()
{
  printf("All done printing \"PICKLES!\"\n");
}

void print_pickles()
{
  printf("PICKLES!\n");
}
