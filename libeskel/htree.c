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
   Interfaces for maintaining the skeleton hierarchy tree. */

#include <eskel.h>

char skel_name[NSKEL][MAX_SKEL_NAME] = {
    "unknown", /* Unkown skeleton. */
    "pipe",    /* Pipeline skeleton. */
    "deal",    /* Deal skeleton. */
    "task"     /* Task node. */
};

static void __eskel_htree_inherit_source(__eskel_node_t *n);
static void __eskel_htree_update_source(__eskel_node_t *n);
static void __eskel_htree_generate_source(__eskel_node_t *n);
static void __eskel_htree_inherit_sink(__eskel_node_t *n);
static void __eskel_htree_update_sink(__eskel_node_t *n);
static void __eskel_htree_generate_sink(__eskel_node_t *n);
static int __eskel_htree_insert_sibling(__eskel_node_t *n);

/* Generate the sources for each of the leaf nodes (stages). */
void __eskel_htree_generate_source(__eskel_node_t *n) {
    __eskel_child_t *c;
    register unsigned int i;
    __eskel_htree_inherit_source (n);
    if (n->mtype == TASK) __eskel_rt.sstab[n->index] = n;
    if ((c = n->clist))
        for (i = 0; c && i < n->nchr; i++) {
            if (c->child)
                __eskel_htree_generate_source (c->child);
            c = c->next;
        }
    __eskel_htree_update_source (n);
}

/* Inherits source lists from the parent. */
void __eskel_htree_inherit_source(__eskel_node_t *n) {
    register unsigned int i;
    n->sol.n = n->p->sol.n;
    n->sol.l = (int *) __eskel_malloc(sizeof(int)*n->sol.n);
    n->ptype = n->p->ptype;
    for (i = 0; i < n->sol.n; i++)
        n->sol.l[i] = n->p->sol.l[i];
}

/* Update the existing source list based on skeleton type. */
void __eskel_htree_update_source(__eskel_node_t *n) {
    __eskel_child_t *c;
    register unsigned int i, j;
    switch (n->mtype) {
    case TASK:
        if (n->p->mtype != DEAL) {
            n->p->sol.n = 1;
            n->p->sol.l[0] = n->index;
            n->p->ptype = PIPE;
        } else {
            n->temp.n = 1;
            n->temp.l = (int *) __eskel_malloc (sizeof(int));
            n->temp.l[0] = n->index;
         } 
        break;
        
    case PIPE:
        if (n->p->mtype != DEAL) {
            __eskel_free (n->p->sol.l);
            n->p->sol.n = n->sol.n;
            n->p->sol.l = (int *) __eskel_malloc (sizeof(int)*(n->p->sol.n));
            for (i = 0; i < n->p->sol.n; i++)
                n->p->sol.l[i] = n->sol.l[i];
            n->p->ptype = n->ptype;
        } else {
            n->temp.n = n->sol.n;
            n->temp.l = (int *) __eskel_malloc (sizeof(int)*(n->temp.n));
            for (i = 0; i < n->temp.n; i++)
                n->temp.l[i] = n->sol.l[i];
         }
        break;

    case DEAL:
        if (n->p->mtype != DEAL) {
            __eskel_free (n->p->sol.l);
            n->p->sol.n = 0;
            n->p->sol.l = (int *) __eskel_malloc (sizeof(int)*MAX_NSOURCES);
            c = n->clist;
            for (i = 0; i < n->nchr; i++) {
                for (j = 0; j < c->child->temp.n; j++) {
                    n->p->sol.l[n->p->sol.n] = c->child->temp.l[j];
                    n->p->sol.n++;
                }
                c = c->next;
            }
            realloc (n->p->sol.l, sizeof(int)*n->p->sol.n);
            n->p->ptype = DEAL;
        } else {
            n->temp.n = 0;
            n->temp.l = (int *) __eskel_malloc (sizeof(int)*MAX_NSOURCES);
            c = n->clist;
            for (i = 0; i < n->nchr; i++) {
                for (j = 0; j < c->child->temp.n; j++) {
                    n->temp.l[n->temp.n] = c->child->temp.l[j];
                    n->temp.n++;
                }
                c = c->next;
            }
            realloc (n->temp.l, sizeof(int)*n->temp.n);
        }
        break;
    case UNKNOWN:
    case NSKEL:
        break;
    }
}

