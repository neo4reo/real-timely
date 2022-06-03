#ifndef DIFFERENCE_FRAME_H
#define DIFFERENCE_FRAME_H

#include "../sequencer.h"

void difference_frame_setup(FramePipeline *frame_pipeline);
void difference_frame_teardown(FramePipeline *frame_pipeline);
void difference_frame(FramePipeline *frame_pipeline);

#endif