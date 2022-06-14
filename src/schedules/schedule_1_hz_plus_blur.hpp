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
#include "../services/blur_frame.h"
#include "../services/write_frame.h"

#define NUMBER_OF_FRAMES_1_HZ_PLUS_BLUR (20)

Frame frames_1_hz_plus_blur[NUMBER_OF_FRAMES_1_HZ_PLUS_BLUR];

FramePipeline frame_pipeline_1_hz_plus_blur = {
    .number_of_frames = NUMBER_OF_FRAMES_1_HZ_PLUS_BLUR,
    .frames = frames_1_hz_plus_blur,
    .message_queue_attributes = {
        .mq_maxmsg = NUMBER_OF_FRAMES_1_HZ_PLUS_BLUR,
        .mq_msgsize = sizeof(Frame *),
    }};

Schedule schedule_1_hz_plus_blur = {
    .frequency = 3,
    .maximum_iterations = 570,
    .iteration_counter = 0,
    .sequencer_cpu = 0,
    .services = {
        {
            .id = 1,
            .name = "Capture Frame",
            .period = 1,
            .cpu = 1,
            .exit_flag = FALSE,
            .frame_pipeline = &frame_pipeline_1_hz_plus_blur,
            .setup_function = capture_frame_setup,
            .service_function = capture_frame,
            .teardown_function = capture_frame_teardown,
        },
        {
            .id = 2,
            .name = "Difference Frame",
            .period = 1,
            .cpu = 2,
            .exit_flag = FALSE,
            .frame_pipeline = &frame_pipeline_1_hz_plus_blur,
            .setup_function = difference_frame_setup,
            .service_function = difference_frame,
            .teardown_function = difference_frame_teardown,
        },
        {
            .id = 3,
            .name = "Select Frame",
            .period = 1,
            .cpu = 2,
            .exit_flag = FALSE,
            .frame_pipeline = &frame_pipeline_1_hz_plus_blur,
            .setup_function = select_frame_setup,
            .service_function = select_frame,
            .teardown_function = select_frame_teardown,
        },
        {
            .id = 4,
            .name = "Blur Frame",
            .period = 3,
            .cpu = 1,
            .exit_flag = FALSE,
            .frame_pipeline = &frame_pipeline_1_hz_plus_blur,
            .setup_function = blur_frame_setup,
            .service_function = blur_frame,
            .teardown_function = blur_frame_teardown,
        },
        {
            .id = 5,
            .name = "Write Frame",
            .period = 3,
            .cpu = 2,
            .exit_flag = FALSE,
            .frame_pipeline = &frame_pipeline_1_hz_plus_blur,
            .setup_function = write_frame_setup,
            .service_function = write_frame,
            .teardown_function = write_frame_teardown,
        },
    }};