/* A possibly faster version depending on the source list. */
void gen_sink(int sink) {
    register unsigned int i, j, k;
    unsigned int l, temp = __eskel_rt.nleaves - 1;
    for (i = 0; i < temp; i++) {
        __eskel_rt.sstab[i]->sil.l = (int *) __eskel_malloc(sizeof(int)*temp);
        l = 0;
        for (j = 0; j < __eskel_rt.nleaves; j++) {
            if (i != j) {
                for (k = 0; k < __eskel_rt.sstab[j]->sol.n; k++) {
                    if (i >= __eskel_rt.sstab[j]->sol.l[k]) {
                        if (__eskel_rt.sstab[j]->sol.l[k] == i)
                            __eskel_rt.sstab[i]->sil.l[l++] = j;
                        goto next_source_list;
                    }
                }
            }
        next_source_list:;
        }
        realloc(__eskel_rt.sstab[i]->sil.l, sizeof(int)*l);
        __eskel_rt.sstab[i]->sil.n = l;        
    }
    __eskel_rt.sstab[temp]->sil.n = 1;
    __eskel_rt.sstab[temp]->sil.l = (int *) __eskel_malloc(sizeof(int));
    __eskel_rt.sstab[temp]->sil.l[0] = sink;
}

/* Generate the sink list for each of the leaf nodes (stages). */
void __eskel_htree_generate_sink(__eskel_node_t *n) {
    __eskel_child_t *c;
    register unsigned int i;
    __eskel_htree_inherit_sink(n);
    if (n->mtype == TASK) __eskel_rt.sstab[n->index] = n;
    if (n->clist) {
        c = n->clist->prev;
        for (i = 0; c && i < n->nchr; i++) {
            if (c->child) __eskel_htree_generate_sink(c->child);
            c = c->prev;
        }
    }
    __eskel_htree_update_sink(n);
}

/* Inherit sink list from parent. */
void __eskel_htree_inherit_sink (__eskel_node_t *n) {
    register unsigned int i;
    n->sil.n = n->p->sil.n;
    n->sil.l = (int *) __eskel_malloc (sizeof(int)*n->sil.n);
    n->stype = n->p->stype;
    for (i = 0; i < n->sil.n; i++)
        n->sil.l[i] = n->p->sil.l[i];
}

/* Update existing sink list. */
void __eskel_htree_update_sink (__eskel_node_t *n) {
    __eskel_child_t *c;
    register unsigned int i, j;
    switch (n->mtype) {
    case TASK:
        if (n->p->mtype != DEAL) {
            n->p->sil.n = 1;
            n->p->sil.l[0] = n->index;
            n->p->stype = PIPE;
        } else {
            n->temp.n = 1;
            n->temp.l = (int *) __eskel_malloc(sizeof(int));
            n->temp.l[0] = n->index;
         }
        break;
        
    case PIPE:
        if (n->p->mtype != DEAL) {
            __eskel_free (n->p->sil.l);
            n->p->sil.n = n->sil.n;
            n->p->sil.l = (int *) __eskel_malloc(sizeof(int)*(n->p->sil.n));
            for (i = 0; i < n->p->sil.n; i++) n->p->sil.l[i] = n->sil.l[i];
            n->p->stype = n->stype;
        } else {
            n->temp.n = n->sil.n;
            n->temp.l = (int *) __eskel_malloc(sizeof(int)*(n->temp.n));
            for (i = 0; i < n->temp.n; i++)    n->temp.l[i] = n->sil.l[i];
         }
        break;
        
    case DEAL:
        if (n->p->mtype != DEAL) {
            __eskel_free (n->p->sil.l);
            n->p->sil.n = 0;
            n->p->sil.l = (int *) __eskel_malloc(sizeof(int)*MAX_NDESTINATIONS);
            c = n->clist;
            for (i = 0; i < n->nchr; i++) {
                for (j = 0; j < c->child->temp.n; j++) {
                    n->p->sil.l[n->p->sil.n] = c->child->temp.l[j];
                    n->p->sil.n++;
                }
                c = c->next;
            }
            realloc(n->p->sil.l, sizeof(int)*n->p->sil.n);
            n->p->stype = DEAL;
        } else {
            n->temp.n = 0;
            n->temp.l = (int *) __eskel_malloc(sizeof(int)*MAX_NDESTINATIONS);
            c = n->clist;
            for (i = 0; i < n->nchr; i++) {
                for (j = 0; j < c->child->temp.n; j++) {
                    n->temp.l[n->temp.n] = c->child->temp.l[j];
                    n->temp.n++;
                }
                c = c->next;
            }
            realloc(n->temp.l, sizeof(int)*n->temp.n);
        }
        break;
    case UNKNOWN:
    case NSKEL:
        break;
    }
}

