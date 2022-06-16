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
#include "../utils/time.h"
#include "select_frame.h"

#define TICK_DETECTION_THRESHOLD_PERCENTAGE (0.38)

double previous_difference_percentage{0};
Frame *current_best_frame;

unsigned int frame_count;

/**
 * @brief Initializes values used by the selection algorithm.
 */
void select_frame_setup(FramePipeline *frame_pipeline)
{
  // Initialize the current best frame.
  current_best_frame = &frame_pipeline->frames[0];
}

/**
 * @brief Does nothing.
 */
void select_frame_teardown(FramePipeline *frame_pipeline)
{
}

/**
 * @brief Inspects the next incoming frame for a tick event, and maintains a
 * reference to the most stable frame since the most recent tick event.
 *
 * When a tick event is detected, determined by the relative difference from
 * the previous frame crossing above a certain threshold, the stable frame is
 * added to the que for writing to disk and the stable frame is reset to the
 * current frame. Subsequent frames with lower difference percentages are set
 * as the favored frame until the next tick event is detected.
 */
void select_frame(FramePipeline *frame_pipeline, Service *service, unsigned int request_counter)
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

  // Start request timer.
  write_log_with_timer("Service: %i, Service Name: %s, Request: %u, BEGIN", service->id, service->name, request_counter);
  get_current_monotonic_raw_time(&service->work_start_time);

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

    // Begin a new search for the best frame, staring with this one.
    current_best_frame = frame;
  }
  else if (
      // This frame is better than the current best frame.
      frame->difference_percentage < current_best_frame->difference_percentage)
    // Make this frame the new best frame.
    current_best_frame = frame;

  // End request timer.
  get_current_monotonic_raw_time(&service->work_complete_time);
  write_log_with_timer(
      "Service: %i, Service Name: %s, Request: %u, DONE, Request Elapsed Time: %6.9lf",
      service->id,
      service->name,
      request_counter,
      get_elapsed_time_in_seconds(&service->work_start_time, &service->work_complete_time));

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
