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

   These interfaces are invoked by the scheduler to determine best
   task-to-node mappings by using the Enhance technology (pattern
   directed PEPA based performance models. */

#include <eskel.h>

/* Initialise source-destination pattern matrix. */
static int __eskel_pepa_pmatrix[3][3] = {
    /*           Unknown | Pipe | Deal */
    /* Unknown |    0    |   1  |  2   */
    /*    Pipe |    3    |   4  |  5   */
    /*    Deal |    6    |   7  |  8   */
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8}
};

/* The greatest common divisor. */
static int gcd(int a, int b) {
    while (a != b) {
        if (a > b) a = a - b;
        else b = b - a;
    }
    return a;
}

/* The lowest common multiple. */
#define lcm(a,b) (((a)*(b))/gcd((a),(b)))

/* FIXME: This is not right. */
static char tput_network[512];
static char tput_proc[512];
static char tput_tasks[512];
static char processor[1024];
static char network[1024];
static char task[1024];
static char moves[1024];
static char model[1024];
static char temp[1024];
static char *use_flag;
static int current_map = 0;

extern __eskel_rmon_t *__eskel_rmon;
extern long __eskel_sched_count;
extern long __eskel_nmaps;
extern int *__eskel_maps;
extern int __eskel_summary_request;

/* Generate task definition for this leaf-node. */
static int __eskel_pepa_task_def(__eskel_node_t *node) {
    int i, j, k, l;
    if (__eskel_pepa_pmatrix[node->ptype][node->stype] == 0) {
        printf("Error\n");
        return -1;
    }
    if ((node->sol.n > 0) && (node->sil.n > 0))
        l = lcm(node->sol.n, node->sil.n);
    else
        l = node->sol.n + node->sil.n;
    sprintf(temp, "t_%d = \t", node->index);
    strcat(task, temp);

    switch(__eskel_pepa_pmatrix[node->ptype][node->stype]) {
    case 1:
    case 2:
        for (i = 0; i < l; i++) {
            k = node->sil.l[i % node->sil.n];
            if ((node->index == 0) && (k == 1)) {
                sprintf(temp, "(comp_%d, infty).t_01;\n"
                        "t_01 = \t(move_%d_%d, infty).%s",
                        node->index, node->index, k,
                        ((l > 1) && (i < l - 1)) ? "\n\t" : "");
            } else {
                sprintf(temp, "(comp_%d, infty).(move_%d_%d, infty).%s",
                        node->index, node->index, k,
                        ((l > 1) && (i < l - 1)) ? "\n\t" : "");
            }
            strcat(task, temp);
            __ESKEL_2DMBER(use_flag, node->index, k, __eskel_rt.nproc) = 'U';
            sprintf(temp, "move_%d_%d, ", node->index, k);
            strcat(moves, temp);
        }
        break;
    case 3:
    case 6:
        for (i = 0; i < l; i++) {
            j = node->sol.l[i % node->sol.n];
            sprintf(temp, "(move_%d_%d, infty).(comp_%d, infty).%s",
                    j, node->index, node->index,
                    ((l > 1) && (i < l - 1)) ? "\n\t" : "");
            strcat(task, temp);
        }
        break;        
    case 4:
    case 5:
    case 7:
    case 8:
        for (i = 0; i < l; i++) {
            k = node->sil.l[i % node->sil.n];
            j = node->sol.l[i % node->sol.n];
            sprintf(temp, "(move_%d_%d, infty).(comp_%d, infty)."
                    "(move_%d_%d, infty).%s",
                    j, node->index, node->index, node->index, k,
                    ((l > 1) && (i < l - 1)) ? "\n\t" : "");
            strcat(task, temp);
            __ESKEL_2DMBER(use_flag, node->index, k, __eskel_rt.nproc) = 'U';
            sprintf(temp, "move_%d_%d, ", node->index, k);
            strcat(moves, temp);
        }
        break;        
    }
    sprintf(temp, "t_%d;\n", node->index);
    strcat(task, temp);
    return 0;
}

