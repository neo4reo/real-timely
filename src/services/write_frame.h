#ifndef WRITE_FRAME_H
#define WRITE_FRAME_H

#include "../sequencer.hpp"

void write_frame_setup(FramePipeline *frame_pipeline);
void write_frame_teardown(FramePipeline *frame_pipeline);
void write_frame(FramePipeline *frame_pipeline, Service *service);

#endif
