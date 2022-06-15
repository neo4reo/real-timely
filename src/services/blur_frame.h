#ifndef BLUR_FRAME_H
#define BLUR_FRAME_H

#include "../sequencer.hpp"

void blur_frame_setup(FramePipeline *frame_pipeline);
void blur_frame_teardown(FramePipeline *frame_pipeline);
void blur_frame(FramePipeline *frame_pipeline);

#endif
