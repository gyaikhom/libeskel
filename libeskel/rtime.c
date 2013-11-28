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
   This is the place where we define all the variables that are global
   to the runtime system.

   FIXME: Should be changed. Put everything in __eskel_rt, which is the
   runtime data structure. */

#include <eskel.h>

MPI_Request __eskel_sched_listener;
int __eskel_reschedule_flag = 0;
__eskel_take_t __eskel_take = NULL;
__eskel_give_t __eskel_give = NULL;
double __eskel_task_duration;
double __eskel_start_time = -1.0;
int __eskel_time_task = 0;
int __eskel_rate_count = 0;
struct __eskel_rt_s __eskel_rt;
eskel_sched_handler_t __eskel_sched_handler = NULL;
eskel_sched_init_t __eskel_sched_init = NULL;
eskel_sched_final_t __eskel_sched_final = NULL;
unsigned int __eskel_sched_period = 1;
__eskel_rmon_t *__eskel_rmon = NULL;
long __eskel_nmaps;
int *__eskel_maps = NULL;
time_t __eskel_exec_time[2];
long __eskel_sched_count = 0;
int __eskel_summary_request = 0;

int __eskel_time_comp(const void *a, const void *b) {
    if (*(double *)a == *(double *)b)
        return 0;
    else if (*(double *)a > *(double *)b)
        return 1;
    else return -1;
}
