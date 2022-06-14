/**
 * @author Nick McCrea (nickmccrea.com)
 * @brief Final project for Real Time Embedded Systems series, University of
 * Colorado Boulder's online MSEE. Instructor Dr. Sam Siewert.
 * @date 2022
 */

#include <mqueue.h>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <unistd.h>
#include "../sequencer.hpp"
#include "../utils/error.h"
#include "../utils/log.h"
#include "../utils/time.h"
#include "blur_frame.h"

/**
 * @brief TODO NICK: doc
 */
void blur_frame_setup(FramePipeline *frame_pipeline)
{
}

/**
 * @brief TODO NICK: doc
 */
void blur_frame_teardown(FramePipeline *frame_pipeline)
{
}

/**
 * @brief TODO NICK: doc
 */
void blur_frame(FramePipeline *frame_pipeline)
{
  // Dequeue the next selected frame.
  Frame *frame;
  attempt(
      mq_receive(
          frame_pipeline->selected_frame_queue,
          (char *)&frame,
          sizeof(Frame *),
          NULL),
      "mq_receive() selected_frame_queue in blur_frame");

  // Blur the frame
  cv::blur(frame->frame_buffer, frame->frame_buffer, cv::Size(50, 50));

  // Enqueue the blurred frame.
  attempt(
      mq_send(
          frame_pipeline->blurred_frame_queue,
          (const char *)&frame,
          sizeof(Frame *),
          0),
      "mq_send() blurred_frame_queue in blur_frame");
}
