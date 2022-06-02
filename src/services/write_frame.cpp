/**
 * @author Nick McCrea (nickmccrea.com)
 * @brief Final project for Real Time Embedded Systems series, University of
 * Colorado Boulder's online MSEE. Instructor Dr. Sam Siewert.
 * @date 2022
 */

#include <iomanip>
#include <ios>
#include <mqueue.h>
#include <opencv2/core/cvstd.hpp>
#include <opencv2/imgcodecs.hpp>
#include <sstream>
#include "../sequencer.h"
#include "../utils/error.h"
#include "../utils/log.h"
#include "write_frame.h"

#define FILENAME_PREFIX "frames/frame_"
#define FILENAME_EXTENSION ".ppm"

unsigned int frame_number{0};
std::ostringstream filename_number;

/**
 * @brief TODO NICK: doc
 */
void write_frame_setup(FramePipeline *frame_pipeline)
{
}

/**
 * @brief TODO NICK: doc
 */
void write_frame_teardown(FramePipeline *frame_pipeline)
{
}

/**
 * @brief TODO NICK: doc
 */
void write_frame(FramePipeline *frame_pipeline)
{
  // Dequeue the next selected frame buffer.
  cv::Mat *frame_buffer;
  attempt(
      mq_receive(
          frame_pipeline->captured_frame_queue,
          (char *)&frame_buffer,
          sizeof(cv::Mat *),
          NULL),
      "mq_receive() captured_frame_queue");

  // Write the frame to disk.
  filename_number.str("");
  filename_number.clear();
  filename_number << std::right << std::setfill('0') << std::setw(6) << frame_number;
  cv::imwrite(
      FILENAME_PREFIX + filename_number.str() + FILENAME_EXTENSION,
      *frame_buffer);

  // Increment the frame number.
  ++frame_number;

  // Enqueue the newly-available frame buffer.
  attempt(
      mq_send(
          frame_pipeline->available_frame_queue,
          (const char *)&frame_buffer,
          sizeof(cv::Mat *),
          0),
      "mq_send() available_frame_queue");
}
