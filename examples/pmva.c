/*********************************************************************

  THE ENHANCE PROJECT
  School of Informatics,
  University of Edinburgh,
  Edinburgh - EH9 3JZ
  United Kingdom

  Written by: Gagarine Yaikhom

*********************************************************************/

#include <assert.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <eskel.h>

#define MAX_LINE_LEN 1024
#define MAX_NAME 64 
#define TAG_QUEUE_LEN 100

static int nclass = 0;           /* Number of classes. */
static int nqueue = 0;           /* Number of service stations. */
static char **c_names = NULL;    /* Names of the classes. */
static char **q_names = NULL;    /* Names of the service stations */
static int *load_vect = NULL;    /* Load intensity vector. */
static int *rtype = NULL;        /* Station type: TRUE -> queue, FALSE -> delay. */
static double *visit_vect = NULL;     /* Visitation vector. */
static double *service_time = NULL;   /* Service time. */
static double *service_demand = NULL; /* Service demand. */
static MPI_Datatype qlen_dtype; /* data type for interprocess communications. */
static int eff_nproc = 0;        /* Effective processors (without scheduler). */

/* Used during state registration. */
static ssize_t sztput;
static ssize_t szutil;
static ssize_t szqlen;
static ssize_t szresp;
static ssize_t szresp_c;
static ssize_t sznidx;
static ssize_t sztqlen;

static int parse_input(char*);
static int get_runs(int start, int end);
static int set_range(int coord, int *start, int *end);
static int update_nidx(int *nidx, int start, int end);
static unsigned long translate_index(int *nidx, int start, int end);
int save_results(char *fname, double *util, double *qlen,
                 double *resp, double *resp_c, double *tput);
static void __eskel_free_input();
static void init();

/* This is the first stage of the pipeline. This generates the results
 * starting from the base queue length when all the class populations
 * are zero. */
