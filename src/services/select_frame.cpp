/**
 * @author Nick McCrea (nickmccrea.com)
 * @brief Final project for Real Time Embedded Systems series, University of
 * Colorado Boulder's online MSEE. Instructor Dr. Sam Siewert.
 * @date 2022
 */

#include <mqueue.h>
#include "../sequencer.h"
#include "../utils/error.h"
#include "../utils/log.h"
#include "select_frame.h"

/**
 * @brief TODO NICK: doc
 */
void select_frame_setup(FramePipeline *frame_pipeline)
{
}

/**
 * @brief TODO NICK: doc
 */
void select_frame_teardown(FramePipeline *frame_pipeline)
{
}

/**
 * @brief TODO NICK: doc
 */
void select_frame(FramePipeline *frame_pipeline)
{
  // Dequeue the next differenced frame buffer.
  cv::Mat *frame_buffer;
  attempt(
      mq_receive(
          frame_pipeline->difference_frame_queue,
          (char *)&frame_buffer,
          sizeof(cv::Mat *),
          NULL),
      "mq_receive() difference_frame_queue");

  // TODO NICK: Implement

  // Enqueue the selected frame buffer.
  attempt(
      mq_send(
          frame_pipeline->selected_frame_queue,
          (const char *)&frame_buffer,
          sizeof(cv::Mat *),
          0),
      "mq_send() selected_frame_queue");
}
