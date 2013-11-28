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
   These are the internal interfaces for sending data to other tasks. */

#include <eskel.h>

/* For comparing recorded times while sorting. */
extern int __eskel_time_comp(const void *a, const void *b);

/* Check if a new schedule was received. If new schedule received,
   send that schedule to the succeeding process. */
static int __eskel_check_and_send(__eskel_node_t *n,
                                  void *buffer, int units) {
    int flag = 0, i;
    char dummy = 'A';

    /* Test if the scheduler put new schedule recently. */
    MPI_Test(&__eskel_sched_listener, &flag, MPI_STATUS_IGNORE);

    /* If yes. */
    if (flag) {
        /* Check if the new schedule is different from the existing
           schedule. If it is, then continue with the new
           schedule. Otherwise, acknowledge the scheduler that the new
           schedule is received. This acknowledgement is sent by the
           process executing the first stage, instead of the process
           executing the last stage because the new schedule is going
           to be ignored. */
        for (i = 0; i < __eskel_rt.nleaves; i++)
            if (__eskel_rt.curr_task2node[i] != __eskel_rt.new_task2node[i])
                goto new_scheduling;
        /* Send acknowledgement of the new schedule. */
        MPI_Send(&dummy, 1, MPI_CHAR, ESKEL_SCHEDULER,
                 ESKEL_ACKNOWLEDGEMENT, MPI_COMM_WORLD);
    }
    /* Nothing new. Just continue with sending the data. */
    MPI_Send(buffer, units, n->output_dtype,
             __eskel_rt.curr_task2node[n->sil.l[0]],
             ESKEL_DATA, MPI_COMM_WORLD);
    return 0;
    
 new_scheduling:

    /* Send the data (with new schedule tag). */
    MPI_Send(buffer, units, n->output_dtype,
             __eskel_rt.curr_task2node[n->sil.l[0]],
             ESKEL_DATA_SCHEDULE, MPI_COMM_WORLD);

    /* Send new schedule. */
    MPI_Send(__eskel_rt.new_task2node, __eskel_rt.nleaves,
             MPI_INT, __eskel_rt.curr_task2node[n->sil.l[0]],
             ESKEL_NEW_SCHEDULE, MPI_COMM_WORLD);
    return 1;
}

