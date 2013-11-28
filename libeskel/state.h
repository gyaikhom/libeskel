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

#ifndef __ESKEL_COMPILE
#error "Please do not include 'state.h'; include 'eskel.h' instead."
#endif

#ifndef __ESKEL_STATE_H
#define __ESKEL_STATE_h

#include <mpi.h>
#include <stdio.h>

/* There are cases where memory allocated for a state task should be
   deallocated before rescheduling. In such cases, the runtime system
   should know if a pointer to a state should be deallocated. If this
   is not done, there may be situations where memory is allocated with
   every rescheduling without deallocation. Since the programmer
   cannot determine when the rescheduling will happen, deallocation
   should be left to the runtime system. This means that the runtime
   system should be aware of the pointer to states that are not simply
   local variables, but programmer allocated memory chunks.

   1. Persistent memory are local variables that are defined as static
   within the task function. They are allocated by the compiler; and
   therefore needed not be deallocated by the programmer. Hence, the
   runtime system need not intervene.

   2. Transient memory is a task specific programmer allocated memory
   chunk that should be deallocated before exiting the task function.
*/
typedef enum {
    ESKEL_PERSISTENT = 0,
    ESKEL_TRANSIENT
} eskel_state_type_t;

typedef struct __eskel_state_elem_s __eskel_state_elem_t;
struct __eskel_state_elem_s {
    void *state_ptr;             /* Pointer to state variable. */
    size_t state_size;           /* Size of the state variable data. */
    eskel_state_type_t mem_type; /* What type is the memory? */
    __eskel_state_elem_t *next;  /* Pointer to next state variable. */
};

/* The old state linked list contains the state of the process before the
   rescheduling. If the process was not scheduled with any task, this
   linked list is empty. The new state linked list contains the state
   of the new task it is scheduled after the rescheduled. If the
   process was not scheduled for any task (which means that it is
   going to enter the wait state), this linked list is empty. A linked
   list is created when eskel_state_register() is invoked. */
struct __eskel_state_s {
    __eskel_state_elem_t *curr_head; /* Head of the old state linked list. */
    __eskel_state_elem_t *curr_tail; /* Tail of the old state linked list. */
    __eskel_state_elem_t *new_head;  /* Head of the new state linked list. */
    __eskel_state_elem_t *new_tail;  /* Tail of the new state linked list. */
    int replacing;                   /* The process I am replacing. */
    int replacer;                    /* The process which is replacing me. */
};

extern struct __eskel_state_s __eskel_state;
extern int __eskel_state_send(void);
extern int __eskel_state_release(__eskel_state_elem_t *head, int flag);
extern int eskel_state_register(void *ptr, size_t size, eskel_state_type_t flag);
extern int eskel_state_create(void **ptr, size_t size);
extern int eskel_begin(void);
#define eskel_end() {                                      \
        __eskel_state_release(__eskel_state.curr_head, 0); \
        return ESKEL_COMPLETED;                            \
    }

#endif /* __ESKEL_STATE_H */
