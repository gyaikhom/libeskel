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
   This provides a multithreaded process resource monitor which is
   spawned after eskel_init(). */

#include <eskel.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

extern int sysinfo(struct sysinfo* info);
static int nagle_flag = 0;          /* Disable Nagle Algorithm for the request sockets. */
static struct linger lng = {1, 10}; /* Linger setting on, 10 seconds. */
double *latency;
extern double __eskel_task_duration;

static int get_cpu_info(__eskel_rmon_t *r) {
    FILE *f;
    char *buff, *mstr;
    size_t bytes_read;
    if (r->cpufreq != 0.0) return 0;

    /* Get the number of configured and online CPUs. */
    r->cpucount[0] = sysconf(_SC_NPROCESSORS_CONF);
    r->cpucount[1] = sysconf(_SC_NPROCESSORS_ONLN);

    /* Default to one processor only. */
    if (r->cpucount[0] == -1) r->cpucount[0] = 1;
    if (r->cpucount[1] == -1) r->cpucount[1] = 1;

    /* FIXME: This is quite annoying. Better method should be devided so
       that the frequencies for each of the CPUs are correctly set. At
       the moment, it is simply assumed that all the CPUs have the
       same frequency (which is not right).

       I really hate this code segment :( */
    if (!(buff = (char *) __eskel_malloc(sizeof(char)*1024))) return -1;
    if (!(f = fopen ("/proc/cpuinfo", "r"))) goto exit_error;
    bytes_read = fread(buff, 1, 1024, f);
    fclose (f);
    if (bytes_read == 0 || bytes_read == sizeof (buff)) goto exit_error;
    buff[bytes_read] = '\0';
    if (!(mstr = strstr(buff, "cpu MHz"))) goto exit_error;
    sscanf (mstr, "cpu MHz : %f", &r->cpufreq);
    __eskel_free(buff);
    return 0;
 exit_error:
    __eskel_free(buff);
    return -1;
}

/* Get the average load (1min, 5 mins, 15 mins). This is nothing more
   that the dampened load average which uses kernel values. */
static int get_avg_load(__eskel_rmon_t *r) {
    struct sysinfo info;
    sysinfo(&info);
    r->nprocs = info.procs;
    r->load[0] = LOAD_INT(info.loads[0]) + 0.01 * LOAD_FRAC(info.loads[0]);
    r->load[1] = LOAD_INT(info.loads[1]) + 0.01 * LOAD_FRAC(info.loads[1]);
    r->load[2] = LOAD_INT(info.loads[2]) + 0.01 * LOAD_FRAC(info.loads[2]);
    return 0;
}

/* Here, we are assuming that the total number of user processes are
   at the most MAX_NPROCS. Well, a better way should be devised. */
#define MAX_NPROCS 256
static struct ps_fields_s {
    unsigned int pid; /* Process id. */
    int nice;         /* Nice value. */
    float pcpu;       /* % CPU. */
    char status[8];   /* Status of the process. */
} ps_fields[MAX_NPROCS];
static int nfields; /* Number of processes recorded. */
static int get_precise_info(void) {
    FILE *f;
    int status;
    char nice_string[8];
    char ps_command[128] =
        "ps -e -o pid= -o ni= -o pcpu= -o stat= > ";
    /* The name of the temporary file to which we direct the
       output is the md5 checksum of "eskel_library_ps_temp_file"
       , which is "326b8d218a559c2debaeb5dbcd92543f". */
    char fname[64];

    /* Execute the 'ps' command and output the results into a
       temporary file for parsing. */
    sprintf(fname, "326b8d218a559c2debaeb5dbcd92543f_PROCESS_%d",
            __eskel_rt.rank);
    strcat(ps_command, fname);
    switch(fork()) {
    case -1:
        perror("Cannot fork ps command");
        return -1;
    case 0:
        execl("/bin/sh", "sh", "-c", ps_command, NULL);
    }
    if (wait(&status) < 0) return -1;
    if (!WIFEXITED(status)) return -1;
    
    /* Open and parse the ps output. */
    if ((f = fopen(fname, "r"))) {
        nfields = 0;
        while(nfields < MAX_NPROCS) {
            if (fscanf(f, "%d %s %f %s",
                       &(ps_fields[nfields].pid),
                       nice_string,
                       &(ps_fields[nfields].pcpu),
                       ps_fields[nfields].status) == EOF)
                break;
            if (nice_string[0] == '-')
                continue;
            ps_fields[nfields].nice = atoi(nice_string);
            nfields++;
        }
        fclose(f);
        remove(fname);
    }
    return 0;
}
#undef MAX_NPROCS

