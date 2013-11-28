/*********************************************************************

  THE ENHANCE PROJECT
  School of Informatics,
  University of Edinburgh,
  Edinburgh - EH9 3JZ
  United Kingdom

  Written by: Gagarine Yaikhom

*********************************************************************/

#include <eskel.h>

/* Creates the data structures for maintaining the source-sink lookup
   table. The contents of the table are as follows:
   
   1. The current task to node (or processor) assignment vector.
   2. The current node to task assignment vector.
   3. The new task to node (or processor) assignment vector.
   4. The new node to task assignment vector.
   
   The last two vectors are used while rescheduling. */
static int __eskel_create_ltab(void) {
    if (!__eskel_rt.sstab)
        return -1;
    if (!(__eskel_rt.curr_task2node =
          (int *) __eskel_malloc(sizeof(int)*__eskel_rt.nleaves)))
        return -1;
    if (!(__eskel_rt.curr_node2task =
          (int *) __eskel_malloc(sizeof(int)*__eskel_rt.nproc))) {
        __eskel_free(__eskel_rt.curr_task2node);
        return -1;
    }
    if (!(__eskel_rt.new_task2node =
          (int *) __eskel_malloc(sizeof(int)*__eskel_rt.nleaves))) {
        __eskel_free(__eskel_rt.curr_node2task);
        __eskel_free(__eskel_rt.curr_task2node);
        return -1;
    }
    if (!(__eskel_rt.new_node2task =
          (int *) __eskel_malloc(sizeof(int)*__eskel_rt.nproc))) {
        __eskel_free(__eskel_rt.new_task2node);
        __eskel_free(__eskel_rt.curr_node2task);
        __eskel_free(__eskel_rt.curr_task2node);
        return -1;
    }
    return 0;
}

/* Destroy all of the lookup tables. */
static void __eskel_destroy_ltabs(void) {
    __eskel_free(__eskel_rt.sstab);
    __eskel_free(__eskel_rt.curr_task2node);
    __eskel_free(__eskel_rt.curr_node2task);
    __eskel_free(__eskel_rt.new_task2node);
    __eskel_free(__eskel_rt.new_node2task);
}

/* Initialise the skeleton library. This is the first function called
   by any application using the eskel library. */
int eskel_init(int *argc, char ***argv) {
    /* Get the process ID. This is used while spawning and monitoring
       resource utilisation by this application (see "rmon.c"). */
    __eskel_rt.pid = getpid();

    /* Initialise the MPI library. */
    MPI_Init(argc, argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &__eskel_rt.rank);
    MPI_Comm_size(MPI_COMM_WORLD, &__eskel_rt.nproc);

#ifdef __ESKEL_DEBUG_SCHED
    printf("[0] sched - Node %d has pid %d\n",
           __eskel_rt.rank, __eskel_rt.pid);
#endif

    /* Now spawn the resource monitor thread. */
    eskel_rmon_spawn(&__eskel_rt.rmon_thread);
    return 0;
}

/* This function commits the current skeleton hierarchy tree. This
   means that the tasks have been organised completely, and hence, the
   execution of the tasks can be initiated.

   TODO:
   At the moment, only one skeleton hierarchy tree is allowed. This is
   not a requirement: in future implementations, multiple skeleton
   hierarchy tree should be supported. */
int eskel_htree_commit(void) {
    /* Generate the source-sink table. This defines the inter-task
       data dependencies (the data flow graph) from the skeleton
       hierarchy tree. The __ESKEL_SOURCE_IFACE and
       __ESKEL_SINK_IFACE are dummy interfaces for interacting with
       external environment. They do not contribute anything to the
       execution. */
    __eskel_htree_gen_sstab(__eskel_rt.skel_tree,
                            __ESKEL_SOURCE_IFACE,
                            __ESKEL_SINK_IFACE);

    /* Create the task-to-node and node-to-task lookup table.  */
    __eskel_create_ltab();
    return 0;
}

/* Finalise the skeleton library. This function should be called
   before the application exits. */
int eskel_final(void) {
    /* Destroy the skeleton hierarchy tree. */
    __eskel_htree_destroy(__eskel_rt.skel_tree);

    /* Check if the execution was downgraded to execution without
       dynamic scheduling support (due to failure in scheduler
       initialisation). If everything went well, don't forget to
       finalise the resource monitor thread. */
    if (__eskel_sched_handler)
        eskel_rmon_final(&__eskel_rt.rmon_thread);

    __eskel_destroy_ltabs(); /* Destroy all of the lookup tables. */
    
#ifdef __ESKEL_DEBUG_MEMORY
    __eskel_x_check_leaks(0);
#endif

    MPI_Finalize();    /* We are now ready to finalise the MPI. */
    return 0;
}