int first_stage (void) { 
    unsigned long cidx = 0, gidx = 0;
    int i, c, k, reset_clock = 1;
    double start_time = 0.0;

    /* The following constitutes the state of the execution. */
    static double *tput;   /* Class throughput. */
    static double *util;   /* Station utilization. */
    static double *qlen;   /* Queue length. */
    static double *resp;   /* Station response time. */
    static double *resp_c; /* Class response time. */
    static int *nidx;      /* Index vector for pointing the computing node. */
    static double *tqlen;  /* Intermediate queue length. */
    static int coord;      /* Cartesian coordinate of the processor. */
    static int start;      /* Start point of class 0. */
    static int end;        /* End point of class 0. */
    static double accum_duration = 0.0;
    static int niter = 0, ngives = 0, ntakes = 0;

    /* These values will be overwritten if this entry to task is after a
       rescheduling. */
    coord = eskel_task_idx;
    set_range(coord, &start, &end);

    /* Initialise memory chunk sizes for allocating the task workspace. */
    i = nclass*nqueue;
    sznidx = sizeof(int) * nclass;
    sztqlen = sizeof(double) * get_runs(start, end) * nqueue;
    sztput = sizeof(double) * nclass;
    szutil = sizeof(double) * i;
    szqlen = szutil;
    szresp = szutil;
    szresp_c = sizeof(double) * nclass;

    /* Initialize the index vector using the <start> index. This is
       used to keep track of the processing cycle, which decides which
       computation a processor should perform next. */
    nidx = (int *) __eskel_malloc(sznidx);
    for (c = 0; c < nclass; c++) nidx[c] = 0;
    nidx[0] = start;

    tqlen = (double*) __eskel_malloc(sztqlen); /* Temporary queue length vector */
    tput = (double*) __eskel_malloc(sztput); /* Throughput. */
    util = (double*) __eskel_malloc(szutil); /* Utilisation. */
    qlen = (double*) __eskel_malloc(szqlen); /* Average queue length. */
    resp = (double*) __eskel_malloc(szresp); /* Average response time.*/
    resp_c = (double*) __eskel_malloc(szresp_c); /* Response time per class. */

    eskel_state_register(tput, sztput, ESKEL_TRANSIENT);
    eskel_state_register(util, szutil, ESKEL_TRANSIENT);
    eskel_state_register(qlen, szqlen, ESKEL_TRANSIENT);
    eskel_state_register(resp, szresp, ESKEL_TRANSIENT);
    eskel_state_register(resp_c, szresp_c, ESKEL_TRANSIENT);
    eskel_state_register(nidx, sznidx, ESKEL_TRANSIENT);
    eskel_state_register(tqlen, sztqlen, ESKEL_TRANSIENT);
    eskel_state_register(&coord, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&start, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&end, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&accum_duration, sizeof(double), ESKEL_PERSISTENT);
    eskel_state_register(&niter, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&ngives, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&ntakes, sizeof(int), ESKEL_PERSISTENT);

    eskel_begin();
    do {
        /* Used for calculating reference time for this stage. */
        if (reset_clock) {
            start_time = MPI_Wtime();
            reset_clock = 0;
        }
        cidx = translate_index(nidx, start, end);
        for (c = 0; c < nclass; c++)
            if (nidx[c] != 0) {
                nidx[c]--;
                gidx = translate_index(nidx, start, end);
                nidx[c]++;
                resp_c[c] = 0.0;
                for (k = 0; k < nqueue; k++) {
                    resp[c + k*nclass] = 
                        service_demand[c + k*nclass]*
                        (1. + rtype[k]*tqlen[k + gidx*nqueue]);
                    resp_c[c] += resp[c + k*nclass];
                }
                tput[c] = ((double)nidx[c])/resp_c[c];
            } else {
                resp_c[c] = 0.0;
                tput[c] = 0.0;
                for (k = 0; k < nqueue; k++) {
                    resp[c + k*nclass] = 0.0;
                }
            }
        for (k = 0; k < nqueue; k++) {
            tqlen[k + cidx * nqueue] = 0.0;
            for (c = 0; c < nclass; c++) {
                tqlen[k + cidx*nqueue] += 
                    tput[c]*resp[c + k*nclass];
            }
        }

        /* Send intermediate queue length to the following stage. */
        if (nidx[0] == end) {
            eskel_give(tqlen + nqueue*cidx, 1);

            /* Record performance related statistical data. */
            accum_duration += MPI_Wtime() - start_time;
            ngives++;
            niter++;
            reset_clock = 1;
        }
    } while (update_nidx(nidx, start, end));
    printf("[%d] Gives: %d, Rate: %f seconds\n",
           eskel_rank, ngives, accum_duration / niter);

    __eskel_free(nidx);
    __eskel_free(tqlen);
    __eskel_free(tput);
    __eskel_free(util);
    __eskel_free(qlen);
    __eskel_free(resp);
    __eskel_free(resp_c);

    eskel_end();
}

/* This is the last stage of the pipeline. This produces the results
 * when the population on all the classes have reached their final
 * values. */