/* This is an attempt to provide more information on the current state of
   the resources. At the moment, we use the 'ps' command, but this
   should be changed with an internal implementation of 'top', which
   registers changes more immediately. */
static int what_is_really_happening(__eskel_rmon_t *r) {
    int i;
    float normalized, total, my_normalized = 0.0;

    get_avg_load(r);
    get_cpu_info(r);
    get_precise_info();

    /* Analyse the results. */
    r->total = 0.0;
    r->contrib = 0.0;
    r->rprocs = 0;

    for (i = 0, total = 0.0; i < nfields; i++) {
        /* Convert nice values so that they are not negative. Also, reverse
           these values so higher chunk size means higher nice value
           (remember, the nice values received does the opposite).*/
        normalized = 39 - (20 + ps_fields[i].nice);
        r->total += ps_fields[i].pcpu;
        if (__eskel_rt.pid == ps_fields[i].pid) {
            r->contrib = ps_fields[i].pcpu;
            /* Retain my nice factor for determination of chunk size. */
            my_normalized = normalized;
        }
        if (ps_fields[i].status[0] == 'R') {
            r->rprocs++;
            total += normalized;
        } else {
            /* Although this node is not assigned any task, what will
               be its effect on the node if a task was assigned. My
               contribution should be accounted for. */
            if (__eskel_rt.pid == ps_fields[i].pid)
                total += normalized;
        }
    }
    r->theor = ((float)r->cpucount[1] * 99.99 * my_normalized) / total;
}

#ifdef __ESKEL_DEBUG_RMONITOR
static void print_resource_info(__eskel_rmon_t *r) {
    if (r)
        printf("[%d]  rmon - %d %f [%f %f %f] %f:%f:%f %d(%d)\n",
               __eskel_rt.rank, __eskel_rt.pid, r->cpufreq,
               r->load[0], r->load[1], r->load[2],
               r->total, r->contrib, r->theor, r->nprocs, r->rprocs);
}
#endif

