#include <stdio.h>
#include "test_functions.h"

void print_beans_setup(FramePipeline *frame_pipeline)
{
  printf("Ready to print \"BEANS!\"\n");
}

void print_beans_teardown(FramePipeline *frame_pipeline)
{
  printf("All done printing \"BEANS!\"\n");
}

void print_beans(FramePipeline *frame_pipeline, Service *service)
{
  printf("BEANS!\n");
}

void print_cornbread_setup(FramePipeline *frame_pipeline)
{
  printf("Ready to print \"CORNBREAD!\"\n");
}

void print_cornbread_teardown(FramePipeline *frame_pipeline)
{
  printf("All done printing \"CORNBREAD!\"\n");
}

void print_cornbread(FramePipeline *frame_pipeline, Service *service)
{
  printf("CORNBREAD!\n");
}

void print_pickles_setup(FramePipeline *frame_pipeline)
{
  printf("Ready to print \"PICKLES!\"\n");
}

void print_pickles_teardown(FramePipeline *frame_pipeline)
{
  printf("All done printing \"PICKLES!\"\n");
}

void print_pickles(FramePipeline *frame_pipeline, Service *service)
{
  printf("PICKLES!\n");
}