/* Generate task definitions for all the leaf-nodes in subtree. */
static void __eskel_pepa_subtree_def(__eskel_node_t *n) {
    __eskel_child_t *c;
    short i;
    if (n) {
        if (n->mtype == TASK)
            __eskel_pepa_task_def(n);
        c = n->clist;
        for (i = 0; c && i < n->nchr; i++) {
            __eskel_pepa_subtree_def(c->child);
            c = c->next;
        }
    }
}

/* Generate tasks definitions throughout the hierarchy tree. */
static int __eskel_pepa_define_tasks(void) {
    if (!__eskel_rt.skel_tree || __eskel_rt.node_sum) {
        printf ("Invalid tree.\n");
        return -1;
    }
    strcpy(task, "");
    __eskel_pepa_subtree_def(__eskel_rt.skel_tree);
    return 0;
}

/* Generate the overall model from this subtree of the hierarchy
   tree. This is where we define the synchronisation sets for all the
   interacting tasks under the subtree. */
static int __eskel_pepa_subtree_model(__eskel_node_t *n) {
    __eskel_child_t *c;
    int i;
    if (n) {
        if (n->mtype == TASK) {
            if (n->p->mtype == PIPE) {
                if (n->ptype == DEAL) {
                    sprintf(temp, " <move_%d_%d", n->sol.l[0], n->index);
                    strcat(model, temp);                    
                    for (i = 1; i < n->sol.n; i++) {
                        int k = n->sol.l[i % n->sol.n];
                        sprintf(temp, ", move_%d_%d", k, n->index);
                        strcat(model, temp);
                    }
                    sprintf(temp, "> ");
                    strcat(model, temp);

                    /* Application model. */
                    sprintf(temp, " || ");
                    strcat(tput_tasks, temp);
                }
            }
            sprintf(temp, "t_%d", n->index);
            strcat(model, temp);

            /* Application model. */
            if (n->index == 0) sprintf(temp, "t_01");
            else if (n->index == 1) sprintf(temp, "t_%d", n->index);
            else sprintf(temp, " ** ");
            strcat(tput_tasks, temp);

            if ((n->p->mtype == PIPE) && (n->rank < n->p->nchr - 1)) {
                sprintf(temp, " <move_%d_%d", n->index, n->sil.l[0]);
                strcat(model, temp);
                for (i = 1; i < n->sil.n; i++) {
                    int k = n->sil.l[i % n->sil.n];
                    sprintf(temp, ", move_%d_%d", n->index, k);
                    strcat(model, temp);
                }
                sprintf(temp, "> ");
                strcat(model, temp);

                /* Application model. */
                sprintf(temp, " || ");
                strcat(tput_tasks, temp);
            } else if (n->p->mtype == DEAL) {
                if (n->rank < (n->p->nchr - 1)) {
                    sprintf(temp, " || ");
                    strcat(model, temp);

                    /* Application model. */
                    sprintf(temp, " || ");
                    strcat(tput_tasks, temp);
                }
            }
        } else {
            sprintf(temp, "(");
            strcat(model, temp);

            /* Application model. */
            sprintf(temp, "(");
            strcat(tput_tasks, temp);

            c = n->clist;
            for (i = 0; c && i < n->nchr; i++) {
                __eskel_pepa_subtree_model(c->child);
                c = c->next;
            }
            sprintf(temp, ")");
            strcat(model, temp);

            /* Application model. */
            sprintf(temp, ")");
            strcat(tput_tasks, temp);

            if (n->p && n->p->mtype == DEAL)
                if (n->rank < n->p->nchr - 1) {
                    sprintf(temp, " || ");
                    strcat(model, temp);

                    /* Application model. */
                    sprintf(temp, " || ");
                    strcat(tput_tasks, temp);
                }
        }
    }
    return 0;
}

