/**
 * @author Nick McCrea (nickmccrea.com)
 * @brief Final project for Real Time Embedded Systems series, University of
 * Colorado Boulder's online MSEE. Instructor Dr. Sam Siewert.
 * @date 2022
 */

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "schedules/schedule_1_hz.hpp"
#include "services/capture_frame.h"
#include "services/difference_frame.h"
#include "services/select_frame.h"
#include "services/write_frame.h"
#include "sequencer.hpp"
#include "utils/error.h"
#include "utils/log.h"
#include "utils/time.h"

FramePipeline *frame_pipeline = &frame_pipeline_1_hz;
Schedule *schedule = &schedule_1_hz;

/**
 * @brief Compare two services' periods for sorting priority.
 */
int compare_service_periods(const void *a, const void *b)
{
  const Service *service_a = (Service *)a;
  const Service *service_b = (Service *)b;
  return service_a->period - service_b->period;
}

/**
 * @brief Sort the services from shortest period to longest, and assign rate-
 * monotonic priorities accordingly.
 */
void assign_service_priorities(Schedule *schedule)
{
  qsort(schedule->services, NUMBER_OF_SERVICES, sizeof(Service), compare_service_periods);
  for (int index = 0; index < NUMBER_OF_SERVICES; ++index)
    schedule->services[index].priority_descending = index + 1;
}

/**
 * @brief A real-time service thread entry point, for use with
 * `pthread_create()`. Provides initialization of service thread resources. The
 * service to execute on this thread must be provided in the thread arguments.
 */
void *ServiceThread(void *thread_parameters)
{
  // Unpack the thread parameters.
  Service *service = (Service *)thread_parameters;

  // Run the service's setup function.
  write_log("Service: %i (%s) SETUP STARTING...", service->id, service->name);
  if (service->setup_function != 0)
    (service->setup_function)(service->frame_pipeline);
  write_log("Service: %i (%s) SETUP COMPLETE", service->id, service->name);

  // Allow sequencer to proceed.
  attempt(
      sem_post(&service->setup_semaphore),
      "sem_post()");

  struct timespec request_start_time, request_complete_time;
  unsigned int request_counter = 0;
  while (TRUE)
  {
    // Block until requested.
    attempt(sem_wait(&service->semaphore), "sem_wait()");

    // Exit the thread if indicated.
    if (service->exit_flag)
    {
      // Run the service's teardown function.
      if (service->teardown_function != 0)
        (service->teardown_function)(service->frame_pipeline);

      write_log_with_timer("Service: %i, Service Name: %s, Request: %u, TERMINATING SERVICE", service->id, service->name, request_counter);

      // Terminate the thread.
      pthread_exit((void *)0);
    }

    // Begin new service request by incrementing the counter.
    ++request_counter;

    // Perform the work.
    write_log_with_timer("Service: %i, Service Name: %s, Request: %u, BEGIN", service->id, service->name, request_counter);
    get_current_monotonic_raw_time(&request_start_time);
    (service->service_function)(service->frame_pipeline);
    get_current_monotonic_raw_time(&request_complete_time);
    write_log_with_timer(
        "Service: %i, Service Name: %s, Request: %u, DONE, Request Elapsed Time: %6.9lf",
        service->id,
        service->name,
        request_counter,
        get_elapsed_time_in_seconds(&request_start_time, &request_complete_time));
  }
}

/**
 * @brief Terminate a running schedule sequencer.
 */
void terminate_all_service_threads(Schedule *schedule)
{
  Service *services = schedule->services;

  // Disable the interval timer.
  get_timespec_from_seconds(0, &schedule->timer_interval.it_value);
  get_timespec_from_seconds(0, &schedule->timer_interval.it_interval);
  attempt(timer_settime(
              schedule->timer,
              0,
              &schedule->timer_interval,
              NULL),
          "timer_settime()");

  // Set all services to terminate.
  for (int index = 0; index < NUMBER_OF_SERVICES; ++index)
  {
    services[index].exit_flag = TRUE;
    attempt(
        sem_post(&services[index].semaphore),
        "sem_post()");
  }
}

/**
 * @brief A sequencer function. Generates requests for services according to
 * the defined schedule.
 */
void Sequencer(int signal_number)
{
  write_log_with_timer("Sequencer: %llu", schedule->iteration_counter);

  // Release all the services that are scheduled for this time unit.
  for (int index = 0; index < NUMBER_OF_SERVICES; ++index)
  {
    Service *service = &schedule->services[index];
    if ((schedule->iteration_counter % service->period) == 0)
      attempt(sem_post(&service->semaphore), "sem_post()");
  }

  // Increment the sequence counter.
  ++schedule->iteration_counter;

  if (schedule->iteration_counter >= schedule->maximum_iterations)
    terminate_all_service_threads(schedule);
}