#define ESKEL_LATENCY 10
static int __eskel_mpi_hnames(void) {
    register unsigned int i, j, count;
    int nlen = 0;
    double __eskel_start_time = 0.0;

    if (!(__eskel_rt.hnames = (char **)
          __eskel_malloc(sizeof(char *) * __eskel_rt.nproc)))
        return -1;
    for (i = 0; i < __eskel_rt.nproc; i++) {
        if(!(__eskel_rt.hnames[i] = (char *)
             __eskel_malloc(sizeof(char) * MPI_MAX_PROCESSOR_NAME + 1))) {
            for (j = 0; j < i; j++)
                __eskel_free(__eskel_rt.hnames[j]);
            __eskel_free(__eskel_rt.hnames);
            return -1;
        }
    }
    MPI_Get_processor_name(__eskel_rt.hnames[__eskel_rt.rank], &nlen);

    if (__eskel_rt.rank == 0)
        j = __eskel_rt.nproc*__eskel_rt.nproc;
    else
        j = __eskel_rt.nproc;
    if (!(latency = (double *) calloc(j, sizeof(double)))) {
        perror("Allocate latency matrix");
        return -1;
    }

    for (i = 0; i < __eskel_rt.nproc; i++) {

        /* The following broadcast is enough. However, we are sending the
           messages point-to-point so that we get communication
           latency which will be used for PEPA modelling.

           MPI_Bcast(__eskel_rt.hnames[i], count,
           MPI_CHAR, i, 
           MPI_COMM_WORLD);

           FIXME: This is not fine. A method for dynamic determination
           of the internode communication latency should be
           devised. Well, for the moment, we assume that the latency
           does not contributed too much on the scheduling since we
           are varying the load significantly. However, this
           assumption is wrong for live applications.

           Greg: This will be tricky to implement since the
           communication latency may be corrupted by the inter-process
           communications due to MPI processes (which should be
           discounted while measuring the latency).    */

        if (i == __eskel_rt.rank) {
            count = nlen + 1;
#ifdef __ESKEL_DEBUG_RMONITOR
            printf ("[%d]  rmon - Broadcasting my name \"%s\".\n",
                    __eskel_rt.rank, __eskel_rt.hnames[i]);
#endif
            for (j = 0; j < __eskel_rt.nproc; j++)
                if (j != __eskel_rt.rank) {
                    __eskel_start_time = MPI_Wtime();
                    MPI_Send(__eskel_rt.hnames[i], count, MPI_CHAR, j,
                             ESKEL_LATENCY, MPI_COMM_WORLD);
                    latency[j] = MPI_Wtime() - __eskel_start_time;
                } else latency[j] = 0.0;
        } else {
            count = MPI_MAX_PROCESSOR_NAME + 1;
            MPI_Recv(__eskel_rt.hnames[i], count, MPI_CHAR, i,
                     ESKEL_LATENCY, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }
    MPI_Gather(latency, __eskel_rt.nproc, MPI_DOUBLE, 
               latency, __eskel_rt.nproc, MPI_DOUBLE,
               0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);

#ifdef __ESKEL_DEBUG_RMONITOR
    printf("[%d]  rmon - Latency [ ", __eskel_rt.rank);
    for (j = 0; j < __eskel_rt.nproc; j++)
        printf("%f ", latency[j]);
    printf("]\n");
#endif
    return 0;
}
#undef ESKEL_LATENCY

static void *__eskel_rmon_thread(void *arg) {
    char r;
    __eskel_rmon_t rmon;

#ifdef __ESKEL_DEBUG_RMONITOR
    printf ("[%d]  rmon - Thread started...\n", __eskel_rt.rank);
#endif
    rmon.cpufreq = 0.0;
    rmon.pid = __eskel_rt.pid;
    while (1) {
        while (recv(__eskel_rt.rmon_fd[0], &r, sizeof(char),
                    MSG_WAITALL) != sizeof(char));
#ifdef __ESKEL_DEBUG_RMONITOR
        printf ("[%d]  rmon - Received %s.\n", __eskel_rt.rank,
                (r == 'r') ? "resource request" : "exit signal");
#endif
        if (r == 'r') {
            what_is_really_happening(&rmon);
            rmon.duration = __eskel_task_duration;
            /*                print_resource_info(&rmon); */
            write(__eskel_rt.rmon_fd[0], &rmon, sizeof(rmon));
        } else if (r == 'e') {
            close(__eskel_rt.rmon_fd[0]);
#ifdef __ESKEL_DEBUG_RMONITOR
            printf ("[%d]  rmon - Thread exiting...\n", __eskel_rt.rank);
#endif
            pthread_exit(NULL);
            return NULL;
        }
    }
    return NULL;
}

static int __eskel_rmon_listener(int *lsock) {
    int optval = 1;
    struct sockaddr_in inet;

    /* Create a listener socket. */
    if ((*lsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("Creating request listener");
        return -1;
    }
    
    /* Set the request socket options. */
    setsockopt(*lsock, SOL_SOCKET, SO_LINGER,
               (char *) &lng, sizeof(lng));
    setsockopt(*lsock, SOL_SOCKET, SO_REUSEADDR,
               (void *) &optval, sizeof(int));
    setsockopt(*lsock, IPPROTO_TCP, TCP_NODELAY,
               (char *)&nagle_flag, sizeof(nagle_flag));
    
    memset((void *) &inet, 0, sizeof(inet));
    inet.sin_family = AF_INET;
    inet.sin_addr.s_addr = INADDR_ANY;
    inet.sin_port = __ESKEL_BASE_PORT + __eskel_rt.rank;

#ifdef __ESKEL_DEBUG_RMONITOR
    printf ("[%d]  rmon - Binding port %d\n",
            __eskel_rt.rank, inet.sin_port);
#endif    
    if (bind(*lsock, (struct sockaddr *) &inet, sizeof(inet)) < 0) {
        close(*lsock);
        perror("Binding request listener");
        return -1;
    }
    if (listen(*lsock, 1) < 0) {
        close(*lsock);
        perror("Start request listener");
        return -1;
    }
    return 0;
}

static int __eskel_rmon_connect(int rank) {
    struct hostent *hostp;
    struct sockaddr_in remote;
    int fd;

    if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        return -1;
    setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *)&lng, sizeof(lng));
    if (!(hostp = gethostbyname(__eskel_rt.hnames[rank]))) {
        perror("Get host by name");
        return -1;
    }
    memset((void *) &remote, 0, sizeof(remote));
    memcpy((void *) &remote.sin_addr,
           hostp->h_addr, hostp->h_length);
    remote.sin_family = AF_INET;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
               (char *)&nagle_flag, sizeof(nagle_flag));
    remote.sin_port = __ESKEL_BASE_PORT + rank;
#ifdef __ESKEL_DEBUG_RMONITOR
    printf("[%d]  rmon - Establish connection with n[%d].\n",
           __eskel_rt.rank, rank);
#endif
    while (connect(fd, (struct sockaddr *)&remote, sizeof(remote)) < 0);
#ifdef __ESKEL_DEBUG_RMONITOR
    printf("[%d]  rmon - Connection established with n[%d] at port %d.\n",
           __eskel_rt.rank, rank, fd);
#endif
    return fd;
}

