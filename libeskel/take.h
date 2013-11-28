/*********************************************************************

  THE ENHANCE PROJECT
  School of Informatics,
  University of Edinburgh,
  Edinburgh - EH9 3JZ
  United Kingdom

  Written by: Gagarine Yaikhom

*********************************************************************/

/**
   DESCRIPTION:
   These are the internal interfaces for receiving data. */

#ifndef __ESKEL_COMPILE
#error "Please do not include 'take.h'; include 'eskel.h' instead."
#endif

#ifndef __ESKEL_TAKE_H
#define __ESKEL_TAKE_H

typedef int (*__eskel_take_t)(void *buffer, int units);
extern MPI_Request __eskel_sched_listener;
extern __eskel_take_t __eskel_take;
extern double __eskel_task_duration;
extern double __eskel_start_time;
extern int __eskel_time_task;
extern int __eskel_rate_count;
extern int __eskel_reschedule_flag;

extern int __eskel_take_with_scheduling(void *buffer, int units);
extern int __eskel_take_without_scheduling(void *buffer, int units);

#endif /* __ESKEL_TAKE_H */
