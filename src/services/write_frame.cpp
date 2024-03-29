/**
 * @author Nick McCrea (nickmccrea.com)
 * @brief Final project for Real Time Embedded Systems series, University of
 * Colorado Boulder's online MSEE. Instructor Dr. Sam Siewert.
 * @date 2022
 */

#include <filesystem>
#include <iomanip>
#include <ios>
#include <mqueue.h>
#include <opencv2/core/cvstd.hpp>
#include <opencv2/imgcodecs.hpp>
#include <sstream>
#include <string>
#include <time.h>
#include "../sequencer.hpp"
#include "../utils/error.h"
#include "../utils/log.h"
#include "../utils/time.h"
#include "write_frame.h"

#define OUTPUT_DIRECTORY "output"
#define FILENAME_EXTENSION ".ppm"

unsigned int frame_number{0};
std::ostringstream filename_number;

const struct timespec dequeue_timeout = {
    .tv_sec = 5,
    .tv_nsec = 0,
};

/**
 * @brief Delete old results from the output directory.
 */
void write_frame_setup(FramePipeline *frame_pipeline)
{
  // Make sure the output folder exists.
  std::filesystem::create_directory(OUTPUT_DIRECTORY);

  // Delete the existing contents of the output folder.
  for (const auto &entry : std::filesystem::directory_iterator(OUTPUT_DIRECTORY))
    std::filesystem::remove_all(entry.path());
}

/**
 * @brief Does nothing.
 */
void write_frame_teardown(FramePipeline *frame_pipeline)
{
}

/**
 * @brief Write to disk all frames currently enqueued for writing.
 */
void write_frame(FramePipeline *frame_pipeline, Service *service, unsigned int request_counter)
{
  // Dequeue the next selected frame.
  Frame *frame;

  // Deque and save all frames in the selected frames queue.
  while (-1 != mq_timedreceive(
                   frame_pipeline->selected_frame_queue,
                   (char *)&frame,
                   sizeof(Frame *),
                   NULL,
                   &dequeue_timeout))
  {
    // Start write timer.
    write_log_with_timer("Service: %i, Service Name: %s, Frame Number: %u, BEGIN WRITE", service->id, service->name, frame_number);
    get_current_monotonic_raw_time(&service->work_start_time);

    write_log_with_timer("Write Frame - WRITING FRAME %u", frame_number);
    write_assignment_log_with_timer(frame_number);

    // Write the frame to disk.
    filename_number.str("");
    filename_number.clear();
    filename_number << std::right << std::setfill('0') << std::setw(6) << frame_number;
    cv::imwrite(
        static_cast<std::string>(OUTPUT_DIRECTORY) + "/" + filename_number.str() + FILENAME_EXTENSION,
        frame->frame_buffer);

    // End write timer.
    get_current_monotonic_raw_time(&service->work_complete_time);
    write_log_with_timer(
        "Service: %i, Service Name: %s, Frame Number: %u, END WRITE, Request Elapsed Time: %6.9lf",
        service->id,
        service->name,
        frame_number,
        get_elapsed_time_in_seconds(&service->work_start_time, &service->work_complete_time));

    // Increment the frame number.
    ++frame_number;
  }
}