/* Generate source-sink table. */
int __eskel_htree_gen_sstab(__eskel_node_t *n, int source, int sink) {
    __eskel_child_t *c;
    register unsigned int i;
    if (!n || __eskel_rt.node_sum) return -1;
    if (!(__eskel_rt.sstab = (__eskel_node_t **)
          __eskel_malloc(sizeof (__eskel_node_t *)*__eskel_rt.nleaves)))
        return -1;
    if (!(n->sol.l = (int *) __eskel_malloc(sizeof(int)))) {
        __eskel_free(__eskel_rt.sstab);
        return -1;
    }
    if (!(n->sil.l = (int *) __eskel_malloc(sizeof(int)))) {
        __eskel_free(n->sol.l);
        __eskel_free(__eskel_rt.sstab);
        return -1;
    }
    
    /* Initialise the source and sink for the root node. */
    n->sol.n = 1;
    n->sol.l[0] = source;
    n->sil.n = 1;
    n->sil.l[0] = sink;
    n->ptype = UNKNOWN;
    n->stype = UNKNOWN;
    if (n->mtype == TASK) {
        __eskel_rt.sstab[n->index] = n;
        return 0;
    }
    
    /* Generate the source and sink for the remaining nodes. */
    if ((c = n->clist))
        for (i = 0; c && i < n->nchr; i++) {
            if (c->child)
                __eskel_htree_generate_source(c->child);
            c = c->next;
        }
/*     gen_sink (sink); */
    if (n->clist) {
        c = n->clist->prev;
        for (i = 0; c && i < n->nchr; i++) {
            if (c->child)
                __eskel_htree_generate_sink(c->child);
            c = c->prev;
        }
    }
    return 0;
}

/* Destroys the skeleton tree. */
void __eskel_htree_destroy(__eskel_node_t *n) {
    __eskel_child_t *c, *t;
    if (n) {
        if ((c = n->clist)) {
            c->prev->next = NULL; /* Break double linked list. */
            while (c) {
                t = c->next;
                __eskel_htree_destroy(c->child);
                __eskel_free(c);
                c = t;
            }
        }
        __eskel_free(n->temp.l);
        __eskel_free(n->sol.l);
        __eskel_free(n->sil.l);
        __eskel_free(n);
    }
}

/* Insert sibling into the double linked list. */
int __eskel_htree_insert_sibling(__eskel_node_t *n) {
    __eskel_child_t *m;

    /* Create a new sibling node for the double linke dist. */
    if (!(m = (__eskel_child_t *) __eskel_malloc(sizeof(__eskel_child_t))))
        return -1;
    /* Each sibling node can have a subtree of its own. */
    m->child = n;
    /* What is my sibling rank in the double linked list? The head
       (the first sibling inserted has rank zero. */
    n->rank = __eskel_rt.curr_node->nchx;
    if (__eskel_rt.curr_node->clist) {
        /* If there are siblings in the double linked list, append self
           at the end of the list. Remeber to update the following
           pointer.

           * The new sibling should point to the head and the last.
           * The previous last should point to the new as follower.
           * The head of the double linked list (the first sibling)
           should point to the new sibling as previous. */
        m->prev = __eskel_rt.curr_node->clist->prev;
        m->next = __eskel_rt.curr_node->clist;
        __eskel_rt.curr_node->clist->prev->next = m;
        __eskel_rt.curr_node->clist->prev = m;
    } else {
        /* If this is the first child, this child self
          references until more siblings are inserted. */
        m->prev = m;
        m->next = m;
        __eskel_rt.curr_node->clist = m;                
    }
    /* Acknowledge that a new sibling has entered. This is required to
       test if the parent node has created all the required children. */
    __eskel_rt.curr_node->nchx++;
    __eskel_rt.node_sum--;
    return 0;
}

/* Insert a subtree node to the parent node. If the skeleton type if
   TASK then we are required to have the function pointer to the
   stage function as the variable argument. */
