/**
 * @author Nick McCrea (nickmccrea.com)
 * @brief Final project for Real Time Embedded Systems series, University of
 * Colorado Boulder's online MSEE. Instructor Dr. Sam Siewert.
 * @date 2022
 */

#include <iostream>
#include <mqueue.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include "../sequencer.hpp"
#include "../utils/error.h"
#include "../utils/log.h"
#include "capture_frame.h"

cv::VideoCapture video_capture;

/**
 * @brief Starts up the camera, warms up the frame buffers in memory, and
 * initalizes the queue of frames available for writing to.
 */
void capture_frame_setup(FramePipeline *frame_pipeline)
{
  // Start the camera.
  if (!video_capture.open(0))
    print_error_and_exit("Error at `video_capture.open()`\n");

  // Warm up each frame buffer.
  for (int index = 0; index < NUMBER_OF_FRAMES; ++index)
  {
    Frame *frame = &frame_pipeline->frames[index];

    // Write a frame to the buffer. (I think this will initialize memory
    // allocation.)
    while (!video_capture.read(frame->frame_buffer))
    {
      std::cout << "No frame.\n";
      cv::waitKey(25);
    }

    // Enqueue the frame.
    attempt(mq_send(
                frame_pipeline->available_frame_queue,
                (const char *)&frame,
                sizeof(Frame *),
                0),
            "mq_send() available_frame_queue in capture_frame");
  }
}

/**
 * @brief Shuts down the camera.
 */
void capture_frame_teardown(FramePipeline *frame_pipeline)
{
  // Shut down the camera.
  video_capture.release();
}

/**
 * @brief Copies one frame from the camera into the next availablle frame
 * buffer.
 */
void capture_frame(FramePipeline *frame_pipeline, Service *service)
{
  // Dequeue the next available frame.
  Frame *frame;
  attempt(
      mq_receive(
          frame_pipeline->available_frame_queue,
          (char *)&frame,
          sizeof(Frame *),
          NULL),
      "mq_receive() available_frame_queue in capture_frame");

  // Capture a frame.
  if (!video_capture.read(frame->frame_buffer))
  {
    std::cout << "No frame.\n";
    cv::waitKey(25);
  }

  // Enqueue the captured frame.
  attempt(
      mq_send(
          frame_pipeline->captured_frame_queue,
          (const char *)&frame,
          sizeof(Frame *),
          0),
      "mq_send() captured_frame_queue in capture_frame");
}
