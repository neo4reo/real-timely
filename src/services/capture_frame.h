#ifndef CAPTURE_FRAME_H
#define CAPTURE_FRAME_H

#include "../sequencer.h"

void capture_frame_setup(FramePipeline *frame_pipeline);
void capture_frame_teardown(FramePipeline *frame_pipeline);
void capture_frame(FramePipeline *frame_pipeline);

#endif