/**
 * @brief Initialize real-time thread attributes. Configures preemptive fixed-
 * priority run-to-completion, CPU affinity, and priority.
 */
void initialize_real_time_thread_attributes(
    pthread_attr_t *thread_attributes,
    struct sched_param *schedule_parameters,
    int cpu_id,
    int priority_descending)
{
  // Initialize default attributes
  errno = pthread_attr_init(thread_attributes);
  if (errno)
    print_with_errno_and_exit("pthread_attr_init()");

  // Disable thread schedule inheritence
  errno = pthread_attr_setinheritsched(thread_attributes, PTHREAD_EXPLICIT_SCHED);
  if (errno)
    print_with_errno_and_exit("pthread_attr_setinheritsched()");

  // Enable FIFO real-time scheduiling
  errno = pthread_attr_setschedpolicy(thread_attributes, SCHED_FIFO);
  if (errno)
    print_with_errno_and_exit("pthread_attr_setschedpolicy()");

  // Apply CPU affinity.
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(cpu_id, &cpu_set);
  errno = pthread_attr_setaffinity_np(thread_attributes, sizeof(cpu_set_t), &cpu_set);
  if (errno)
    print_with_errno_and_exit("pthread_attr_setaffinity_np()");

  // Set the thread's priority, by thread index, descending
  schedule_parameters->sched_priority = sched_get_priority_max(SCHED_FIFO) - priority_descending;
  errno = pthread_attr_setschedparam(thread_attributes, schedule_parameters);
  if (errno)
    print_with_errno_and_exit("pthread_attr_setschedparam()");
}

/**
 * @brief Initialize the resources used by the frame pipeline.
 */
void initialize_frame_pipeline(FramePipeline *frame_pipeline)
{
  // Initialize the available frame queue
  mq_unlink(AVAILABLE_FRAME_QUEUE_NAME);
  frame_pipeline->available_frame_queue = mq_open(AVAILABLE_FRAME_QUEUE_NAME,
                                                  O_CREAT | O_RDWR,
                                                  S_IRWXU,
                                                  &frame_pipeline->message_queue_attributes);
  if (frame_pipeline->available_frame_queue == -1)
    print_with_errno_and_exit("mq_open() failed opening available_frame_queue");

  // Initialize the captured frame queue
  mq_unlink(CAPTURED_FRAME_QUEUE_NAME);
  frame_pipeline->captured_frame_queue = mq_open(CAPTURED_FRAME_QUEUE_NAME,
                                                 O_CREAT | O_RDWR,
                                                 S_IRWXU,
                                                 &frame_pipeline->message_queue_attributes);
  if (frame_pipeline->captured_frame_queue == -1)
    print_with_errno_and_exit("mq_open() failed opening captured_frame_queue");

  // Initialize the difference frame queue
  mq_unlink(DIFFERENCE_FRAME_QUEUE_NAME);
  frame_pipeline->difference_frame_queue = mq_open(DIFFERENCE_FRAME_QUEUE_NAME,
                                                   O_CREAT | O_RDWR,
                                                   S_IRWXU,
                                                   &frame_pipeline->message_queue_attributes);
  if (frame_pipeline->difference_frame_queue == -1)
    print_with_errno_and_exit("mq_open() failed opening difference_frame_queue");

  // Initialize the selected frame queue
  mq_unlink(SELECTED_FRAME_QUEUE_NAME);
  frame_pipeline->selected_frame_queue = mq_open(SELECTED_FRAME_QUEUE_NAME,
                                                 O_CREAT | O_RDWR,
                                                 S_IRWXU,
                                                 &frame_pipeline->message_queue_attributes);
  if (frame_pipeline->selected_frame_queue == -1)
    print_with_errno_and_exit("mq_open() failed opening selected_frame_queue");
}

/**
 * @brief Release the resources used by the frame pipeline.
 */
void uninitialize_frame_pipeline(FramePipeline *frame_pipeline)
{
  // Close the available message queue.
  attempt(mq_close(frame_pipeline->available_frame_queue), "mq_close() available_frame_queue");
  mq_unlink(AVAILABLE_FRAME_QUEUE_NAME);

  // Close the captured message queue.
  attempt(mq_close(frame_pipeline->captured_frame_queue), "mq_close() captured_frame_queue");
  mq_unlink(CAPTURED_FRAME_QUEUE_NAME);

  // Close the difference message queue.
  attempt(mq_close(frame_pipeline->difference_frame_queue), "mq_close() difference_frame_queue");
  mq_unlink(DIFFERENCE_FRAME_QUEUE_NAME);

  // Close the selected message queue.
  attempt(mq_close(frame_pipeline->selected_frame_queue), "mq_close() selected_frame_queue");
  mq_unlink(SELECTED_FRAME_QUEUE_NAME);
}

