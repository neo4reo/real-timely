#ifndef DIFFERENCE_FRAME_H
#define DIFFERENCE_FRAME_H

#include "../sequencer.hpp"

void difference_frame_setup(FramePipeline *frame_pipeline);
void difference_frame_teardown(FramePipeline *frame_pipeline);
void difference_frame(FramePipeline *frame_pipeline, Service *service, unsigned int request_counter);

#endif
