/*********************************************************************

  THE ENHANCE PROJECT
  School of Informatics,
  University of Edinburgh,
  Edinburgh - EH9 3JZ
  United Kingdom

  Written by: Gagarine Yaikhom

*********************************************************************/

/*
  $Id$
  Interface for receiving data from the previous stages.

  Written by: Gagarine Yaikhom (g.yaikhom@sms.ed.ac.uk)
  School of Informatics, University of Edinburgh
*/

#include <eskel.h>

extern int __eskel_time_comp(const void *a, const void *b);

int __eskel_branch_auxilliary (__eskel_node_t *n) {
    int i;
    char dummy = 'A';
    if (n->stype != UNKNOWN)
        for (i = 0; i < n->sil.n; i++) {
#ifdef __ESKEL_DEBUG_TAKE
            printf ("[%d] take - Branching auxilliary from task_%d to task_%d.\n",
                    __eskel_rt.rank, __eskel_rt.curr_node2task[__eskel_rt.rank],
                    __eskel_rt.curr_task2node[n->sil.l[i]]);
#endif
            /* Here we are not sending anything that will
               change the values on the remote buffer. This is
               important because the remote buffer might be a
               state variable, which could be tarnished with
               junk value. This is required because the
               receiver is expecting a data unit, which we are
               not sending because we don't have any to send. */
            MPI_Send(&dummy, 0, MPI_CHAR,
                     __eskel_rt.curr_task2node[n->sil.l[i]],
                     ESKEL_NEW_SCHEDULE, MPI_COMM_WORLD);
            /* Send the new schedule. */
            MPI_Send(__eskel_rt.new_task2node, __eskel_rt.nleaves,
                     MPI_INT, __eskel_rt.curr_task2node[n->sil.l[i]],
                     ESKEL_NEW_SCHEDULE, MPI_COMM_WORLD);
        }
    else
        MPI_Send(&dummy, 1, MPI_CHAR, ESKEL_SCHEDULER,
                 ESKEL_ACKNOWLEDGEMENT, MPI_COMM_WORLD);
    return 0;
}

