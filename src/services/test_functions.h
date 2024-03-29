#ifndef TEST_FUNCTIONS_H
#define TEST_FUNCTIONS_H

#include "../sequencer.hpp"

void print_beans_setup(FramePipeline *frame_pipeline);
void print_beans_teardown(FramePipeline *frame_pipeline);
void print_beans(FramePipeline *frame_pipeline, Service *service, unsigned int request_counter);
void print_cornbread_setup(FramePipeline *frame_pipeline);
void print_cornbread_teardown(FramePipeline *frame_pipeline);
void print_cornbread(FramePipeline *frame_pipeline, Service *service, unsigned int request_counter);
void print_pickles_setup(FramePipeline *frame_pipeline);
void print_pickles_teardown(FramePipeline *frame_pipeline);
void print_pickles(FramePipeline *frame_pipeline, Service *service, unsigned int request_counter);

#endif
