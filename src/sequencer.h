#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <pthread.h>
#include <semaphore.h>

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
  void (*setup_function)();
  void (*service_function)();
  void (*teardown_function)();
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
  const unsigned int number_of_services;
  struct Service *services;
  timer_t timer;
  struct itimerspec timer_interval;
} Schedule;

#endif