/**
 * @brief Initialize and start all of the service threads for the the given
 * schedule.
 */
void start_all_service_threads(Schedule *schedule, FramePipeline *frame_pipeline)
{
  // Start each service thread.
  for (int index = 0; index < NUMBER_OF_SERVICES; ++index)
  {
    Service *service = &schedule->services[index];

    // Initialize the service's setup semaphore to block until released.
    attempt(
        sem_init(&service->setup_semaphore, 0, 0),
        "sem_init()");

    // Initialize the service's semaphore to block until released.
    attempt(
        sem_init(&service->semaphore, 0, 0),
        "sem_init()");

    // Initialize the thread attributes for real-time.
    initialize_real_time_thread_attributes(
        &service->thread_attributes,
        &service->schedule_parameters,
        service->cpu,
        service->priority_descending);

    // Start the service's thread.
    write_log("Service: %i (%s) THREAD CREATE STARTED...\n", service->id, service->name);
    errno = pthread_create(
        &service->thread_descriptor,
        &service->thread_attributes,
        ServiceThread,
        service);
    if (errno)
      print_with_errno_and_exit("pthread_create()");
    write_log("Service: %i (%s) THREAD CREATE COMPLETE", service->id, service->name);
  }

  // Wait for each service thread to finish setup.
  for (int index = 0; index < NUMBER_OF_SERVICES; ++index)
  {
    Service *service = &schedule->services[index];
    attempt(sem_wait(&service->setup_semaphore), "sem_wait()");
    write_log("Service: %i (%s) READY", service->id, service->name);
  }
}

/**
 * @brief Join the calling thread to all of the running service threads in the
 * given schedule.
 */
void join_all_service_threads(Schedule *schedule)
{
  for (int index = 0; index < NUMBER_OF_SERVICES; ++index)
  {
    Service *service = &schedule->services[index];
    errno = pthread_join(service->thread_descriptor, NULL);
    if (errno)
      print_with_errno_and_exit("pthread_join()");
  }
}

/**
 * @brief Start sequencing service requests according to the provided schedule.
 */
void begin_sequencing(Schedule *schedule)
{
  // Configure the interval handler.
  struct sigaction alarm_action = {.sa_handler = (void (*)(int))Sequencer};
  attempt(
      sigaction(SIGALRM, &alarm_action, NULL),
      "sigaction()");

  // Initialize the timer.
  get_timespec_from_seconds(
      1 / schedule->frequency,
      &schedule->timer_interval.it_value);
  get_timespec_from_seconds(
      1 / schedule->frequency,
      &schedule->timer_interval.it_interval);
  attempt(
      timer_create(CLOCK_REALTIME, NULL, &schedule->timer),
      "timer_create()");

  // Start the timer.
  start_log_timer();
  attempt(
      timer_settime(
          schedule->timer,
          0,
          &schedule->timer_interval,
          NULL),
      "timer_settime()");
}

/**
 * @brief Set the calling thread to the highest-priority real-time schedule.
 */
void set_current_thread_to_real_time(int cpu)
{
  struct sched_param schedule_paramaters;
  attempt(
      sched_getparam(0, &schedule_paramaters),
      "sched_getparam()");

  schedule_paramaters.sched_priority = sched_get_priority_max(SCHED_FIFO);
  attempt(
      sched_setscheduler(0, SCHED_FIFO, &schedule_paramaters),
      "sched_setscheduler()");

  // Apply CPU affinity.
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  CPU_SET(cpu, &cpu_set);
  attempt(
      sched_setaffinity(0, sizeof(cpu_set), &cpu_set),
      "sched_setaffinity()");
}

/**
 * @brief Validate that the calling thread is running with the highest-priority
 * real-time schedule.
 */
void validate_current_thread_is_real_time()
{
  struct sched_param schedule_paramaters;
  attempt(
      sched_getparam(0, &schedule_paramaters),
      "sched_getparam()");

  int schedule_type = sched_getscheduler(0);

  if (schedule_type != SCHED_FIFO || schedule_paramaters.sched_priority != sched_get_priority_max(SCHED_FIFO))
  {
    printf("FATAL: Main thread must not be preemptible.\n");
    exit(EXIT_FAILURE);
  }
}

int main()
{
  reset_log();

  set_current_thread_to_real_time(schedule->sequencer_cpu);
  validate_current_thread_is_real_time();
  initialize_frame_pipeline(frame_pipeline);

  assign_service_priorities(schedule);
  start_all_service_threads(schedule, frame_pipeline);
  begin_sequencing(schedule);

  join_all_service_threads(schedule);
  uninitialize_frame_pipeline(frame_pipeline);
}
