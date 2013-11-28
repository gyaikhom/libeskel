/*********************************************************************

  THE ENHANCE PROJECT
  School of Informatics,
  University of Edinburgh,
  Edinburgh - EH9 3JZ
  United Kingdom

  Written by: Gagarine Yaikhom

*********************************************************************/

#ifndef __ESKEL_H
#define __ESKEL_H

/* Define these macros to get module specific debugging messages. */
//#define __ESKEL_DEBUG_MEMORY     /* Check memory leaks. */
//#define __ESKEL_DEBUG_SCHEDULER  /* Scheduling. */
//#define __ESKEL_DEBUG_RMONITOR   /* Resource monitor multithreading. */
//#define __ESKEL_DEBUG_PEPA       /* PEPA related things. */
//#define __ESKEL_DEBUG_TAKE       /* Take interface for receiving data. */
//#define __ESKEL_DEBUG_GIVE       /* Give interface for sending data. */
//#define __ESKEL_DEBUG_STATE      /* State transfer. */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <mpi.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <libeskel/math.h>
#include <libeskel/coollex.h>
#include <libeskel/summ.h>
#include <libeskel/mem.h>
#include <libeskel/htree.h>
#include <libeskel/pepa.h>
#include <libeskel/rmon.h>
#include <libeskel/sched.h>
#include <libeskel/state.h>
#include <libeskel/take.h>
#include <libeskel/give.h>
#include <libeskel/tags.h>

/* These macros expands to the member (or member pointer) in the ith
   row, jth column of the vector representation of two dimensional of
   matrix, m, where k is the number of columns in a row. */
#define __ESKEL_2DMBER(m, i, j, k) (*(m + (k * i) + j))
#define __ESKEL_2DMBER_PTR(m, i, j, k) (m + (k * i) + j)

/* Programmer interfaces for:
   initialisation, execution and finalisation. */
extern int eskel_init(int *argc, char ***argv);
extern int eskel_exec(int);
extern int eskel_final(void);

/* Macros for easy access to the eskel runtime system. */
#define eskel_rank __eskel_rt.rank
#define eskel_size __eskel_rt.nproc
#define eskel_ntask __eskel_rt.nleaves
#define eskel_task_idx __eskel_rt.curr_node2task[eskel_rank]

/* These macros handle rescheduling when needed. Note here that these
   interfaces are used to communicate between tasks, no matter what
   the patterns are (see "take.c" and "give.c" for details on
   functional overloading of these interfaces). */
#define eskel_give(X,Y)                                \
    {                                                  \
        if (__eskel_give((X),(Y)) == ESKEL_RESCHEDULE) \
            return ESKEL_RESCHEDULE;                   \
    }                                                                    
#define eskel_take(X,Y)                                \
    {                                                  \
        if (__eskel_take((X),(Y)) == ESKEL_RESCHEDULE) \
            return ESKEL_RESCHEDULE;                   \
    }

#endif /* __ESKEL_H */
