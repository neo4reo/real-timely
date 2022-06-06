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
#include "difference_frame.h"

/**
 * @brief TODO NICK: doc
 */
void difference_frame_setup(FramePipeline *frame_pipeline)
{
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

  // TODO NICK: Implement

  // Enqueue the difference frame.
  attempt(
      mq_send(
          frame_pipeline->difference_frame_queue,
          (const char *)&frame,
          sizeof(Frame *),
          0),
      "mq_send() difference_frame_queue");
}
