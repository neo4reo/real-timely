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
#include "../sequencer.h"
#include "../utils/error.h"
#include "../utils/log.h"
#include "capture_frame.h"

cv::VideoCapture video_capture;

/**
 * @brief TODO NICK: doc
 */
void capture_frame_setup(FramePipeline *frame_pipeline)
{
  // Start the camera.
  if (!video_capture.open(0))
    print_error_and_exit("Error at `video_capture.open()`\n");

  // Warm up each frame buffer.
  for (int index = 0; index < NUMBER_OF_FRAME_BUFFERS; ++index)
  {
    cv::Mat *frame_buffer = &frame_pipeline->frame_buffers[index];

    // Write a frame to the buffer. (I think this will initialize memory
    // allocation.)
    while (!video_capture.read(*frame_buffer))
    {
      std::cout << "No frame.\n";
      cv::waitKey(25);
    }

    // Enqueue the frame_buffer.
    attempt(mq_send(
                frame_pipeline->available_frame_queue,
                (const char *)&frame_buffer,
                sizeof(cv::Mat *),
                0),
            "mq_send()");
  }
}

/**
 * @brief TODO NICK: doc
 */
void capture_frame_teardown(FramePipeline *frame_pipeline)
{
  // Shut down the camera.
  video_capture.release();
}

/**
 * @brief TODO NICK: doc
 */
void capture_frame(FramePipeline *frame_pipeline)
{
  // Get the next available frame buffer.
  cv::Mat *frame_buffer;

  // Dequeue the next available frame buffer.
  attempt(
      mq_receive(
          frame_pipeline->available_frame_queue,
          (char *)&frame_buffer,
          sizeof(cv::Mat *),
          NULL),
      "mq_receive()");

  // Capture a frame.
  if (!video_capture.read(*frame_buffer))
  {
    std::cout << "No frame.\n";
    cv::waitKey(25);
  }
}
