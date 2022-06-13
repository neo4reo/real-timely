/**
 * @author Nick McCrea (nickmccrea.com)
 * @brief Final project for Real Time Embedded Systems series, University of
 * Colorado Boulder's online MSEE. Instructor Dr. Sam Siewert.
 * @date 2022
 */

#include <mqueue.h>
#include "../sequencer.hpp"
#include "../utils/error.h"
#include "../utils/log.h"
#include "select_frame.h"

#define TICK_DETECTION_THRESHOLD_PERCENTAGE (0.45)

double previous_difference_percentage{0};
Frame *current_best_frame;

unsigned int frame_count;

/**
 * @brief TODO NICK: doc
 */
void select_frame_setup(FramePipeline *frame_pipeline)
{
  // Initialize the current best frame.
  current_best_frame = &frame_pipeline->frames[0];
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
  // Dequeue the next differenced frame.
  Frame *frame;
  attempt(
      mq_receive(
          frame_pipeline->difference_frame_queue,
          (char *)&frame,
          sizeof(Frame *),
          NULL),
      "mq_receive() difference_frame_queue in select_frame");

  write_log_with_timer("Select Frame - Previous: %f, Current: %f", previous_difference_percentage, frame->difference_percentage);

  if (
      // This frame crosses above the threshold.
      previous_difference_percentage < TICK_DETECTION_THRESHOLD_PERCENTAGE &&
      frame->difference_percentage >= TICK_DETECTION_THRESHOLD_PERCENTAGE)
  {
    write_log_with_timer("Select Frame - TICK DETECTED, SAVING BEST FRAME", previous_difference_percentage, frame->difference_percentage);
    // Enqueue the selected frame buffer.
    attempt(
        mq_send(
            frame_pipeline->selected_frame_queue,
            (const char *)&current_best_frame,
            sizeof(Frame *),
            0),
        "mq_send() selected_frame_queue in select_frame");
  }
  else if (
      // This frame crosses below the threshold.
      previous_difference_percentage >= TICK_DETECTION_THRESHOLD_PERCENTAGE &&
      frame->difference_percentage < TICK_DETECTION_THRESHOLD_PERCENTAGE)
  {
    write_log_with_timer("Select Frame - STABILITY DETECTED, RESETTING BEST FRAME", previous_difference_percentage, frame->difference_percentage);

    // Begin a new search for the best frame, staring with this one.
    current_best_frame = frame;
  }
  else if (
    // This frame is better than the current best frame.
    frame->difference_percentage < current_best_frame->difference_percentage
  )
    // Make this frame the new best frame.
    current_best_frame = frame;

  // Enqueue the processed frame.
  attempt(
      mq_send(
          frame_pipeline->available_frame_queue,
          (const char *)&frame,
          sizeof(Frame *),
          0),
      "mq_send() available_frame_queue in select_frame");

  previous_difference_percentage = frame->difference_percentage;
  ++frame_count;
}
