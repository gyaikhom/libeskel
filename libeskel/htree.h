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
  Programmer interfaces for inserting nodes into the skeleton
  hierarchy tree.

  Written by: Gagarine Yaikhom (g.yaikhom@sms.ed.ac.uk)
  School of Informatics, University of Edinburgh
*/

#ifndef __ESKEL_COMPILE
#error "Please do not include 'htree.h'; include 'eskel.h' instead."
#endif

#ifndef __ESKEL_HTREE_H
#define __ESKEL_HTREE_H

#include <sys/types.h>
#include <unistd.h>

/* Type of skeleton hierarchy tree component. */
typedef enum {
    UNKNOWN = 0,  /* Interface with outside. */
    PIPE,         /* Pipeline (sequential). */
    DEAL,         /* Round-robin distribution. */
    TASK,         /* Task function (leaf-node) */
    NSKEL         /* Number of patterns supported. */
} __eskel_comp_t; /* Hierarchy tree component. */

typedef struct __eskel_node_s __eskel_node_t;
typedef struct __eskel_child_s {
    struct __eskel_child_s *prev; /* Pointer to the previous sibling. */
    struct __eskel_child_s *next; /* Pointer to the next sibling. */
    __eskel_node_t *child;        /* The child pointer list. */
} __eskel_child_t;
typedef struct __eskel_plist_s {
    int n;  /* Number of processes. */
    int *l; /* The process list. */
} __eskel_plist_t;

/*
  The child list is required for the current implementation because
  a node can have more than two children. In future versions, we can
  transform this tree into a Binary tree by imposing the restriction
  that a node can only have two children. This, I think, should not be
  a big deal because we can always group all the remaining siblings,
  except the first sibling into a single node.
*/
typedef int (*__eskel_fptr_t) (void);
struct __eskel_node_s {
    char name[256];            /* FIXME: Name of the task. */
    double duration;           /* FIXME: This should be outside. */
    __eskel_node_t *p;         /* Pointer to parent. */
    __eskel_child_t *clist;    /* Child list. */
    __eskel_comp_t mtype;      /* Type of tree node. */
    __eskel_comp_t ptype;      /* Predecessor type. */
    __eskel_comp_t stype;      /* Successor type. */
    int nchr;                  /* Number of child nodes required. */
    int nchx;                  /* Number of child nodes created. */
    int index;                 /* Node index in the tree. */
    int rank;                  /* My sibling rank. */
    __eskel_plist_t sol;       /* List of sources. */
    __eskel_plist_t sil;       /* List of sinks. */
    __eskel_plist_t temp;      /* Related to farm. */
    __eskel_fptr_t function;   /* Stage function. */
    MPI_Datatype input_dtype;  /* Input data type. */
    MPI_Datatype output_dtype; /* Output data type. */
    unsigned short input_du;   /* Number of input data. */
    unsigned short output_du;  /* Number of input data. */
};

struct __eskel_rt_s {
    __eskel_node_t *skel_tree; /* Main skeleton tree. */
    __eskel_node_t *curr_node; /* Current node. */
    __eskel_node_t **sstab;    /* Source-sink lookup table. */
    int nnodes;                /* Number of nodes in the tree. */
    int nleaves;               /* Number of leaf nodes. */
    int node_sum;              /* Used to validate tree structure. */
    int nproc;                 /* Number of processes available. */
    int rank;                  /* Rank of this process. */
    int *curr_task2node;       /* Current scheduler vector. */
    int *curr_node2task;       /* Current node-to-task mapping. */
    int *new_task2node;        /* New scheduler vector. */
    int *new_node2task;        /* New node-to-task mapping. */
    int *rmon_fd;              /* Resource monitor sockets. */
    char **hnames;             /* Hostnames of available processes. */
    pthread_t rmon_thread;     /* Resource monitor thread. */
    pid_t pid;                 /* Process id. */
};

extern struct __eskel_rt_s __eskel_rt;
extern int eskel_htree_display(unsigned short flags);
extern int eskel_htree_commit(void);
extern int __eskel_htree_display_sstab(void);
extern int __eskel_htree_insert_node(__eskel_comp_t skel, int nchild, ...);
extern int __eskel_htree_gen_sstab(__eskel_node_t *n, int source, int sink);
extern void __eskel_htree_destroy(__eskel_node_t *n);

#define eskel_pipe(X) __eskel_htree_insert_node(PIPE, (X));
#define eskel_xdeal(X) __eskel_htree_insert_node(DEAL, (X));

#define eskel_deal(X,Y,I,O)                                  \
    {                                                        \
        register int i;                                      \
        __eskel_htree_insert_node(DEAL, (X));                \
        for (i = 0; i < (X); i++)                            \
            __eskel_htree_insert_node(TASK, 0, #Y, Y, I, O); \
    }
#define eskel_task(X,I,O)                            \
    __eskel_htree_insert_node(TASK, 0, #X, X, I, O);

#endif /* __ESKEL_HTREE_H */