/* Send data to the next stage (case with scheduling). */
int __eskel_give_with_scheduling(void *buffer, int units) {
    int i, current_task;
    static int current = 0;
    char dummy = 'A';
    __eskel_node_t *n;
    double temp[11], median;

    /* Which stage am I executing. */
    current_task = __eskel_rt.curr_node2task[__eskel_rt.rank];
    n = __eskel_rt.sstab[current_task];

    /* If I am executing the first stage, I should check if there was
       a new schedule issued by the scheduler. */
    if (__eskel_rt.curr_node2task[__eskel_rt.rank] == 0) {
        if (__eskel_check_and_send(n, buffer, units)) {
            __eskel_reschedule_flag = 0;
            return __eskel_check_rescheduling();
        }

        /* Do timing for the first stage. */
        if (__eskel_rt.rank == 1) {
            if ((__eskel_time_task < __eskel_rt.nleaves) &&
                (__eskel_time_task == current_task)) {
                if (__eskel_start_time == -1.0) {
#ifdef __ESKEL_DEBUG_GIVE
                    printf("(%d-", current_task);
#endif
                    __eskel_start_time = MPI_Wtime();
                } else {
#ifdef __ESKEL_DEBUG_GIVE
                    printf("%d)", current_task);
#endif
                    temp[__eskel_rate_count] = MPI_Wtime() - __eskel_start_time;
                    __eskel_task_duration += temp[__eskel_rate_count];
                    __eskel_start_time = -1.0;
                    __eskel_rate_count++;
                    if (__eskel_rate_count == 11) {
#ifdef __ESKEL_DEBUG_GIVE
                        printf("\n");
#endif
                        /* Find the median and average. */
                        qsort(temp, 11, sizeof(double), __eskel_time_comp);
                        median = temp[6];
                        __eskel_task_duration /= 11;

                        /* MPI_Send(&__eskel_task_duration, 1, MPI_DOUBLE, */
                        /*          ESKEL_SCHEDULER, 12345, MPI_COMM_WORLD); */
                        MPI_Send(&median, 1, MPI_DOUBLE,
                                 ESKEL_SCHEDULER, 12345, MPI_COMM_WORLD);

                        __eskel_rate_count = 0;
#ifdef __ESKEL_DEBUG_GIVE
                        printf("[%d]  rmon - Task %d "
                               "[mean: %f, median: %f] secs.\n",
                               __eskel_rt.rank, current_task,
                               __eskel_task_duration, median);
#endif
                        __eskel_time_task++;
                    }
                }
            }
        }
        return ESKEL_DONT_RESCHEDULE;
    }

    /* Do timing for the intermediate stages. */
    if (__eskel_rt.rank == 1) {
        if ((__eskel_time_task < __eskel_rt.nleaves) &&
            (__eskel_time_task == current_task)) {
#ifdef __ESKEL_DEBUG_GIVE
            printf("%d)", current_task);
#endif
            temp[__eskel_rate_count] = MPI_Wtime() - __eskel_start_time;
            __eskel_task_duration += temp[__eskel_rate_count];
            __eskel_rate_count++;
            if (__eskel_rate_count == 11) {
#ifdef __ESKEL_DEBUG_GIVE
                printf("\n");
#endif
                /* Find the median and average. */
                qsort(temp, 11, sizeof(double), __eskel_time_comp);
                median = temp[6];
                __eskel_task_duration /= 11;

                /* MPI_Send(&__eskel_task_duration, 1, MPI_DOUBLE, */
                /*          ESKEL_SCHEDULER, 12345, MPI_COMM_WORLD); */
                MPI_Send(&median, 1, MPI_DOUBLE,
                         ESKEL_SCHEDULER, 12345, MPI_COMM_WORLD);

                __eskel_rate_count = 0;
#ifdef __ESKEL_DEBUG_GIVE
                printf("[%d]  rmon - Task %d "
                       "[mean: %f, median: %f] secs.\n",
                       __eskel_rt.rank, current_task,
                       __eskel_task_duration, median);
#endif
                __eskel_start_time = -1.0;
                __eskel_time_task++;
            }
        }
    }

    /* If I am not the first stage, just give data to the next stage
       with the scheduling information if I received any from the
       previous stage. */
    switch (n->stype) {
    case PIPE:
        if (__eskel_reschedule_flag) {
            /* Send data with new schedule tag. */
            MPI_Send(buffer, units, n->output_dtype,
                     __eskel_rt.curr_task2node[n->sil.l[0]],
                     ESKEL_DATA_SCHEDULE, MPI_COMM_WORLD);
            /* send new schedule. */
            MPI_Send(__eskel_rt.new_task2node, __eskel_rt.nleaves,
                     MPI_INT, __eskel_rt.curr_task2node[n->sil.l[0]],
                     ESKEL_NEW_SCHEDULE, MPI_COMM_WORLD);
        } else {
            /* Send data. */
            MPI_Send(buffer, units, n->output_dtype,
                     __eskel_rt.curr_task2node[n->sil.l[0]],
                     ESKEL_DATA, MPI_COMM_WORLD);
        }
        break;
    case DEAL:
        if (__eskel_reschedule_flag) {
            /* Send the data unit (product) to the current task due to
               round-robin data distribution. */
            MPI_Send(buffer, units, n->output_dtype,
                     __eskel_rt.curr_task2node[n->sil.l[current]],
                     ESKEL_DATA_SCHEDULE, MPI_COMM_WORLD);
            MPI_Send(__eskel_rt.new_task2node, __eskel_rt.nleaves, MPI_INT,
                     __eskel_rt.curr_task2node[n->sil.l[current]],
                     ESKEL_NEW_SCHEDULE, MPI_COMM_WORLD);

            /* To the rest of the task, send the schedule only. */
            for (i = 0; i < n->sil.n; i++)
                if (i != current) {
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
                    /* Send the schedule. */
                    MPI_Send(__eskel_rt.new_task2node, __eskel_rt.nleaves,
                             MPI_INT, __eskel_rt.curr_task2node[n->sil.l[i]],
                             ESKEL_NEW_SCHEDULE, MPI_COMM_WORLD);
                }
            current = 0;
        } else {
            /* Send data as normal. */
            MPI_Send(buffer, units, n->output_dtype,
                     __eskel_rt.curr_task2node[n->sil.l[current]],
                     ESKEL_DATA, MPI_COMM_WORLD);
            if (++current == n->sil.n) current = 0;
        }
        break;
    case TASK:
    case UNKNOWN:
    case NSKEL:
        break;        
    }
    return ESKEL_DONT_RESCHEDULE;    
}

/* Send data to the next stage (case without scheduling). */
int __eskel_give_without_scheduling(void *buffer, int units) {
    static int current = 0;
    __eskel_node_t *n;

    /* Which stage am I executing. */
    n = __eskel_rt.sstab[__eskel_rt.curr_node2task[__eskel_rt.rank]];
    
    /* If I am not the first stage, just give data to the next stage
       with the scheduling information if I received any from the
       previous stage. */
    switch (n->stype) {
    case PIPE:
        /* Send data. */
        MPI_Send(buffer, units, n->output_dtype,
                 __eskel_rt.curr_task2node[n->sil.l[0]],
                 ESKEL_DATA, MPI_COMM_WORLD);
        break;
    case DEAL:
        MPI_Send(buffer, units, n->output_dtype,
                 __eskel_rt.curr_task2node[n->sil.l[current]],
                 ESKEL_DATA, MPI_COMM_WORLD);
        if (++current == n->sil.n) current = 0;
        break;
    case TASK:
    case UNKNOWN:
    case NSKEL:
        break;        
    }
    return ESKEL_DONT_RESCHEDULE;    
}
