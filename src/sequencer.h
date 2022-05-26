#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <pthread.h>
#include <semaphore.h>

/**
 * @brief A struct containing the properties of a single real-time service.
 */
typedef struct Service
{
  unsigned int id;
  char *name;
  int priority_descending;
  int period;
  int cpu;
  void (*setup_function)();
  void (*service_function)();
  void (*teardown_function)();
  int exit_flag;
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
  double frequency;
  unsigned int number_of_services;
  struct Service *services;
  unsigned long long maximum_iterations;
  unsigned long long iteration_counter;
  int sequencer_cpu;
  timer_t timer;
  struct itimerspec timer_interval;
} Schedule;

#endif
