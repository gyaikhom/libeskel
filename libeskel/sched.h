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

   This is where execution actually begins. The nodes starts executing
   their assigned task, or waits for instruction from the scheduler. */

#ifndef __ESKEL_COMPILE
#error "Please do not include 'sched.h'; include 'eskel.h' instead."
#endif

#ifndef __ESKEL_SCHED_H
#define __ESKEL_SCHED_H

extern MPI_Request __eskel_sched_listener;
extern int __eskel_reschedule_flag;
extern int __eskel_check_rescheduling(void);

typedef int (*eskel_sched_handler_t)(int *);
typedef int (*eskel_sched_init_t)(void);
typedef int (*eskel_sched_final_t)(void);
extern int eskel_set_scheduler(eskel_sched_handler_t sched_handler,
                               eskel_sched_init_t sched_init,
                               eskel_sched_final_t sched_final);
extern void eskel_set_schedule_period(unsigned int secs);

extern eskel_sched_handler_t __eskel_sched_handler;
extern eskel_sched_init_t __eskel_sched_init;
extern eskel_sched_final_t __eskel_sched_final;

#endif /* __ESKEL_SCHED_H */
