/**
 * @author Nick McCrea (nickmccrea.com)
 * @brief Final project for Real Time Embedded Systems series, University of
 * Colorado Boulder's online MSEE. Instructor Dr. Sam Siewert.
 * @date 2022
 */

#include <mqueue.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <unistd.h>
#include "../sequencer.h"
#include "../utils/error.h"
#include "../utils/log.h"
#include "../utils/time.h"
#include "difference_frame.h"

cv::Mat *previous_frame_buffer;
cv::Mat difference_frame_buffer;
int is_initialized = FALSE;
unsigned int difference_absolute, max_difference_absolute;

/**
 * @brief Calculate the percentage of the given value relative to the given
 * maximum value.
 */
double get_percentage(unsigned int value, unsigned int max_value)
{
  return ((double)value / (double)max_value) * 100.0;
}

/**
 * @brief TODO NICK: doc
 */
void difference_frame_setup(FramePipeline *frame_pipeline)
{
  // Wait for the first frame to receive warmup data.
  cv::Mat *warmup_frame_buffer = &frame_pipeline->frames[0].frame_buffer;
  while (warmup_frame_buffer->cols == 0)
    usleep(MICROSECONDS_PER_SECOND);

  // Compute the maximum absolute difference.
  cv::cvtColor(*warmup_frame_buffer, *warmup_frame_buffer, CV_BGR2GRAY);
  max_difference_absolute = (*warmup_frame_buffer).cols * (*warmup_frame_buffer).rows * 255;
}

/**
 * @brief TODO NICK: doc
 */
void difference_frame_teardown(FramePipeline *frame_pipeline)
{
}

/**
 * @brief TODO NICK: doc
 */
void difference_frame(FramePipeline *frame_pipeline)
{
  // Dequeue the next captured frame.
  Frame *frame;
  attempt(
      mq_receive(
          frame_pipeline->captured_frame_queue,
          (char *)&frame,
          sizeof(Frame *),
          NULL),
      "mq_receive() captured_frame_queue");

  // If this is the very first frame, initialize the previous frame buffer
  // with this same frame.
  if (!is_initialized)
  {
    previous_frame_buffer = &frame->frame_buffer;
    is_initialized = TRUE;
  }

  // Convert the frame to grayscale.
  cv::cvtColor(frame->frame_buffer, frame->frame_buffer, CV_BGR2GRAY);

  // Compute the difference from the previous frame.
  cv::absdiff(*previous_frame_buffer, frame->frame_buffer, difference_frame_buffer);
  frame->difference_absolute = (unsigned int)cv::sum(difference_frame_buffer)[0];
  frame->difference_percentage = get_percentage(frame->difference_absolute, max_difference_absolute);

  // Update the previous frame buffer.
  previous_frame_buffer = &frame->frame_buffer;

  // Enqueue the difference frame.
  attempt(
      mq_send(
          frame_pipeline->difference_frame_queue,
          (const char *)&frame,
          sizeof(Frame *),
          0),
      "mq_send() difference_frame_queue");
}