/* Receive data from the previous stage (case with scheduling). */
int __eskel_take_with_scheduling(void *buffer, int units) {
    MPI_Status status;
    static int reschedule_count = 0;
    int current_task;
    __eskel_node_t *n;
    double temp[11], median;

    /* Which stage am I executing. */
    current_task = __eskel_rt.curr_node2task[__eskel_rt.rank];
    n = __eskel_rt.sstab[current_task];

    /* The reschedule flag is set by previous take if there was a
       schedule received. We delayed the necessary house keeping
       chores until this take. */
    if (__eskel_reschedule_flag && (reschedule_count == n->sol.n)) {
        __eskel_reschedule_flag = 0;
        reschedule_count = 0;
        return __eskel_check_rescheduling();
    } 

    /* What is the type of producer skeleton. */
    switch (n->ptype) {
    case PIPE:
        /* Try to receive a data unit. */
        MPI_Recv(buffer, units, n->input_dtype,
                 __eskel_rt.curr_task2node[n->sol.l[0]],
                 MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        /* Check the tag. */
        if (status.MPI_TAG == ESKEL_DATA) {
            /* A data unit was received without scheduling tag.
               Continue as normal. */
        } else if (status.MPI_TAG == ESKEL_DATA_SCHEDULE) {
            /* Receive the new schedule. */
            MPI_Recv(__eskel_rt.new_task2node,
                     __eskel_rt.nleaves, MPI_INT,
                     __eskel_rt.curr_task2node[n->sol.l[0]],
                     ESKEL_NEW_SCHEDULE, MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);
            __eskel_reschedule_flag = 1;
            reschedule_count = 1;
        } else {
            /* Receive the new schedule. */
            MPI_Recv(__eskel_rt.new_task2node,
                     __eskel_rt.nleaves, MPI_INT,
                     __eskel_rt.curr_task2node[n->sol.l[0]],
                     ESKEL_NEW_SCHEDULE, MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            /* Branch new schedule to all the successors. */
            __eskel_branch_auxilliary(n);

            /* Since I did not receive a valid data unit, I should
               reschedule immediately. */
            return ESKEL_RESCHEDULE;
        }
        break;

    case DEAL:
        while (reschedule_count < n->sol.n) {
            /* Try to receive a data unit. */
            MPI_Recv(buffer, units, n->input_dtype, MPI_ANY_SOURCE,
                     MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            /* Check the tags. */
            if (status.MPI_TAG == ESKEL_DATA) {
                /* Everything went normal. */
                return ESKEL_DONT_RESCHEDULE;
            } else if (status.MPI_TAG == ESKEL_DATA_SCHEDULE) {
                /* Receive new schedule. */
                MPI_Recv(__eskel_rt.new_task2node, __eskel_rt.nleaves,
                         MPI_INT, status.MPI_SOURCE, ESKEL_NEW_SCHEDULE,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                __eskel_reschedule_flag = 1;
                reschedule_count++;
                return ESKEL_DONT_RESCHEDULE;
            } else if (status.MPI_TAG == ESKEL_NEW_SCHEDULE) {
                /* I did not receive a data unit, so I should
                   reschedule immediately. However, since this point
                   serves as a converge point for the new schedule, I
                   should wait until I have received the new schedule
                   from the rest of the senders. */
                MPI_Recv(__eskel_rt.new_task2node, __eskel_rt.nleaves,
                         MPI_INT, status.MPI_SOURCE, ESKEL_NEW_SCHEDULE,
                         MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                __eskel_reschedule_flag = 2;
                reschedule_count++;
            }
        }

        /* Branch new schedule to all the successors. */
        __eskel_branch_auxilliary(n);
        
        /* Since I did not receive a valid data unit, I should
           reschedule immediately. */
        return ESKEL_RESCHEDULE;
        break;

    case TASK:
    case UNKNOWN:
    case NSKEL:
        break;
    }

    /* Do timing for the last stage. Only node 1 should time the tasks. */
    if (__eskel_rt.rank == 1) {
        if ((__eskel_time_task < __eskel_rt.nleaves) &&
            (__eskel_time_task == current_task)) {
            if (current_task == (__eskel_rt.nleaves - 1)) {
                if (__eskel_start_time == -1.0) {
#ifdef __ESKEL_DEBUG_TAKE
                    printf("(%d-", current_task);
#endif
                    __eskel_start_time = MPI_Wtime();
                } else {
                    temp[__eskel_rate_count] = MPI_Wtime() - __eskel_start_time;
                    __eskel_task_duration += temp[__eskel_rate_count];
#ifdef __ESKEL_DEBUG_TAKE
                    printf("%d)", current_task);
#endif
                    __eskel_start_time = -1.0;
                    __eskel_rate_count++;
                    if (__eskel_rate_count == 11) {
#ifdef __ESKEL_DEBUG_TAKE
                        printf("\n");
#endif
                        /* Find the median and average. */
                        qsort(temp, 11, sizeof(double), __eskel_time_comp);
                        median = temp[6];
                        __eskel_task_duration /= 11;

                        /*                         MPI_Send(&__eskel_task_duration, 1, MPI_DOUBLE, */
                        /*                                  ESKEL_SCHEDULER, 12345, MPI_COMM_WORLD); */
                        MPI_Send(&median, 1, MPI_DOUBLE,
                                 ESKEL_SCHEDULER, 12345, MPI_COMM_WORLD);

                        __eskel_rate_count = 0;
#ifdef __ESKEL_DEBUG_TAKE
                        printf("[%d]  rmon - Task %d "
                               "[mean: %f, median: %f] secs.\n",
                               __eskel_rt.rank, current_task,
                               __eskel_task_duration, median);
#endif
                        __eskel_time_task++;
                    }
                }
            } else {
#ifdef __ESKEL_DEBUG_TAKE
                printf("(%d-", current_task);
#endif
                __eskel_start_time = MPI_Wtime();
            }
        }
    }
    return ESKEL_DONT_RESCHEDULE;
}

/* Receive data from the previous stage (case without scheduling). */
int __eskel_take_without_scheduling(void *buffer, int units) {
    __eskel_node_t *n;

    /* Which stage am I executing. */
    n = __eskel_rt.sstab[__eskel_rt.curr_node2task[__eskel_rt.rank]];

    /* What is the type of producer skeleton. */
    switch (n->ptype) {
    case PIPE:
        MPI_Recv(buffer, units, n->input_dtype,
                 __eskel_rt.curr_task2node[n->sol.l[0]],
                 ESKEL_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        break;
    case DEAL:
        MPI_Recv(buffer, units, n->input_dtype, MPI_ANY_SOURCE,
                 ESKEL_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        break;

    case TASK:
    case UNKNOWN:
    case NSKEL:
        break;
    }
    return ESKEL_DONT_RESCHEDULE;
}