int last_stage (void) { 
    unsigned long cidx = 0, gidx = 0;
    int i, c, k, reset_clock = 1;
    double start_time = 0.0;

    /* The following constitutes the state of the execution. */
    static double *tput;   /* Class throughput. */
    static double *util;   /* Station utilization. */
    static double *qlen;   /* Queue length. */
    static double *resp;   /* Station response time. */
    static double *resp_c; /* Class response time. */
    static int *nidx;      /* Index vector for pointing the computing node. */
    static double *tqlen;  /* Intermediate queue length. */
    static int coord;      /* Cartesian coordinate of the processor. */
    static int start;      /* Start point of class 0. */
    static int end;        /* End point of class 0. */
    static double accum_duration = 0.0;
    static int niter = 0, ngives = 0, ntakes = 0;

    /* These values will be overwritten if this entry to task is after a
       rescheduling. */
    coord = eskel_task_idx;
    set_range(coord, &start, &end);

    /* Initialise memory chunk sizes for allocating the task workspace. */
    i = nclass*nqueue;
    sznidx = sizeof(int) * nclass;
    sztqlen = sizeof(double) * get_runs(start, end) * nqueue;
    sztput = sizeof(double) * nclass;
    szutil = sizeof(double) * i;
    szqlen = szutil;
    szresp = szutil;
    szresp_c = sizeof(double) * nclass;

    /* Initialize the index vector using the <start> index. This is
       used to keep track of the processing cycle, which decides which
       computation a processor should perform next. */
    nidx = (int *) __eskel_malloc(sznidx);
    for (c = 0; c < nclass; c++) nidx[c] = 0;
    nidx[0] = start;

    tqlen = (double*) __eskel_malloc(sztqlen); /* Temporary queue length vector */
    tput = (double*) __eskel_malloc(sztput); /* Throughput. */
    util = (double*) __eskel_malloc(szutil); /* Utilisation. */
    qlen = (double*) __eskel_malloc(szqlen); /* Average queue length. */
    resp = (double*) __eskel_malloc(szresp); /* Average response time.*/
    resp_c = (double*) __eskel_malloc(szresp_c); /* Response time per class. */

    eskel_state_register(tput, sztput, ESKEL_TRANSIENT);
    eskel_state_register(util, szutil, ESKEL_TRANSIENT);
    eskel_state_register(qlen, szqlen, ESKEL_TRANSIENT);
    eskel_state_register(resp, szresp, ESKEL_TRANSIENT);
    eskel_state_register(resp_c, szresp_c, ESKEL_TRANSIENT);
    eskel_state_register(nidx, sznidx, ESKEL_TRANSIENT);
    eskel_state_register(tqlen, sztqlen, ESKEL_TRANSIENT);
    eskel_state_register(&coord, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&start, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&end, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&accum_duration, sizeof(double), ESKEL_PERSISTENT);
    eskel_state_register(&niter, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&ngives, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&ntakes, sizeof(int), ESKEL_PERSISTENT);

    eskel_begin();
    do {
        cidx = translate_index(nidx, start, end);
        
        /* Receive intermediate queue length from the previous stage. */
        if (nidx[0] == start) {
            eskel_take (tqlen + nqueue*(cidx - 1), 1);

            /* Record performance related statistical data. */
            ntakes++;
            if (!reset_clock) {
                accum_duration += MPI_Wtime() - start_time;
                niter++;
                reset_clock = 1;
            }
        }

        if (reset_clock) {
            start_time = MPI_Wtime();
            reset_clock = 0;
        }

        for (c = 0; c < nclass; c++)
            if (nidx[c] != 0) {
                nidx[c]--;
                gidx = translate_index(nidx, start, end);
                nidx[c]++;
                resp_c[c] = 0.0;
                for (k = 0; k < nqueue; k++) {
                    resp[c + k*nclass] = 
                        service_demand[c + k*nclass]*
                        (1. + rtype[k]*tqlen[k + gidx*nqueue]);
                    resp_c[c] += resp[c + k*nclass];
                }
                tput[c] = ((double)nidx[c])/resp_c[c];
            } else {
                resp_c[c] = 0.0;
                tput[c] = 0.0;
                for (k = 0; k < nqueue; k++) {
                    resp[c + k*nclass] = 0.0;
                }
            }
        for (k = 0; k < nqueue; k++) {
            tqlen[k + cidx * nqueue] = 0.0;
            for (c = 0; c < nclass; c++) {
                tqlen[k + cidx*nqueue] += 
                    tput[c]*resp[c + k*nclass];
            }
        }
    } while (update_nidx(nidx, start, end));
    printf("[%d] Takes: %d, Rate: %f seconds\n",
           eskel_rank, ntakes, accum_duration / niter);

    /* Generate the  */
    for (k = 0; k < nqueue; k++)
        for (c = 0; c < nclass; c++) {
            qlen[c + k*nclass] =
                tput[c]*resp[c + k*nclass];
            util[c + k*nclass] =
                tput[c]*service_demand[c + k*nclass];
        }
    save_results("output.dat", util, qlen, resp, resp_c, tput);

    __eskel_free(nidx);
    __eskel_free(tqlen);
    __eskel_free(tput);
    __eskel_free(util);
    __eskel_free(qlen);
    __eskel_free(resp);
    __eskel_free(resp_c);

    eskel_end();
}

