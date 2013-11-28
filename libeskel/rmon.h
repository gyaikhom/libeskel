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

#ifndef __ESKEL_COMPILE
#error "Please do not include 'rmon.h'; include 'eskel.h' instead."
#endif

#ifndef __ESKEL_RMON_H
#define __ESKEL_RMON_h

#define FSHIFT          16          /* nr of bits of precision */
#define FIXED_1         (1<<FSHIFT) /* 1.0 as fixed-point */
#define LOAD_INT(x) ((x) >> FSHIFT)
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1-1)) * 100)
#define __ESKEL_BASE_PORT 89898989

typedef struct __eskel_rmon_s __eskel_rmon_t;
struct __eskel_rmon_s {
    pid_t pid;             /* Process ID. */
    float cpufreq;         /* CPU clock speed. */
    int cpucount[2];       /* Number of cpus: configured and online. */
    float load[3];         /* The 1, 5, 15 minute load average. */
    float total, contrib;  /* Total CPU util., and my contrib.. */
    float theor;           /* Theoretical value based on nice values. */
    unsigned short nprocs; /* Total number of processes. */
    unsigned short rprocs; /* Total number of running processes. */
    double duration;       /* Duration of the current task. */
};

extern int eskel_rmon_spawn (pthread_t *rmon_thread);
extern int eskel_rmon_request(__eskel_rmon_t *r, int rank);
extern int eskel_rmon_final(pthread_t *rmon_thread);

#endif /* __ESKEL_RMON_H */