/* Generate model for the overall hierarchy tree. */
static int __eskel_pepa_define_model(void) {
    int i;
    if (!__eskel_rt.skel_tree || __eskel_rt.node_sum) {
        printf ("Invalid tree.\n");
        return -1;
    }

    __eskel_pepa_subtree_model(__eskel_rt.skel_tree);

    /* Fill in the processor model. */
    sprintf(temp, "<comp_0");
    strcat(model, temp);    
    for (i = 1; i < __eskel_rt.nleaves; i++) {
        sprintf(temp, ", comp_%d", i);
        strcat(model, temp);
    }
    sprintf(temp, ">");
    strcat(model, temp);    

    sprintf(temp, "(Processor_0");
    strcat(model, temp);    

    /* Application model. */
    sprintf(temp, "(**");
    strcat(tput_proc, temp);    

    for (i = 1; i < __eskel_rt.nleaves; i++) {
        sprintf(temp, " || Processor_%d", i);
        strcat(model, temp);

        /* Application model. */
        sprintf(temp, " || **");
        strcat(tput_proc, temp);    
    }
    sprintf(temp, ")");
    strcat(model, temp);

    /* Application model. */
    sprintf(temp, ")");
    strcat(tput_proc, temp);    

    return 0;
}

/* This transforms the available CPU power, and the corresponding task
   rates into PEPA model rates, which will be used in defining the
   processor. Note that the conversion is for injunctive scheduling
   where only one task is assigned to a processor. */
static double convert_to_rate(double duration,
                              double avail,
                              double cpu_freq_node,
                              double cpu_freq_ref) {
    /*      printf("%f %f %f %f\n", duration, avail, cpu_freq_node, cpu_freq_ref); */
    return ((avail * cpu_freq_node)/(cpu_freq_ref * duration));
}

/* Define the processors. This is where the actual rate comes into
   picture. */
static int __eskel_pepa_define_processor(void) {
    int i, node;
    double rate;
    double avail;
    
    for (i = 0; i < __eskel_rt.nleaves; i++) {
        node = __ESKEL_2DMBER(__eskel_maps, current_map, i, __eskel_rt.nleaves) - 1;

        /* The above code segment is for an idealised case, which is weak.
           In the new version, the remote node sends the current real
           contribution of the current process, including the
           theoretical value which is the expected availability
           calculated based on the process priorities (read nice
           values). If the contribution is > 0.0, we use this value
           since this is what we will get in real. If the process is
           sleeping because it was not assigned any task, the
           contribution will be zero. In this case, we use the
           theoretical value (which gives what can I expect if I am
           assigned as task. */
        /*         if (__eskel_rmon[node].contrib > 0.0) */
        /*             avail = __eskel_rmon[node].contrib; */
        /*         else */
        avail = __eskel_rmon[node].theor;

        /* Convert the availability to the PEPA rate value. */
        rate = convert_to_rate(__eskel_rt.sstab[i]->duration,
                               avail, __eskel_rmon[node].cpufreq,
                               __eskel_rmon[0].cpufreq);
#ifdef __ESKEL_DEBUG_PEPA
        printf("[0]  pepa - (%d: %d->%d) [t: %f, c: %f, "
               "a: %f, r: %f]\n", current_map, i, node,
               __eskel_rmon[node].total, __eskel_rmon[node].contrib, avail, rate);
#endif
        sprintf(temp,
                "Processor_%d = (comp_%d, %f).Processor_%d;\n",
                i, i, rate,    i);
        strcat(processor, temp);
    }
}

/* Define the network. This is where the actual latency comes into
   picture. */
static int __eskel_pepa_define_network(void) {
    int i, j, x, y, t, u;
    j = __eskel_rt.nproc*__eskel_rt.nproc;
    for (i = 0; i < j; i++) {
        if (use_flag[i] == 'U') {
            x = i / __eskel_rt.nproc;
            y = i % __eskel_rt.nproc;
            t = __ESKEL_2DMBER(__eskel_maps, current_map, x, __eskel_rt.nleaves);
            u = __ESKEL_2DMBER(__eskel_maps, current_map, y, __eskel_rt.nleaves);
            sprintf(temp, "(move_%d_%d, %f).Network + ",
                    x, y, 1 / __ESKEL_2DMBER(latency, t, u, __eskel_rt.nproc));
            strcat(network, temp);

            if (y == 1) {
                sprintf(tput_network, "%f * {**",
                        1 / __ESKEL_2DMBER(latency, t, u, __eskel_rt.nproc));
            }
        }
    }

    j = strlen(network);
    network[j - 1] = '\0';
    network[j - 2] = '\n';
    network[j - 3] = ';';
    j = strlen(moves);
    moves[j - 2] = '\0';
    return 0;
}

