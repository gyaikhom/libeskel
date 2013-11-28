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
  Registration and transfer of state from previously scheduled process
  to the newly scheduled process.

  Written by: Gagarine Yaikhom (g.yaikhom@sms.ed.ac.uk)
  School of Informatics, University of Edinburgh
*/

#include <eskel.h>

/* The state of a task in execution is maintained as a linked list on
   the process that is currently executing the task. During
   rescheduling, if the process is assigned with a different task, the
   process will have two linked lists representing the two states:
   (1) The state of the old task which it was executing before the new
   schedule arrived, and (2) The state of the new task which it should
   now take over; which means the current state of this new task
   should be taken from the process which executed the task
   proviously. In the implementation, we maintain these two linked
   list using two pairs of head and tail pointers. */
struct __eskel_state_s __eskel_state = {
    .curr_head = NULL, .curr_tail = NULL,
    .new_head = NULL,  .new_tail = NULL,
    .replacing = -1,   .replacer = -1
};

/* Send the state to the replacer process. This function may be called
   by a process entering a wait state, since it will never execute
   __eskel_send_recv_state() which is invoked within the task when
   eskel_state_commit() is invoked. */
int __eskel_state_send(void) {
    __eskel_state_elem_t *cursor;
    if (__eskel_state.curr_head) {
#ifdef __ESKEL_DEBUG_STATE
        printf("[%d] state - Sending state to %d\n",
               __eskel_rt.rank, __eskel_state.replacer);
#endif
        /* Traverse the state variable linked list from head to tail,
           and send the current data stored in the state variable
           associated with the node. Continue till all the nodes have
           been traversed. */
        cursor = __eskel_state.curr_head;
        while (cursor) {
            MPI_Send(cursor->state_ptr, cursor->state_size,
                     MPI_CHAR, __eskel_state.replacer,
                     ESKEL_STATE, MPI_COMM_WORLD);
            cursor = cursor->next;
        }
#ifdef __ESKEL_DEBUG_STATE
        printf("[%d] state - State sent to %d\n",
               __eskel_rt.rank, __eskel_state.replacer);
#endif
        return 0;
    }
    return -1; /* This should never happen. */
}

/* Receive state from the replacing process (the previous process
   executing the task now assigned to this process). */