/* This is the intermediate stage of the pipeline. This uses the
 * results generated by the previous stage, to produce results for the
 * next stage. */
int inter_stage (void) { 
    unsigned long cidx = 0, gidx = 0;
    int i, c, k;
    double start_time = 0.0;

    /* The following constitutes the state of the execution. */
    static double *tput;    /* Class throughput. */
    static double *util;    /* Station utilization. */
    static double *qlen;    /* Queue length. */
    static double *resp;    /* Station response time. */
    static double *resp_c;    /* Class response time. */
    static int *nidx;        /* Index vector for pointing the computing node. */
    static double *tqlen;    /* Intermediate queue length. */
    static int coord = -1;    /* Cartesian coordinate of the processor. */
    static int start = -1;    /* Start point of class 0. */
    static int end = -1;    /* End point of class 0. */
    static double accum_duration = 0.0;
    static int niter = 0, ngives = 0, ntakes = 0;

    /* These values will be overwritten if this entry to task is after a
       rescheduling. */
    coord = eskel_task_idx;
    set_range(coord, &start, &end);

    /* Initialise memory chunk sizes for allocating the task workspace. */
    i = nclass*nqueue;
    sznidx = sizeof(int) * nclass;
    sztqlen = sizeof(double) * get_runs(start, end) * nqueue;
    sztput = sizeof(double) * nclass;
    szutil = sizeof(double) * i;
    szqlen = szutil;
    szresp = szutil;
    szresp_c = sizeof(double) * nclass;

    /* Initialize the index vector using the <start> index. This is
       used to keep track of the processing cycle, which decides which
       computation a processor should perform next. */
    nidx = (int *) __eskel_malloc(sznidx);
    for (c = 0; c < nclass; c++) nidx[c] = 0;
    nidx[0] = start;

    tqlen = (double*) __eskel_malloc(sztqlen); /* Temporary queue length vector */
    tput = (double*) __eskel_malloc(sztput); /* Throughput. */
    util = (double*) __eskel_malloc(szutil); /* Utilisation. */
    qlen = (double*) __eskel_malloc(szqlen); /* Average queue length. */
    resp = (double*) __eskel_malloc(szresp); /* Average response time.*/
    resp_c = (double*) __eskel_malloc(szresp_c); /* Response time per class. */

    eskel_state_register(tput, sztput, ESKEL_TRANSIENT);
    eskel_state_register(util, szutil, ESKEL_TRANSIENT);
    eskel_state_register(qlen, szqlen, ESKEL_TRANSIENT);
    eskel_state_register(resp, szresp, ESKEL_TRANSIENT);
    eskel_state_register(resp_c, szresp_c, ESKEL_TRANSIENT);
    eskel_state_register(nidx, sznidx, ESKEL_TRANSIENT);
    eskel_state_register(tqlen, sztqlen, ESKEL_TRANSIENT);
    eskel_state_register(&coord, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&start, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&end, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&accum_duration, sizeof(double), ESKEL_PERSISTENT);
    eskel_state_register(&niter, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&ngives, sizeof(int), ESKEL_PERSISTENT);
    eskel_state_register(&ntakes, sizeof(int), ESKEL_PERSISTENT);

    eskel_begin();
    do {
        cidx = translate_index(nidx, start, end);
        
        /* Receive intermediate queue length from the previous stage. */
        if (nidx[0] == start) {
            eskel_take(tqlen + nqueue*(cidx - 1), 1);

            /* Record performance related statistical data. */
            start_time = MPI_Wtime();
            ntakes++;
        }

        for (c = 0; c < nclass; c++)
            if (nidx[c] != 0) {
                nidx[c]--;
                gidx = translate_index(nidx, start, end);
                nidx[c]++;
                resp_c[c] = 0.0;
                for (k = 0; k < nqueue; k++) {
                    resp[c + k*nclass] = 
                        service_demand[c + k*nclass]*
                        (1. + rtype[k]*tqlen[k + gidx*nqueue]);
                    resp_c[c] += resp[c + k*nclass];
                }
                tput[c] = ((double)nidx[c])/resp_c[c];
            } else {
                resp_c[c] = 0.0;
                tput[c] = 0.0;
                for (k = 0; k < nqueue; k++) {
                    resp[c + k*nclass] = 0.0;
                }
            }
        for (k = 0; k < nqueue; k++) {
            tqlen[k + cidx * nqueue] = 0.0;
            for (c = 0; c < nclass; c++) {
                tqlen[k + cidx*nqueue] += 
                    tput[c]*resp[c + k*nclass];
            }
        }

        /* Send intermediate queue length to the following stage. */
        if (nidx[0] == end) {
            eskel_give(tqlen + nqueue*cidx, 1);

            /* Record performance related statistical data. */
            accum_duration += MPI_Wtime() - start_time;
            niter++;
            ngives++;
        }
    } while (update_nidx(nidx, start, end));
    printf("[%d] Takes: %d, Gives: %d, Rate: %f seconds\n",
           eskel_rank, ntakes, ngives, accum_duration / niter);

    __eskel_free(nidx);
    __eskel_free(tqlen);
    __eskel_free(tput);
    __eskel_free(util);
    __eskel_free(qlen);
    __eskel_free(resp);
    __eskel_free(resp_c);

    eskel_end();
}

