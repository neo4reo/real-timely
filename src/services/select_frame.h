#ifndef SELECT_FRAME_H
#define SELECT_FRAME_H

#include "../sequencer.h"

void select_frame_setup(FramePipeline *frame_pipeline);
void select_frame_teardown(FramePipeline *frame_pipeline);
void select_frame(FramePipeline *frame_pipeline);

#endif
