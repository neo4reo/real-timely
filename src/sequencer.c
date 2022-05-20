/**
 * Sequencer by Nick McCrea.
 *
 * Built for Real Time Embedded Systems project at University of Colorado
 * Boulder, as taught by Dr. Sam Siewert.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils/error.h"
#include "utils/time.h"
#include "sequencer.h"
#include "test_functions.h"

#define TRUE (1)
#define FALSE (0)

#define NUMBER_OF_SERVICES (2)

/**
 * @brief The service schedule.
 */
Schedule schedule = {
    .frequency = 10,
    .maximum_iterations = 50,
    .iteration_counter = 0,
    .number_of_services = NUMBER_OF_SERVICES,
    .services = (Service[NUMBER_OF_SERVICES]){
        {
            .id = 1,
            .period = 10,
            .cpu = 3,
            .priority_descending = 1,
            .exit_flag = FALSE,
            .service_function = (void (*)())print_beans,
        },
        {
            .id = 2,
            .period = 5,
            .cpu = 3,
            .priority_descending = 2,
            .exit_flag = FALSE,
            .service_function = (void (*)())print_cornbread,
        }},
};

// Timing objects
struct timespec start_time, end_time, current_time;

/**
 * @brief A real-time service thread entry point, for use with
 * `pthread_create()`. Provides initialization of service thread resources. The
 * service to execute on this thread must be provided in the thread arguments.
 */
void *ServiceThread(void *thread_parameters)
{
  // Cast the thread parameters so the compiler can handle them.
  Service *service = (Service *)thread_parameters;

  unsigned long long request_counter = 0;
  while (TRUE)
  {
    // Block until requested.
    attempt(sem_wait(&service->semaphore), "sem_wait()");

    // Exit the thread if indicated.
    if (service->exit_flag)
      pthread_exit((void *)0);

    // Begin new service request by incrementing the counter.
    ++request_counter;

    // Perform the work.
    (service->service_function)();
  }
}

/**
 * @brief Terminate a running schedule sequencer.
 */
void terminate_all_services(Schedule *schedule)
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
  for (int index = 0; index < schedule->number_of_services; ++index)
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
  // Release all the services that are scheduled for this time unit.
  for (int index = 0; index < schedule.number_of_services; ++index)
  {
    Service *service = &schedule.services[index];
    if ((schedule.iteration_counter % service->period) == 0)
      attempt(sem_post(&service->semaphore), "sem_post()");
  }

  // Increment the sequence counter.
  ++schedule.iteration_counter;

  if (schedule.iteration_counter >= schedule.maximum_iterations)
    terminate_all_services(&schedule);
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
 * @brief Initialize and start all of the service threads for the the given
 * schedule.
 */
void start_all_services(const Schedule *schedule)
{
  for (int index = 0; index < schedule->number_of_services; ++index)
  {
    Service *service = &schedule->services[index];

    // Initialize the thread attributes for real-time.
    initialize_real_time_thread_attributes(
        &service->thread_attributes,
        &service->schedule_parameters,
        service->cpu,
        service->priority_descending);

    // Initialize the service's semaphore to block until released.
    attempt(
        sem_init(&service->semaphore, 0, 0),
        "sem_init()");

    // Start the service's thread.
    errno = pthread_create(
        &service->thread_descriptor,
        &service->thread_attributes,
        ServiceThread,
        service);
    if (errno)
      print_with_errno_and_exit("pthread_create()");
  }
}

/**
 * @brief Join the calling thread to all of the running service threads in the
 * given schedule.
 */
void join_all_service_threads(const Schedule *schedule)
{
  for (int index = 0; index < schedule->number_of_services; ++index)
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
  struct sigaction alarm_action = {.sa_handler = (void (*)())Sequencer};
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
  get_current_monotonic_raw_time(&start_time);
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
void set_current_thread_to_real_time()
{
  struct sched_param schedule_paramaters;
  attempt(
      sched_getparam(0, &schedule_paramaters),
      "sched_getparam()");

  schedule_paramaters.sched_priority = sched_get_priority_max(SCHED_FIFO);
  attempt(
      sched_setscheduler(0, SCHED_FIFO, &schedule_paramaters),
      "sched_setscheduler()");
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
  // Make sure the main thread cannot get preempted.
  set_current_thread_to_real_time();
  validate_current_thread_is_real_time();

  start_all_services(&schedule);
  begin_sequencing(&schedule);

  join_all_service_threads(&schedule);
}