/* Function for generating ans solving PEPA models for a Pipeline.
   This function also returns the best mapping index. */
#define xstr(s) str(s)
#define str(s) #s
static int gen_solve_pepa_models(void) {
    int j, status;
    long i;
    int best_map = 0;
    long double best_map_result = 0.0, temp = 0.0;
    FILE *file;
    char fname[] = "mapping.pepa";

    for (i = 0; i < __eskel_nmaps; i++) {
        current_map = i;
        for (j = 0; j < __eskel_rt.nproc*__eskel_rt.nproc; j++)
            use_flag[j] = 'N';        
        if (!(file = fopen(fname, "w"))) {
            perror("Creating model file");
            return -1;
        }
        strcpy(processor, "");
        strcpy(network, "Network = ");
        strcpy(model, "");
        strcpy(moves, "");
        strcpy(tput_tasks, "");
        strcpy(tput_proc, "");
        strcpy(tput_network, "");

        __eskel_pepa_define_processor();
        __eskel_pepa_define_tasks();
        __eskel_pepa_define_model();
        __eskel_pepa_define_network();

        fprintf(file,
                "// Processor definitions\n"
                "%s\n"
                "// Network definition\n"
                "%s\n"
                "// Task definitions\n"
                "%s\n"
                "// The application Model\n"
                "Network<%s>%s\n\n"
                "// Performance result: throughput\n"
                "T1 = %s || %s || %s};\n",
                processor, network, task, moves, model,
                tput_network, tput_tasks, tput_proc);

        fclose(file);

        switch (fork ()) {
        case -1:
            perror ("Forking PEPA Workbench");
            break;
        case 0:
            /* __ESKEL_INSTALL_PATH is set in the configure.in
               depending on the prefix specified by the user during
               configuration. */
            if (execlp("sh", "sh", "-c",
                       "java -jar "xstr(__ESKEL_INSTALL_PATH)
                       "/share/PEPAWorkbench.jar "
                       "-run lr ./mapping.pepa 1>/dev/null",
                       NULL) == -1)
                perror("Solving model");
        }
        (void ) wait (&status);
        
        /* Get the result. */
        file = fopen("mapping.res", "r");
        fscanf (file, "mapping:%Lf", &temp);
        fclose (file);
        if (temp > best_map_result) {
            best_map_result = temp;
            best_map = i;
        }
        //        remove ("mapping.pepa");
        remove ("mapping.res");
    }
    return best_map;
}
#undef str
#undef xstr

