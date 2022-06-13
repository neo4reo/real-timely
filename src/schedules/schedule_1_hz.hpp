/**
 * @author Nick McCrea (nickmccrea.com)
 * @brief Final project for Real Time Embedded Systems series, University of
 * Colorado Boulder's online MSEE. Instructor Dr. Sam Siewert.
 * @date 2022
 */

#include "../sequencer.hpp"
#include "../services/capture_frame.h"
#include "../services/difference_frame.h"
#include "../services/select_frame.h"
#include "../services/write_frame.h"

#define NUMBER_OF_FRAMES_1_HZ (20)

Frame frames_1_hz[NUMBER_OF_FRAMES_1_HZ];

FramePipeline frame_pipeline_1_hz = {
    .number_of_frames = NUMBER_OF_FRAMES_1_HZ,
    .frames = frames_1_hz,
    .message_queue_attributes = {
        .mq_maxmsg = NUMBER_OF_FRAMES_1_HZ,
        .mq_msgsize = sizeof(Frame *),
    }};

Schedule schedule_1_hz = {
    .frequency = 3,
    .maximum_iterations = 570,
    .iteration_counter = 0,
    .sequencer_cpu = 0,
    .services = {
        {
            .id = 1,
            .name = "Capture Frame",
            .period = 1,
            .cpu = 3,
            .exit_flag = FALSE,
            .frame_pipeline = &frame_pipeline_1_hz,
            .setup_function = capture_frame_setup,
            .service_function = capture_frame,
            .teardown_function = capture_frame_teardown,
        },
        {
            .id = 2,
            .name = "Difference Frame",
            .period = 1,
            .cpu = 3,
            .exit_flag = FALSE,
            .frame_pipeline = &frame_pipeline_1_hz,
            .setup_function = difference_frame_setup,
            .service_function = difference_frame,
            .teardown_function = difference_frame_teardown,
        },
        {
            .id = 3,
            .name = "Select Frame",
            .period = 1,
            .cpu = 3,
            .exit_flag = FALSE,
            .frame_pipeline = &frame_pipeline_1_hz,
            .setup_function = select_frame_setup,
            .service_function = select_frame,
            .teardown_function = select_frame_teardown,
        },
        {
            .id = 4,
            .name = "Write Frame",
            .period = 3,
            .cpu = 3,
            .exit_flag = FALSE,
            .frame_pipeline = &frame_pipeline_1_hz,
            .setup_function = write_frame_setup,
            .service_function = write_frame,
            .teardown_function = write_frame_teardown,
        },
    }};