int main(int argc, char *argv[]) {
    double run_time;

    nice(10);

    eskel_init (&argc, &argv);    /* Initialise skeleton library. */

    /* Define custom MPI data type for sending intermediate queue length. */
    MPI_Type_contiguous(nqueue, MPI_DOUBLE, &qlen_dtype);
    MPI_Type_commit(&qlen_dtype);

    /* Define the skeleton hierarchy. */
    eskel_pipe(3);
    eskel_task(first_stage, qlen_dtype, qlen_dtype);
    eskel_task(inter_stage, qlen_dtype, qlen_dtype);
    eskel_task(last_stage, qlen_dtype, qlen_dtype);

    eskel_htree_commit(); /* Commit skeleton hierarchy. */
    eskel_htree_display(SHOW_INDEX); /* Display skeleton hierarchy. */

    /* Generate the workspace by allocating and initialising relevant
       data structures. Since process with rank 0 is used by the
       scheduler, it should not perform these. */
    if (eskel_rank != 0) init(argv[1]); /* Initialise workspace. */

    /* Attach scheduler. */
    eskel_set_scheduler(NULL, NULL, NULL);

    /* eskel_set_scheduler(__eskel_pepa_schedule, */
    /*                     __eskel_pepa_init, */
    /*                     __eskel_pepa_final); */

    /* Begin execution and record the time when computation began and
       concluded. */ 
    run_time = MPI_Wtime();
    eskel_exec(1);
    run_time = MPI_Wtime() - run_time;
    printf("[%d] Execution time: %f seconds %s\n", eskel_rank, run_time,
           (eskel_rank == 0) ? "(scheduler)" : "");

    /* Release resources allocated for the workspace.  */
    if (eskel_rank != 0) __eskel_free_input();

    eskel_final(); /* Finalise skeleton library. */
    return 0;
}

/* Compute the range: start and end of class 0. */
int set_range(int coord, int *start, int *end) {
    int range;

    range = (load_vect[0] + 1) / eff_nproc;
    if (range == 0) return 0;
    *start = range*coord;
    if (coord < (load_vect[0] + 1) % eff_nproc) {
        range++;
        *start += coord;
    } else
        *start += (load_vect[0] + 1) % eff_nproc;
    *end = *start + range - 1;
    return 1;
}


/* Increments the nidx vector. */
int update_nidx(int *nidx, int start, int end) {
    int i;

    if (nidx[0] == end)    nidx[0] = start;
    else {
        nidx[0]++;
        return 1;
    }
    for (i = 1 ; i < nclass; i++)
        if (nidx[i] == load_vect[i])
            nidx[i] = 0;
        else {
            nidx[i]++;
            return 1;
        }
    return 0;
}