/* Initialises the PEPA interface. */
int __eskel_pepa_init(void) {
    FILE *f;
    int i, j;

    /* Allocate and initialise network latency matrix. */
    j = __eskel_rt.nproc*__eskel_rt.nproc;
    if (!(use_flag = (char *) calloc(j, sizeof(char)))) {
        return -1;
    }

    /* Allocate resource monitor information. */
    if (!(__eskel_rmon = (__eskel_rmon_t *)
          __eskel_malloc(sizeof(__eskel_rmon_t) * __eskel_rt.nproc))) {
        perror("Allocating resource monitor vector");
        __eskel_free(use_flag);
        return -1;
    }

    /* Get the possible mapping choices. */
    if (!(f = fopen("maps.dat", "r"))) {
        /* NOTE: We used to disable scheduling when the mapping was not
           specified. Now, we use cool-lex algorithm to generate all
           the combinations that may be used (when the number of
           combinations are small). Otherwise, we disable scheduling. */

        /* How many mappings will be generated? This equals:
           __eskel_nmaps = C(nproc,nleaves)
           nproc!
           = ----------------------------
           (nproc - nleaves)! nleaves!

           Don't forget to exclude the scheduler while generating the
           mappings.
        */
        __eskel_nmaps = __eskel_Cnr(__eskel_rt.nproc - 1,
                                    __eskel_rt.nleaves);

#ifdef __ESKEL_DEBUG_PEPA
        printf("[0]  pepa - Number of combinations: %ld\n", __eskel_nmaps);
#endif
        if (__eskel_nmaps < __ESKEL_MAX_MAPPINGS) {
            /* Allocate the maps table. */
            if (!(__eskel_maps = (int *)
                  calloc(__eskel_nmaps*__eskel_rt.nleaves, sizeof(int)))) {
                __eskel_free(use_flag);
                __eskel_free(__eskel_rmon);
                return -1;
            }
            /* Fill in the maps by using the cool-lex algorithm. Don't
               forget to exclude the scheduler while generating the
               mappings.*/
            __eskel_coollex(__eskel_maps,
                            __eskel_rt.nproc - __eskel_rt.nleaves - 1,
                            __eskel_rt.nleaves);

#ifdef __ESKEL_DEBUG_PEPA
            printf("[0] sched - Mappings generated with Cool-lex algorithm.\n");
            for (i = 0; i < __eskel_nmaps; i++) {
                for (j = 0; j < __eskel_rt.nleaves; j++) {
                    printf("%d ", __eskel_maps[i*__eskel_rt.nleaves + j]);
                }
                printf("\n");
            }                
#endif
            return 0;
        } else {
            printf("[0] sched - Warning: Specify possible "
                   "mappings in 'maps.dat'.\n");
            __eskel_free(use_flag);
            __eskel_free(__eskel_rmon);
            return -1;
        }
    }
    if (fscanf(f, "%ld", &__eskel_nmaps) == EOF) {
        perror("Reading number of possible mappings");
        __eskel_free(use_flag);
        __eskel_free(__eskel_rmon);
        fclose(f);
        return -1;
    }
    if (!(__eskel_maps = (int *)
          calloc(__eskel_nmaps*__eskel_rt.nleaves, sizeof (int)))) {
        __eskel_free(use_flag);
        __eskel_free(__eskel_rmon);
        fclose(f);
        return -1;
    }
    for (i = 0; i < __eskel_nmaps; i++) {
        for (j = 0; j < __eskel_rt.nleaves; j++) {
            if (fscanf(f, "%d",
                       &__eskel_maps[i*__eskel_rt.nleaves + j]) == EOF) {
                perror ("Reading mappings");
                __eskel_free(__eskel_maps);
                __eskel_free(use_flag);
                __eskel_free(__eskel_rmon);
                fclose (f);
                return -1;
            }
        }
    }
    fclose(f);
    return 0;
}

/* Finalises the PEPA interface. */
int __eskel_pepa_final(void) {
    __eskel_free(use_flag);
    __eskel_free(__eskel_rmon);
    __eskel_free(__eskel_maps);
    return 0;
}

/* This functions requests new resource values from the remote rmons
   running on the worker processes. */
static int update_resources(void) {
    int i, j = __eskel_rt.nproc - 1;
    __eskel_sched_count++;
    for (i = 0; i < j; i++)
        eskel_rmon_request(&__eskel_rmon[i], i);
    return 0;
}

/* This function returns the array of integers ranks which represents
   the best new schedule for the application. */
extern unsigned int __eskel_sched_period;
int __eskel_pepa_schedule(int *s) {
    int i, best_map, *temp;
    static int previous_map = 0;

    sleep(__eskel_sched_period);
    update_resources();
    best_map = gen_solve_pepa_models();

    /* Output the schedule and resource information. */
    if (__eskel_summary_request)
        __eskel_write_dyn(best_map);

    if (best_map == previous_map) {
#ifdef __ESKEL_DEBUG_PEPA
        printf("[0] sched - Same schedule.\n");
#endif
        return -1;
    } else
        previous_map = best_map;
    temp = &__eskel_maps[best_map * __eskel_rt.nleaves];
    for (i = 0; i < __eskel_rt.nleaves; i++) {
        s[i] = temp[i];
#ifdef __ESKEL_DEBUG_PEPA
        printf("%d", s[i]);
#endif
    }
#ifdef __ESKEL_DEBUG_PEPA
    printf("\n");
#endif
    return 0;
}