static int __eskel_rmon_accept(int lsock) {
    struct sockaddr_in client;
    socklen_t addrlen = sizeof(client);
    fd_set rmask, mask;
    int retval;

    FD_ZERO(&mask);
    FD_SET(lsock, &mask);

    do {
        rmask = mask;
        retval = select(lsock + 1, &rmask, NULL, NULL, NULL);
        if (retval < 0) continue;
        if (FD_ISSET(lsock, &rmask)) break;
    } while (1);

    /* Accept a request for connection. */
    while ((__eskel_rt.rmon_fd[0] =
            accept(lsock, (struct sockaddr *) &client, &addrlen)) < 0);
    return 0;
}

int __eskel_rmon_network_create(void) {
    int i, j = __eskel_rt.nproc - 1, lsock;

    /* The set of file descriptors for the resource monitor should be
       created by the scheduler only. Rest of the processes only
       create one file descriptor to connect to the scheduler. */
    if (__eskel_rt.rank == 0) {
        if (!(__eskel_rt.rmon_fd = (int *)
              __eskel_malloc(j * sizeof(int))))
            return -1;
        
        /* Establish conncetion with all the available processors. */
        for (i = 0; i < j; i++)
            __eskel_rt.rmon_fd[i] = __eskel_rmon_connect(i + 1);
    } else {
        if (!(__eskel_rt.rmon_fd = (int *) __eskel_malloc(sizeof(int))))
            return -1;
        
        /* Create a listener socket for the scheduler to connect to. */
#ifdef __ESKEL_DEBUG_RMONITOR
        printf("[%d]  rmon - Creating listener.\n", __eskel_rt.rank);
#endif
        if (__eskel_rmon_listener(&lsock) == -1) {
            __eskel_free(__eskel_rt.rmon_fd);
#ifdef __ESKEL_DEBUG_RMONITOR
            printf("[%d]  rmon - Failure to create network.\n", __eskel_rt.rank);
#endif
            return -1;
        }
        
        /* Accept connection from scheduler. */
#ifdef __ESKEL_DEBUG_RMONITOR
        printf("[%d]  rmon - Accepting connection from scheduler.\n", __eskel_rt.rank);
#endif
        __eskel_rmon_accept(lsock);
        
        /* Now that the conncetions are ready, we do not need these. */
        close(lsock);
    }
#ifdef __ESKEL_DEBUG_RMONITOR
    printf("[%d]  rmon - Network created sucessfully.\n", __eskel_rt.rank);
#endif
    return 0;
}

int eskel_rmon_spawn (pthread_t *rmon_thread) {
    __eskel_mpi_hnames();
    __eskel_rmon_network_create();
    if (__eskel_rt.rank != 0)
        pthread_create(rmon_thread, (void *) NULL,
                       __eskel_rmon_thread, (void *) NULL);
    return 0;
}

/* Rank excluding the scheduler. */
int eskel_rmon_request (__eskel_rmon_t *r, int rank) {
    char c = 'r';
    send(__eskel_rt.rmon_fd[rank], &c, sizeof(char), 0);
    recv(__eskel_rt.rmon_fd[rank], r,
         sizeof(__eskel_rmon_t), MSG_WAITALL);
    return 0;
}

int eskel_rmon_final (pthread_t *rmon_thread) {
    char c = 'e';
    int i, j = __eskel_rt.nproc - 1;

    /* Shutdown sockets from my side. */
    if (__eskel_rt.rank == 0)
        for (i = 0; i < j; i++) {
            send(__eskel_rt.rmon_fd[i], &c, sizeof(char), 0);
            close(__eskel_rt.rmon_fd[i]);
        }
    else pthread_join(*rmon_thread, NULL);
    for (i = 0; i < __eskel_rt.nproc; i++)
        __eskel_free(__eskel_rt.hnames[i]);
    __eskel_free(__eskel_rt.hnames);
    __eskel_free(__eskel_rt.rmon_fd);
    __eskel_free(latency);
    return 0;
}