static int __eskel_state_recv(void) {
    __eskel_state_elem_t *cursor;
    if (__eskel_state.new_head) {
#ifdef __ESKEL_DEBUG_STATE
        printf("[%d] state - Receiving state from %d\n",
               __eskel_rt.rank, __eskel_state.replacing);
#endif
        /* Traverse the state variable linked list from head to tail,
           and receive the current data stored in the state variable
           associated with the node on the previous process. Continue
           till all the nodes have been traversed. */
        cursor = __eskel_state.new_head;
        while (cursor) {
            MPI_Recv(cursor->state_ptr, cursor->state_size,
                     MPI_CHAR, __eskel_state.replacing,
                     ESKEL_STATE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            cursor = cursor->next;
        }
#ifdef __ESKEL_DEBUG_STATE
        printf("[%d] state - State received from %d\n",
               __eskel_rt.rank, __eskel_state.replacing);
#endif
        return 0;
    }
    return -1; /* This should never happen. */
}

/* Send my state if I am being replaced, and receive new state if I am
   replacing a process. */
static int __eskel_state_send_recv(void) {
    /* When replacing is != -1, the current process will be scheduled
       with a new task. The sending of old state and receiving of new
       state will therefore happen when this process starts executing
       the new task. If the process will not be rescheduled with a new
       task (going to enter the wait state), the sending of old state
       is done outside this function, since this function is only
       called from the new task. Check __eskel_wait_state() in sched.c
       source file. */
#ifdef __ESKEL_DEBUG_STATE
    printf("[%d] state - Replacing: %d, Replacer: %d\n", __eskel_rt.rank,
           __eskel_state.replacing, __eskel_state.replacer);
#endif
    if ((__eskel_state.replacing != -1) &&
        (__eskel_state.replacer != -1)) {
        /* Changing task. */
        if (__eskel_state.replacing == __eskel_state.replacer) {
            /* Exchanging task. The following condition is used to
               introduce asymmetry into the algorithm for avoiding
               deadloack. */
            if (__eskel_rt.rank < __eskel_state.replacing) {
                __eskel_state_send();
                __eskel_state_recv();
            } else {
                __eskel_state_recv();
                __eskel_state_send();
            }
        } else {
            /* Not exchange, however cycles leading to deadlock may
               occur. The following condition is used to introduce
               asymmetry into the algorithm for avoiding deadloacks
               due to cycles. */
            if (__eskel_state.replacing < __eskel_state.replacer) {
                __eskel_state_recv();
                __eskel_state_send();
            } else {
                __eskel_state_send();
                __eskel_state_recv();
            }
        }
    } else if (__eskel_state.replacing != -1) {
        __eskel_state_recv();
    } else if (__eskel_state.replacer != -1) {
        __eskel_state_send();
    }

    /* Reset the replacing and replace to -1. */
    __eskel_state.replacer = -1;
    __eskel_state.replacing = -1;
    return 0;
}

/* Release and destroy the state variable linked list. This function
   is called after sending the current state, and therefoe may also be
   called by a process entering a wait state. If the flag is on,
   transient memory is not deallocated. This flag is not set when called
   from eskel_end(). */
int __eskel_state_release(__eskel_state_elem_t *head, int flag) {
    __eskel_state_elem_t *cursor;
    if (head) {
        /* Traverse the linked list and destroy each node. */
        while (head) {
            cursor = head->next;
            if ((head->mem_type == ESKEL_TRANSIENT) && flag)
                __eskel_free(head->state_ptr);
            __eskel_free(head);
            head = cursor;
        }
        return 0;
    }
    return -1;
}

/* Register a state with the skeleton runtime system. The first
   argument gives the pointer to the beginning of the memory location
   which defines the state, the second argument gives the size (in
   bytes) of the state, and the final argument gives the type of
   memory which the first argument points to. Since state can be
   either compiler or programmer allocated, the later type of memory
   should be automatically deallocated by the runtime system after a
   rescheduling. */
int eskel_state_register(void *state_ptr, size_t state_size,
                         eskel_state_type_t mem_type) {
    __eskel_state_elem_t *node = NULL;
    if ((state_ptr == NULL) || (state_size <= 0)) return -1;

    /* Create new state variable node. */
    if (!(node = (__eskel_state_elem_t *)
          __eskel_malloc(sizeof(struct __eskel_state_elem_s))))
        return -1;

    /* Initialise the state variable node. */
    node->state_ptr = state_ptr;
    node->state_size = state_size;
    node->mem_type = mem_type;
    node->next = NULL;

    /* Insert new state variable node into the linked list. */
    if (__eskel_state.new_head == NULL) {
        __eskel_state.new_tail = node;
        __eskel_state.new_head = node;
    } else {
        __eskel_state.new_tail->next = node;
        __eskel_state.new_tail = node;
    }
    return 0;
}

/* Commit the state variable linked list. */
int eskel_begin(void) {
    /* Send my old state to the process replacing me, and receive new
       state from the process I am replacing. It is important for this
       to happen only when all the state variables have been
       registered because send_recv_state() avoids deadlock. It will
       be fairly inefficient to do this for every registered state;
       without having any benefit (it is usual that all the state for
       a task is usually defined before performing the task
       function). */
    __eskel_state_send_recv();

    /* If I was previously scheduled with some task, the old state
       values for that task have been sent to the new process taking
       over that task. We can therefore destroy the current state
       linked list, and promote the new state linked list to become
       the current state linked list. */
    __eskel_state_release(__eskel_state.curr_head, 1);
    __eskel_state.curr_head = __eskel_state.new_head;
    __eskel_state.curr_tail = __eskel_state.new_tail;
    __eskel_state.new_head = NULL;
    __eskel_state.new_tail = NULL;
    return 0;
}


/* Example usage of the above interfaces. */
/* int stage (void) { */
/*     int i = 0, j = 10; */
/*     eskel_state_register (&i, sizeof (int)); */
/*     eskel_state_register (&j, sizeof (int)); */
/*     while (i < 10) { */
/*         printf ("[%d] %d %d\n", rank, i++, j--); */
/*         if ((rank == 0) && (i > 4)) break; */
/*     } */
/*     if (rank == 0) __eskel_state.proc = 1; */
/*     else __eskel_state.proc = -1; */
/*     eskel_state_release (); */
/*     return 0; */
/* } */
                
/* int main (int argc, char *argv[]) { */
/*     MPI_Init (&argc, &argv); */
/*     MPI_Comm_rank (MPI_COMM_WORLD, &rank); */
/*     MPI_Comm_size (MPI_COMM_WORLD, &size); */
/*     if (rank == 0) __eskel_state.proc = -1; */
/*     else __eskel_state.proc = 0; */
/*     stage (); */
/*     MPI_Finalize(); */
/*     return 0; */
/* } */