int __eskel_htree_insert_node(__eskel_comp_t skel, int nchild, ...) {
    __eskel_node_t *n;
    va_list ap;
    if (!(n = (__eskel_node_t *) __eskel_malloc(sizeof(__eskel_node_t))))
        return -1;
    if ((n->mtype = skel) == TASK) {
        va_start(ap, nchild);
        strcpy(n->name, va_arg(ap, char *));
        n->function = va_arg(ap, __eskel_fptr_t);
        n->input_dtype = va_arg(ap, MPI_Datatype);
        n->output_dtype = va_arg(ap, MPI_Datatype);
        va_end(ap);
    } else n->function = NULL;
    n->clist = NULL;             /* No children yet. */
    if (__eskel_rt.skel_tree) {
        /* If there are existing nodes, we append the new node as a
           child node (sibling) which can bear a subtree of its
           own. If no nodes already exists, this node becomes the root
           node, and the parent of all.    */
        switch (skel) {
        case TASK:
            /* If the node is a stage (or function) node, there are
               couple of things we have to ensure for its proper
               execution. First, we have to ensure that the data paths
               are satisfied. In order to do this, we inherit the left
               and right pointers from the parent node.    */
            n->p = __eskel_rt.curr_node;

            /* We have to note that this node is not allowed to have
               any children. */
            n->nchr = n->nchx = 0;

            /* Finally, we have to insert this node as one of the child
               nodes for the parent node. Remember, we maintain the
               children list as a doubl-linked list for easy
               navigation. */
            __eskel_htree_insert_sibling(n);
            n->index = __eskel_rt.nleaves;
            __eskel_rt.nleaves++;
            break;

        case PIPE:
        case DEAL:
            n->p = __eskel_rt.curr_node;
            n->nchr = nchild;
            n->nchx = 0;
            __eskel_htree_insert_sibling(n);
            n->index = __eskel_rt.nnodes;            
            /* Now, we have to enter into this node to complete its
               subtree. Once all the required children are created,
               we return to parent.    */
            __eskel_rt.curr_node = n;
            break;
        default:
            printf("Node type not recognized.\n");
        }
    } else {
        /* This creates the first nodes in the skeleton tree. This
           node, which in effect is the 'root' of the tree, won't have
           a parent node. Because each node is considered as a
           'blackbox' data processing unit, both the left and right
           subtrees are initialised with 'NULL', which means that the
           data source and sink of this node is virtual memory. */
        n->p = NULL;

        /* The 'curr_node' pointer will be left unchanged until all
           of its child nodes have been inserted. With every child
           node that is inserted, the 'nchx' is incremented by one,
           and checked against 'nchr'. If they are the same, the
           'curr_node' pointer is moved to the parent node.    */
        n->nchr = nchild;
        n->nchx = 0;

        /* At the start, the current node and the main tree is
           initialised to this single node.    */
        __eskel_rt.skel_tree = __eskel_rt.curr_node = n;
    }
    __eskel_rt.nnodes++;

    /* For every node (starting from the root), all the required
       children should be created. This is necessary for the tree to be
       functional because the program cannot execute unless all the
       functional units (TASKS) are supplied. Because checking the
       validity of the tree might be expensive if we have to travel the
       tree node, we account for the creation of nodes and siblings
       through this variable. For a valid tree, 'node_sum' equals
       zero. */
    __eskel_rt.node_sum += nchild;

    /* Check if the required number of children have already been
       created. If this condition is satisfied, the subtree for the
       current node is complete. So, move to the next sibling. If there
       are no more siblings remaining, return to the parent. */
    while (__eskel_rt.curr_node &&
           (__eskel_rt.curr_node->nchr == __eskel_rt.curr_node->nchx)) {
        __eskel_rt.curr_node = __eskel_rt.curr_node->p;
    }
    return 0;
}

/* Displays the skeleton tree. */
void __eskel_htree_display(__eskel_node_t *n, short ind,
                           short l, unsigned short flags) {
    __eskel_child_t *c;
    register unsigned int i, j;
    if (n) {
        for (i = 0; i < l; i++) {
            for (j = 0; j < 4; j++)    printf(" ");
            printf("|");
        }
        for (i = 0; i < 2; i++)    printf ("_");
        printf ("%s", skel_name[n->mtype]);
        if (n->mtype == TASK) {
            if (flags & SHOW_INDEX)
                printf("[%d]", n->index);
            if (flags & SHOW_NAME)
                printf(" \"%s\"", n->name);
            if (flags & SHOW_PRED)
                printf(" %s", skel_name[n->ptype]);
            if (flags & SHOW_SUCC)
                printf(" %s", skel_name[n->stype]);
        }
        printf ("\n");
        c = n->clist;
        for (i = 0; c && i < n->nchr; i++) {
            __eskel_htree_display(c->child, ind + 4, l + 1, flags);
            c = c->next;
        }
    }
}

int eskel_htree_display(unsigned short flags) {
     if (__eskel_rt.rank == 0) {
        if (!__eskel_rt.skel_tree || __eskel_rt.node_sum) {
            printf("Invalid tree.\n");
            return -1;
        }
        __eskel_htree_display(__eskel_rt.skel_tree, 4, 0, flags);
    }
    return 0;
}

/* Display source-sink lookup table. */
int __eskel_htree_display_sstab(void) {
    register unsigned int i, j;
    printf("--------------------------\n");
    printf(" Stage | Source |  Sink \n");
    printf ("--------------------------\n");
    for (i = 0; i < __eskel_rt.nleaves; i++) {
        printf("%3d\t", i);
        printf("[%d", __eskel_rt.sstab[i]->sol.l[0]);
        for (j = 1; j < __eskel_rt.sstab[i]->sol.n; j++)
            printf(" %d", __eskel_rt.sstab[i]->sol.l[j]);
        printf("]\t[%d", __eskel_rt.sstab[i]->sil.l[0]);
        for (j = 1; j < __eskel_rt.sstab[i]->sil.n; j++)
            printf(" %d", __eskel_rt.sstab[i]->sil.l[j]);
        printf("]\n");
    }
    printf("--------------------------\n");
    return 0;
}
