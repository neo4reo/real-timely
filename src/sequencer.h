#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <mqueue.h>
#include <opencv2/videoio.hpp>
#include <pthread.h>
#include <semaphore.h>

#define TRUE (1)
#define FALSE (0)

#define LOG_PREFIX "[REAL TIMELY]"

#define NUMBER_OF_SERVICES (4)
#define NUMBER_OF_FRAMES (100)

#define AVAILABLE_FRAME_QUEUE_NAME "/available_frame_queue"
#define CAPTURED_FRAME_QUEUE_NAME "/captured_frame_queue"
#define DIFFERENCE_FRAME_QUEUE_NAME "/difference_frame_queue"
#define SELECTED_FRAME_QUEUE_NAME "/selected_frame_queue"

/**
 * @brief A structure containing a frame buffer and associated metadata.
 */
typedef struct Frame
{
  cv::Mat frame_buffer;
  unsigned int difference_absolute;
  double difference_percentage;
} Frame;

/**
 * @brief A struct containing all of the resources used by the real-time system
 * for processing frames.
 */
typedef struct FramePipeline
{
  Frame frames[NUMBER_OF_FRAMES];
  mqd_t available_frame_queue;
  mqd_t captured_frame_queue;
  mqd_t difference_frame_queue;
  mqd_t selected_frame_queue;
  struct mq_attr message_queue_attributes;
} FramePipeline;

/**
 * @brief A struct containing the properties of a single real-time service.
 */
typedef struct Service
{
  const unsigned int id;
  const char *name;
  const int period;
  const int cpu;
  int exit_flag;
  FramePipeline *frame_pipeline;
  void (*setup_function)(FramePipeline *);
  void (*service_function)(FramePipeline *);
  void (*teardown_function)(FramePipeline *);
  int priority_descending;
  sem_t setup_semaphore;
  sem_t semaphore;
  pthread_t thread_descriptor;
  pthread_attr_t thread_attributes;
  struct sched_param schedule_parameters;
} Service;

/**
 * @brief A struct describing a schedule of real-time services.
 */
typedef struct Schedule
{
  const double frequency;
  const unsigned long long maximum_iterations;
  unsigned long long iteration_counter;
  const int sequencer_cpu;
  Service services[NUMBER_OF_SERVICES];
  timer_t timer;
  struct itimerspec timer_interval;
} Schedule;

#endif