unsigned long translate_index(int *nidx, int start, int end) {
    unsigned long  val = 0;
    int i;
    for (i = nclass - 2; i > 0; i--)
        val = val * (load_vect[i] + 1) + nidx[i];
    val = val * (2 + end - start) + nidx[0] - start + 1;
    return val;
}

int get_runs(int start, int end) {
    int val = 1;
    int i;
    val = 2 + end - start;
    for (i = 1; i < nclass - 1; i++)
        val = val * (load_vect[i] + 1);
    return val;
}

/* Initialise the workspace. */
void init(char *ifname) {
    parse_input(ifname); /* Parse input file. */
    assert(nclass > 0 && load_vect != NULL);

    /* When using the skeleton library, the first process is always
       assigned to execute the scheduler module. Hence, we have to
       initialise the number of effective processes available for the
       computation by using the number of leaf-nodes in the skeleton
       hierarchy. Since the computation uses the rank of a process in
       the effective process set, we have to fix the process rank to
       adjust for the rank 0 which is assigned to the scheduler
       process. */ 
    eff_nproc = eskel_ntask;
    if (eff_nproc > load_vect[0]) {
        printf("Too many processors.\n");
        exit(-1);
    }
}

/* Release the resources allocated for the queuing model. */
void __eskel_free_input() {
    int i;
    for (i = 0; i < nclass; i++) __eskel_free(c_names[i]);
    for (i = 0; i < nqueue; i++) __eskel_free(q_names[i]);
    __eskel_free(c_names);
    __eskel_free(q_names);
    __eskel_free(load_vect);
    __eskel_free(rtype);
    __eskel_free(visit_vect);
    __eskel_free(service_time);
    __eskel_free(service_demand);
}

int parse_input(char *ifname) {
    FILE *f;
    int i, j, r;
    char type[MAX_NAME];

    if (!(f = fopen(ifname, "r"))) return -1;

    fscanf(f, "%d", &nclass);
    /* printf("nclass: %d\n", nclass); */
    if (nclass < 1) {
        printf("Error: Number of classes should be greater than zero.\n");
        return -1;
    }

    fscanf(f, "%d", &nqueue);
    /* printf("nresources: %d\n", nqueue); */
    if (nqueue < 1) {
        printf("Error: Number of queues should be greater than zero.\n");
        return -1;
    }

    j = nclass*nqueue;
    c_names = (char **) __eskel_malloc(sizeof(char*) * nclass);
    q_names = (char **) __eskel_malloc(sizeof(char*) * nqueue);
    load_vect = (int *) __eskel_malloc(sizeof(int) * nclass);
    rtype = (int *) __eskel_malloc(sizeof(int) * nqueue);
    visit_vect = (double *) __eskel_malloc(sizeof(double) * j);
    service_time = (double *) __eskel_malloc(sizeof(double) * j);
    service_demand = (double *) __eskel_malloc(sizeof(double) * j);
    for (i = 0; i < nclass; i++) c_names[i] = NULL;
    for (i = 0; i < nqueue; i++) q_names[i] = NULL;

    for(i = 0; i < nclass; i++) {
        c_names[i] = (char *) __eskel_malloc(sizeof(char*) * MAX_NAME);
        r = fscanf(f, "%s %d", c_names[i], &load_vect[i]);
        /* printf("%s %d\n", c_names[i], load_vect[i]); */
        if (r == EOF || r < 2) {
            printf("Error: Class definitions should have:\n\t"
                   "<class name> followed by its <population>\n");
            __eskel_free_input();
            return -1;
        }
    }

    for(i = 0; i < nqueue; i++) {
        q_names[i] = (char*)__eskel_malloc(sizeof(char*)*MAX_NAME);
        r = fscanf(f, "%s %s", q_names[i], type);
        /* printf("%s %s\n", q_names[i], rtype); */
        if (r == EOF || r < 2) {
            printf("Error: Resource definitions should have:\n\t"
                   "<resource name> followed by its <type>\n");
            __eskel_free_input();
            return -1;
        }
        if (type[0] == 'Q' || type[0] == 'q')
            rtype[i] = 1;
        else
            if (type[0] == 'D' || type[0] == 'd') 
                rtype[i] = 0;
            else {
                printf("Error: Resource types should be either:\n\t"
                       "Q or q for queue, or D or d for delay.\n");
                __eskel_free_input();
                return -1;
            }
    }

    for (i = 0; i < j; i++) {
        r = fscanf(f, "%lg", &visit_vect[i]);
        /* printf("%lg\n", visit_vect[i]); */
        if (r == EOF || r < 1) {
            printf("Error: Not enough elements in visitation vector.\n");
            __eskel_free_input();
            return -1;
        }
    }

    for (i = 0; i < j; i++) {
        r = fscanf(f, "%lg", &service_time[i]);
        /* printf("%lg\n", service_time[i]); */
        if (r == EOF || r < 1) {
            printf("Error: Not enough service times.\n");
            __eskel_free_input();
            return -1;
        }
        /* Initialise service demand. */
        service_demand[i] = service_time[i]*visit_vect[i];
    }
    fclose(f);
    return 0;
}

