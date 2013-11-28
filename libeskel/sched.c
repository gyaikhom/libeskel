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

#include <eskel.h>

static int __eskel_sched_do_scheduling = -1;

/* Translate task-to-node vector (the schedule vector) to the
   corresponding node-to-task vector, which is used to make execution
   decision. */
static int __eskel_task2node_node2task(const int *task2node,
                                       int *node2task) {
    register int i;

    /* Assume that all the nodes are not assigned with any task. */
    for (i = 0; i < __eskel_rt.nproc; i++)
        node2task[i] = ESKEL_NO_TASK;

    /* Fix the assumption with the correct values. */
    for (i = 0; i < __eskel_rt.nleaves; i++)
        node2task[task2node[i]] = i;
    return 0;
}

/* Update the current schedule with the recently received schedule. */
static int __eskel_update_schedule(void) {
    register int i;
    for (i = 0; i < __eskel_rt.nleaves; i++)
        __eskel_rt.curr_task2node[i] = __eskel_rt.new_task2node[i];
    __eskel_task2node_node2task(__eskel_rt.curr_task2node,
                                __eskel_rt.curr_node2task);
    return 0;
}

/* Start the realtime scheduler. */
static int __eskel_scheduler(void) {
    int i = 0, j, index, flag = 0;
    char dummy = 'A';
    MPI_Request notices[2];
    MPI_Status status;
    double duration;
    static int current_task = 0;

    /* Receive acknowledgement from the last stage of the
       initial schedule that was dispatched with a broadcast. */
    MPI_Recv(&dummy, 1, MPI_CHAR, MPI_ANY_SOURCE,
             ESKEL_ACKNOWLEDGEMENT, MPI_COMM_WORLD, &status);
#ifdef __ESKEL_DEBUG_SCHEDULER
    printf("[%d] sched - Acknowledgement "
           "received from t[%d] on n[%d].\n",
           ESKEL_SCHEDULER,
           __eskel_rt.curr_node2task[status.MPI_SOURCE],
           status.MPI_SOURCE);
#endif

    /* Update current schedule with the new schedule. */
    __eskel_update_schedule();
    
    /* Send continuation signal to the process executing
       the first stage in the new schedule. By the time this
       section of the code is executed, __eskel_rt.curr_task2node
       has already been updated with the new schedule. */
#ifdef __ESKEL_DEBUG_SCHEDULER
    printf("[%d] sched - Sending continuation signal "
           "to t[0] on n[%d].\n",
           ESKEL_SCHEDULER, __eskel_rt.curr_task2node[0]);
#endif
    MPI_Send(&dummy, 1, MPI_CHAR, __eskel_rt.curr_task2node[0],
             ESKEL_CONTINUE, MPI_COMM_WORLD);

    /* Initiate a non-blocking receive for receiving completion
       signal from any of the worker processes, which signal
       completion of execution. */
    MPI_Irecv(&dummy, 1, MPI_CHAR, MPI_ANY_SOURCE,
              ESKEL_COMPLETED, MPI_COMM_WORLD, &notices[0]);

    /* If the scheduler was not initialised properly, default to
       no dynamic scheduling. Wait until the completion signal
       is received. Even though a scheduler may be specified by the
       programmer, if it was not initialised properly, there is no
       use. This is checked at the initialisation phase below in
       eskel_exec().

       NOTE: Why then initiate the above non-blocking receive?
       This is because the head task does not know that the scheduler
       was not initialised properly. So, it will be sending a
       completion signal is all cases. */
    if (!__eskel_sched_handler) {
#ifdef __ESKEL_DEBUG_SCHEDULER
        printf("[0] sched - Dynamic scheduling disabled.\n");
#endif
        MPI_Wait(&notices[0], MPI_STATUS_IGNORE);
        goto terminate;
    }

    /* Enter the scheduling cycle. */
    while (1) {
        MPI_Test(&notices[0], &flag, MPI_STATUS_IGNORE);
        if (flag) goto terminate;

        /* Fix task duration so that it is not zero. */
        if (current_task < __eskel_rt.nleaves) {
            MPI_Recv(&duration, 1, MPI_DOUBLE, 1, 12345,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (duration == 0.0)
                __eskel_rt.sstab[current_task]->duration = 1.0E-9;
            else
                __eskel_rt.sstab[current_task]->duration = duration;

#ifdef __ESKEL_DEBUG_SCHEDULER
            printf("[0] sched - Task %d duration is %f seconds.\n",
                   current_task, __eskel_rt.sstab[current_task]->duration);
#endif
            current_task++;

            /* Generate the new schedule for task timing. */
            j = 1;
            for (i = current_task; i < __eskel_rt.nleaves; i++)
                __eskel_rt.new_task2node[i] = j++;
            for (i = 0; i < current_task; i++)
                __eskel_rt.new_task2node[i] = j++;
#ifdef __ESKEL_DEBUG_PEPA
            for (i = 0; i < __eskel_rt.nleaves; i++)
                printf("%d", __eskel_rt.new_task2node[i]);            
            printf("\n");
#endif
        } else {
            if (__eskel_sched_handler(__eskel_rt.new_task2node)) {
#ifdef __ESKEL_DEBUG_SCHEDULER
                printf("[%d] sched - Same schedule, re-evaluate resources.\n",
                       ESKEL_SCHEDULER);
#endif
                continue;
            }
        }
            
        /* Send new schedule to the process executing the first
           task in the current schedule. */
#ifdef __ESKEL_DEBUG_SCHEDULER
        printf("[%d] sched - Sending schedule to t[0] on n[%d]\n",
               ESKEL_SCHEDULER, __eskel_rt.curr_task2node[0]);
#endif
        MPI_Send(__eskel_rt.new_task2node, __eskel_rt.nleaves, MPI_INT,
                 __eskel_rt.curr_task2node[0], ESKEL_NEW_SCHEDULE,
                 MPI_COMM_WORLD);

        /* Receive acknowledgement from the last stage of the
           previously dispatched schedule; or the current first stage
           (in case the schedule was the same as the previous one. */
        MPI_Irecv(&dummy, 1, MPI_CHAR, MPI_ANY_SOURCE,
                  ESKEL_ACKNOWLEDGEMENT, MPI_COMM_WORLD, &notices[1]);

        /* We have to monitor two non-blocking initiations:
           (1) To get execution completion signal.
           (2) To get acknowledgement from the last stage. */
#ifdef __ESKEL_DEBUG_SCHEDULER
        printf("[%d] sched - Waiting for completion signal/acknowledgement.\n",
               ESKEL_SCHEDULER);
#endif
        MPI_Waitany(2, notices, &index, &status);

        /* Which non-blocking initiation was completed. */
        switch (index) {
        case 0: /* If completion of execution. */
            MPI_Cancel(&notices[1]); /* Cancel acknowledgement. */
            goto terminate;
        case 1:
#ifdef __ESKEL_DEBUG_SCHEDULER
            printf("[%d] sched - Acknowledgement "
                   "received from t[%d] on n[%d].\n",
                   ESKEL_SCHEDULER,
                   __eskel_rt.curr_node2task[status.MPI_SOURCE],
                   status.MPI_SOURCE);
#endif
            /* Update current schedule with the new schedule. */
            __eskel_update_schedule();

            /* Send continuation signal to the process executing
               the first stage in the new schedule. By the time this
               section of the code is executed, __eskel_rt.curr_task2node
               has already been updated with the new schedule. */
#ifdef __ESKEL_DEBUG_SCHEDULER
            printf("[%d] sched - Sending continuation signal "
                   "to t[0] on n[%d].\n",
                   ESKEL_SCHEDULER, __eskel_rt.curr_task2node[0]);
#endif
            MPI_Send(&dummy, 1, MPI_CHAR, __eskel_rt.curr_task2node[0],
                     ESKEL_CONTINUE, MPI_COMM_WORLD);
            break;
        }
    }
 terminate:
#ifdef __ESKEL_DEBUG_SCHEDULER
    printf("[%d] sched - Completion signal received "
           "from t[%d] on n[%d].\n", ESKEL_SCHEDULER,
           __eskel_rt.curr_node2task[status.MPI_SOURCE],
           status.MPI_SOURCE);
#endif

#ifdef __ESKEL_DEBUG_SCHEDULER
    printf("[%d] sched - Scheduler terminating.\n", ESKEL_SCHEDULER);
#endif

    /* Wake up all the processes that are in the wait state of the
       previous schedule and inform them that the application has
       completed execution. */
    for (i = 1; i < __eskel_rt.nproc; i++)
        if (__eskel_rt.curr_node2task[i] == ESKEL_NO_TASK) {
#ifdef __ESKEL_DEBUG_SCHEDULER
            printf("[%d] sched - Waking up n[%d] for exit.\n",
                   ESKEL_SCHEDULER, i);
#endif
            MPI_Send (__eskel_rt.new_task2node, __eskel_rt.nleaves,
                      MPI_INT, i, ESKEL_ACCEPTED, MPI_COMM_WORLD);
        }
    return 0;
}

/* Execute the task assigned to this node. */
extern double __eskel_task_duration;
static int __eskel_sched_exec(void) {
    char dummy = 'A'; /* Used for sending acknowledgements. */
    int result;

    __eskel_reschedule_flag = 0;

    /* Start executing the task assigned to this node. */
#ifdef __ESKEL_DEBUG_SCHEDULER
    printf ("[%d] sched - Executing task %d.\n",
            __eskel_rt.rank, __eskel_rt.curr_node2task[__eskel_rt.rank]);
#endif

    /* If the process is assigned to execute the first stage, it
       should wait for continuation signal from the scheduler. */
    if (__eskel_rt.curr_node2task[__eskel_rt.rank] == 0) {
#ifdef __ESKEL_DEBUG_SCHEDULER
        printf("[%d] sched - Waiting for continuation signal.\n",
               __eskel_rt.rank);
#endif
        MPI_Recv(&dummy, 1, MPI_CHAR,
                 ESKEL_SCHEDULER, ESKEL_CONTINUE,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#ifdef __ESKEL_DEBUG_SCHEDULER
        printf("[%d] sched - Continuation signal received from "
               "scheduler.\n",
               __eskel_rt.rank);
#endif
        /* The first stage process should initiate a non-blocking
           receive for receiving new schedule. */
        MPI_Irecv(__eskel_rt.new_task2node, __eskel_rt.nleaves,
                  MPI_INT, ESKEL_SCHEDULER, ESKEL_NEW_SCHEDULE,
                  MPI_COMM_WORLD, &__eskel_sched_listener);
    } else if (__eskel_rt.curr_node2task[__eskel_rt.rank] ==
               (__eskel_rt.nleaves - 1)) {
        /* The process executing the last stage should send an
           acknowledgement for the new schedule to the
           scheduler. This synchronises all the process that are
           going to participate in the new schedule. */ 
#ifdef __ESKEL_DEBUG_SCHEDULER
        printf("[%d] sched - Sending acknowledgement.\n",
               __eskel_rt.rank);
#endif
        MPI_Send(&dummy, 1, MPI_CHAR, ESKEL_SCHEDULER,
                 ESKEL_ACKNOWLEDGEMENT, MPI_COMM_WORLD);
    }

    /* Execute the task function. */
    __eskel_task_duration = 0.0;
    result = __eskel_rt.sstab[__eskel_rt.curr_node2task[__eskel_rt.rank]]->function();
    if (result == ESKEL_COMPLETED) {
#ifdef __ESKEL_DEBUG_SCHEDULER
        printf("[%d] sched - Completed executing task %d.\n",
               __eskel_rt.rank,
               __eskel_rt.curr_node2task[__eskel_rt.rank]);
#endif
        if (__eskel_rt.curr_node2task[__eskel_rt.rank] == 0) {
            /* Inform the scheduler that execution has completed and
               there is no need for further scheduling. */
            MPI_Send(&dummy, 1, MPI_CHAR, ESKEL_SCHEDULER,
                     ESKEL_COMPLETED, MPI_COMM_WORLD);
            /* Cancel listener for receiving new schedule from scheduler. */
            if (__eskel_sched_listener)
                MPI_Cancel(&__eskel_sched_listener);
        }
        return ESKEL_COMPLETED;
    } else {
        /* There was a reschedule. So, cancel the current listener
           due to previous non-blocking receive, so that a new
           schedule listener can be initiated by the task which will
           now execute the first task. */
        if ((__eskel_rt.curr_node2task[__eskel_rt.rank] == 0) &&
            __eskel_sched_listener)
            MPI_Cancel(&__eskel_sched_listener);
        return ESKEL_RESCHEDULE;
    }
}

/* Enter the wait state. Before entering the wait state, old state
   values are sent to the node replacing this node. A blocking
   receive is then issued. This receive call serves two purposes:
   (1) If data was received from the scheduler, the eskel application
   has completed execution. There is nothing left to be done, hence I
   can exit.
   (2) If data is received from a process X, I am going to replace
   that process based on the new schedule which I have just
   received. I should therefore return so that I can be scheduled with
   the next task. */
static int __eskel_sched_wait(void) {
    MPI_Status status;
    int nstage; /* The new task if I an rescheduled with a task. */

#ifdef __ESKEL_DEBUG_SCHEDULER
    printf ("[%d] sched - Entering wait state.\n", __eskel_rt.rank);
#endif
    __eskel_reschedule_flag = 0;

    /* Before entering the wait state, send my old state to the
       process replacing me. This is called separately because
       __eskel_state_send_recv() is only invoked when
       eskel_state_commit() is call (See state.c source file). */
    if ((__eskel_state.replacer != -1) && (__eskel_state.curr_head)) {
#ifdef __ESKEL_DEBUG_SCHEDULER
        printf("[%d] sched - I am replacing n[%d], my replacer is n[%d]\n",
               __eskel_rt.rank, __eskel_state.replacing,
               __eskel_state.replacer);
#endif
        __eskel_state_send();
        __eskel_state_release(__eskel_state.curr_head, 1);
    }
    __eskel_state.curr_head = NULL;
    __eskel_state.curr_tail = NULL;

    /* Issue a blocking receive for receiving a new schedule vector. */
    MPI_Recv(__eskel_rt.new_task2node, __eskel_rt.nleaves,
             MPI_INT, MPI_ANY_SOURCE, ESKEL_ACCEPTED,
             MPI_COMM_WORLD, &status);

    /* Until this point, the node was sleeping in order to be woken up
       either by the scheduler or one of the nodes. Once this point is
       reached, we know that the node should now wake up and decide
       what to do next: exit or reschedule. */
    if (status.MPI_SOURCE == ESKEL_SCHEDULER) {
#ifdef __ESKEL_DEBUG_SCHEDULER
        printf("[%d] sched - Woken up by scheduler; therefore exit.\n",
               __eskel_rt.rank);
#endif
        return ESKEL_COMPLETED;
    } else {
        /* The node should reschedule. Translate the received schedule
           vector to the corresponding node2task vector. */
        __eskel_task2node_node2task(__eskel_rt.new_task2node,
                                    __eskel_rt.new_node2task);

        /* Now, receive the old schedule vector which was existing
           before this node was woken up. Remember, since the node
           went into sleep, a number of arbitrary reschedules which
           does not include this node might have occured. */
        MPI_Recv(__eskel_rt.curr_task2node, __eskel_rt.nleaves,
                 MPI_INT, MPI_ANY_SOURCE, ESKEL_ACCEPTED,
                 MPI_COMM_WORLD, &status);
        __eskel_task2node_node2task(__eskel_rt.curr_task2node,
                                    __eskel_rt.curr_node2task);

        /* Which task should I execute. */
        nstage = __eskel_rt.new_node2task[__eskel_rt.rank];

        /* Who am I going to replace. */
        __eskel_state.replacer = -1;
        __eskel_state.replacing = __eskel_rt.curr_task2node[nstage];

#ifdef __ESKEL_DEBUG_SCHEDULER
        printf("[%d] sched - Woken up by t[%d] on n[%d].\n",
               __eskel_rt.rank,
               __eskel_rt.curr_node2task[__eskel_state.replacing],
               __eskel_state.replacing);
#endif
        return ESKEL_RESCHEDULE; /* Return to the worker cycle. */
    }
}

/* The worker cycle: After receiving a new schedule from the
   scheduler, decisions are made whether to enter the execution state,
   or to enter the wait state. */
static int __eskel_worker(void) {
    int result;

    /* Keep working until completion. */
    do {
        /* Update my schedule with the new schedule. */
        __eskel_update_schedule();

        /* Make the decisison. */
        if (__eskel_rt.curr_node2task[__eskel_rt.rank] == ESKEL_NO_TASK) {
            result = __eskel_sched_wait();
            if (result == ESKEL_COMPLETED) return ESKEL_COMPLETED;
        } else {
            result = __eskel_sched_exec();
            if (result == ESKEL_COMPLETED) return ESKEL_COMPLETED;
        }
    } while(1);
    return ESKEL_COMPLETED;
}

/* This function begins execution of the tasks in the skeleton
   hierarchy tree. There are two main divisions here: scheduler path
   which is taken by the scheduler, and the worker path: which is
   taken by all the nodes that are worker nodes. */
extern int __eskel_summary_request;
extern time_t __eskel_exec_time[2];
int eskel_exec(int flag) {
    int i;
    if (__eskel_rt.rank == ESKEL_SCHEDULER) {
        if (__eskel_sched_handler) {
            /* Request execution and scheduling information */
            __eskel_summary_request = flag;

            if (__eskel_summary_request)
                time(&__eskel_exec_time[0]);

            /* Initialise the schedule handler. */
            if (__eskel_sched_init) {
                /* Initialise the scheduler. If failure, default to
                   execution without dynamic scheduling support. */
                if (__eskel_sched_init() == -1) {
                    __eskel_sched_handler = NULL;

                    /* Resource monitoring is not necessary. */
                    eskel_rmon_final(&__eskel_rt.rmon_thread);

                    /* Cancel request for execution and scheduling information */
                    __eskel_summary_request = 0;
                } else {
                    /* Initialise the summary file. */
                    if (__eskel_summary_request) {
                        __eskel_init_summary("eskel.summary");
                    }
                }
            }
            
            /* Get the starting schedule. */
            /*         __eskel_pepa_schedule(__eskel_rt.stage2proc); */
            /*         dummy_scheduler(__eskel_rt.new_task2node); */
            for (i = 0; i < __eskel_rt.nleaves; i++)
                __eskel_rt.new_task2node[i] = i+1;

            /* For the first time, broadcast the starting schedule
               to all the available processors. */
        } else {
            /* Assume that the task are assigned to the nodes
               consecutively from 1 to the number of tasks.
               
               FIXME: When scheduling is not requested, it is not wise
               to still allocate node 0 to the scheduler. */
            for (i = 0; i < __eskel_rt.nleaves; i++)
                __eskel_rt.new_task2node[i] = i+1;
        }
        MPI_Bcast(__eskel_rt.new_task2node, __eskel_rt.nleaves,
                  MPI_INT, ESKEL_SCHEDULER, MPI_COMM_WORLD);
        __eskel_task2node_node2task(__eskel_rt.new_task2node,
                                    __eskel_rt.new_node2task);

        __eskel_scheduler(); /* Scheduler cycle. Returns when done. */

        if (__eskel_sched_handler) {
            /* Finalise the summary file. */
            if (__eskel_summary_request) {
                time(&__eskel_exec_time[1]);
                __eskel_write_env();
                __eskel_write_exe();
                __eskel_write_ske();
                __eskel_final_summary();
            }
            if (__eskel_sched_final)
                __eskel_sched_final(); /* Finalises the scheduler. */
        }
    } else {
        /* Get the starting schedule from the scheduler. */
        MPI_Bcast(__eskel_rt.new_task2node, __eskel_rt.nleaves,
                  MPI_INT, ESKEL_SCHEDULER, MPI_COMM_WORLD);
        __eskel_task2node_node2task(__eskel_rt.new_task2node,
                                    __eskel_rt.new_node2task);
        __eskel_worker(); /* Enter the worker cycle. */
    }
    /*     MPI_Barrier(MPI_COMM_WORLD); */
    return 0;
}

/* Check if I need rescheduling. */
int __eskel_check_rescheduling(void) {
    int nstage; /* New stage. */
    int cstage; /* Current stage. */
    int rstage; /* Replacer stage. */
    char dummy = 'A';

    __eskel_task2node_node2task(__eskel_rt.new_task2node,
                                __eskel_rt.new_node2task);

    /* What is my current and new stage. */
    cstage = __eskel_rt.curr_node2task[__eskel_rt.rank];
    nstage = __eskel_rt.new_node2task[__eskel_rt.rank];

    /* Who is going to replace me, and who am I going to replace. */
    __eskel_state.replacer = __eskel_rt.new_task2node[cstage];
    if (nstage != ESKEL_NO_TASK)
        __eskel_state.replacing = __eskel_rt.curr_task2node[nstage];
    else __eskel_state.replacing = -1;

    /* What was the replacer executing before this new schedule. */
    rstage = __eskel_rt.curr_node2task[__eskel_state.replacer];

    /* Am I rescheduled with the same stage. */
    if (cstage == nstage) {
        /* Update my schedule with the new schedule. */
        __eskel_update_schedule();

        /* If I am the last stage, send acknowledgement to
           the scheduler. */
        if (nstage == (__eskel_rt.nleaves - 1)) {
#ifdef __ESKEL_DEBUG_SCHEDULER
            printf("[%d] sched - Sending acknowledgement.\n", __eskel_rt.rank);
#endif
            MPI_Send(&dummy, 1, MPI_CHAR, ESKEL_SCHEDULER,
                     ESKEL_ACKNOWLEDGEMENT, MPI_COMM_WORLD);
        } else if (nstage == 0) {
#ifdef __ESKEL_DEBUG_SCHEDULER
            printf("[%d] sched - Waiting for continuation.\n", __eskel_rt.rank);
#endif
            MPI_Recv(&dummy, 1, MPI_CHAR, ESKEL_SCHEDULER,
                     ESKEL_CONTINUE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#ifdef __ESKEL_DEBUG_SCHEDULER
            printf("[%d] sched - Continuation received.\n",
                   __eskel_rt.rank);
#endif
            /* The first stage process should initiate a non-blocking
               receive for receiving new schedule. */
            MPI_Irecv(__eskel_rt.new_task2node, __eskel_rt.nleaves, MPI_INT,
                      ESKEL_SCHEDULER, ESKEL_NEW_SCHEDULE,
                      MPI_COMM_WORLD, &__eskel_sched_listener);
        }
        /* Reset the replacing and replace to -1. */
        __eskel_state.replacer = -1;
        __eskel_state.replacing = -1;

        /* I should not be rescheduled. Therefore continue as it was
           before the new schedule arrived. */
        return ESKEL_DONT_RESCHEDULE;
    } else {
        /* Is the process replacing me in the wait state? If it is
           wake it up by supplying the new schedule. */
        if (rstage == ESKEL_NO_TASK) {
#ifdef __ESKEL_DEBUG_SCHEDULER
            printf("[%d] sched - Waking up n[%d].\n",
                   __eskel_rt.rank, __eskel_state.replacer);
#endif
            MPI_Send(__eskel_rt.new_task2node, __eskel_rt.nleaves,
                     MPI_INT, __eskel_state.replacer,
                     ESKEL_ACCEPTED, MPI_COMM_WORLD);
            MPI_Send(__eskel_rt.curr_task2node, __eskel_rt.nleaves,
                     MPI_INT, __eskel_state.replacer,
                     ESKEL_ACCEPTED, MPI_COMM_WORLD);
#ifdef __ESKEL_DEBUG_SCHEDULER
            printf("[%d] sched - n[%d] has woken up.\n",
                   __eskel_rt.rank, __eskel_state.replacer);
#endif
        }
        return ESKEL_RESCHEDULE; /* I should be rescheduled. */
    }
}

/* Set the scheduler function. This function is invoked whenever a new
   schedule vector is needed. */
extern int eskel_set_scheduler(eskel_sched_handler_t sched_handler,
                               eskel_sched_init_t sched_init,
                               eskel_sched_final_t sched_final) {
    if (__eskel_sched_do_scheduling != -1)
        return 0; /* Allowed to set scheduler only once. */

    if (sched_handler == NULL) {
        /* We do not want scheduling support. */
        __eskel_sched_do_scheduling = 0;

        /* Set the give and take handler. */
        __eskel_give = __eskel_give_without_scheduling;
        __eskel_take = __eskel_take_without_scheduling;
    } else {
        __eskel_sched_do_scheduling = 1;
        __eskel_sched_init = sched_init;
        __eskel_sched_handler = sched_handler;
        __eskel_sched_final = sched_final;

        /* Set the give and take handler. */
        __eskel_give = __eskel_give_with_scheduling;
        __eskel_take = __eskel_take_with_scheduling;
    }
    return 0;
}

extern unsigned int __eskel_sched_period;
void eskel_set_schedule_period(unsigned int secs) {
    __eskel_sched_period = secs;
}
