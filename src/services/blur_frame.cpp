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
 * @brief Does nothing.
 */
void blur_frame_setup(FramePipeline *frame_pipeline)
{
}

/**
 * @brief Does nothing.
 */
void blur_frame_teardown(FramePipeline *frame_pipeline)
{
}

/**
 * @brief Applies a blur effect to the next frame in the queue.
 */
void blur_frame(FramePipeline *frame_pipeline, Service *service, unsigned int request_counter)
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

  // Start request timer.
  write_log_with_timer("Service: %i, Service Name: %s, Request: %u, BEGIN", service->id, service->name, request_counter);
  get_current_monotonic_raw_time(&service->work_start_time);

  // Blur the frame
  cv::blur(frame->frame_buffer, frame->frame_buffer, cv::Size(20, 20));

  // End request timer.
  get_current_monotonic_raw_time(&service->work_complete_time);
  write_log_with_timer(
      "Service: %i, Service Name: %s, Request: %u, DONE, Request Elapsed Time: %6.9lf",
      service->id,
      service->name,
      request_counter,
      get_elapsed_time_in_seconds(&service->work_start_time, &service->work_complete_time));

  // Enqueue the blurred frame.
  attempt(
      mq_send(
          frame_pipeline->blurred_frame_queue,
          (const char *)&frame,
          sizeof(Frame *),
          0),
      "mq_send() blurred_frame_queue in blur_frame");
}