/* Save the results of the queuing model */
int save_results(char *fname, double *util, double *qlen,
                 double *resp, double *resp_c, double *tput) {
    int c, k;
    FILE *f;
    double *util_final, *qlen_final;
    util_final = (double*) __eskel_malloc(sizeof(double) * nqueue);
    qlen_final = (double*) __eskel_malloc(sizeof(double) * nqueue);

    for (k = 0; k < nqueue; k++){
        util_final[k] = 0.0;
        qlen_final[k] = 0.0;
        for (c = 0; c < nclass; c++) {
            util_final[k] += util[c + k*nclass];
            qlen_final[k] += qlen[c + k*nclass];
        }
    }
    f = fopen(fname, "w");
    fprintf(f,"GLOBAL STATISTICS:\n\n");
    for (c = 0; c < nclass; c++)
        fprintf(f, "class %s:\n\tmean response time = %g\n\tthroughput = %g\n",
                c_names[c], resp_c[c], tput[c]);
    fprintf(f, "\n\n");
    for (k = 0; k < nqueue; k++)
        fprintf(f, "%s:\n\tutilization = %g\n\tmean queue length = %g\n",
                q_names[k], util_final[k], qlen_final[k]);
    fprintf(f, "\n\n");
    fprintf(f, "DETAILED STATISTICS:\n\n");
    fprintf(f, "response times:\n");
    for (c = 0; c < nclass; c++)
        fprintf(f, "\t%s", c_names[c]);
    fprintf(f, "\n");
    for (k = 0; k < nqueue; k++) {
        fprintf(f, "%s", q_names[k]);
        for (c = 0; c< nclass; c++)
            fprintf(f, "\t%g", resp[c + k*nclass]);
        fprintf(f, "\n");
    }
    fprintf(f, "\n");
    fprintf(f, "queue lengths:\n");
    for (c = 0; c< nclass; c++)
        fprintf(f, "\t%s", c_names[c]);
    fprintf(f, "\n");
    for (k = 0; k < nqueue; k++) {
        fprintf(f, "%s", q_names[k]);
        for (c = 0; c< nclass; c++)
            fprintf(f, "\t%g", qlen[c + k*nclass]);
        fprintf(f, "\n");
    }
    fprintf(f, "\n");
    fprintf(f, "utilizations:\n");
    for (c = 0; c< nclass; c++)
        fprintf(f, "\t%s", c_names[c]);
    fprintf(f, "\n");
    for (k = 0; k < nqueue; k++) {
        fprintf(f, "%s", q_names[k]);
        for (c = 0; c< nclass; c++)
            fprintf(f, "\t%g", util[c + k*nclass]);
        fprintf(f, "\n");
    }
    __eskel_free(util_final);
    __eskel_free(qlen_final);
    fclose(f);
    return 0;
}
