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
  Programmer interfaces for writing and reading eSkel skeleton library
  profile.

  Written by: Gagarine Yaikhom (g.yaikhom@sms.ed.ac.uk)
  School of Informatics, University of Edinburgh
*/

#include <eskel.h>

static FILE *__eskel_summary_file = NULL;

/* These are the sections under which the information are maintained. */
#define __ESKEL_SUMM_ENV 0
#define __ESKEL_SUMM_SKE 1
#define __ESKEL_SUMM_EXE 2
#define __ESKEL_SUMM_DYN 3
long sec_offset[4] = {-1L, -1L, -1L, -1L};

int __eskel_init_summary(char *fname) {
    if (!(__eskel_summary_file = fopen(fname, "wb"))) {
        perror("Cannot create summary file");
        return -1;
    }
    /* Skip section offset. */
    fseek(__eskel_summary_file, sizeof(long)*4, SEEK_SET);

    /* Set section offset for the dynamic part. */
    sec_offset[__ESKEL_SUMM_DYN] = ftell(__eskel_summary_file);

    /* Skip number of dynamic records. */
    fseek(__eskel_summary_file, sizeof(long), SEEK_CUR);

    return 0;
}

extern long __eskel_sched_count;
int __eskel_final_summary(void) {
    if (!__eskel_summary_file) return -1;
    fseek(__eskel_summary_file, 0L, SEEK_SET);
    fwrite(&sec_offset, sizeof(long)*4, 1, __eskel_summary_file);

    /* By the time this is called, we know the number of cycles.
       Fix the value for the number of resource count. */
    fwrite(&__eskel_sched_count, sizeof(long), 1, __eskel_summary_file);
    fclose(__eskel_summary_file);
    return 0;
}

int __eskel_write_env(void) {
    int i, j, k;

    /* Set section offset. */
    sec_offset[__ESKEL_SUMM_ENV] = ftell(__eskel_summary_file);

    /* Write number of nodes and their hostnames. */
    fwrite(&__eskel_rt.nproc, sizeof(int), 1, __eskel_summary_file);
    for (i = 0; i < __eskel_rt.nproc; i++) {
        k = strlen(__eskel_rt.hnames[i]);
        fwrite(&k, sizeof(int), 1, __eskel_summary_file);
        for (j = 0; j < k; j++) {
            fwrite(&__eskel_rt.hnames[i][j], sizeof(char),
                   1, __eskel_summary_file);
        }
    }

    /* Write the latency. */
    for (i = 0; i < __eskel_rt.nproc; i++) {
        for (j = 0; j < __eskel_rt.nproc; j++) {
            fwrite(__ESKEL_2DMBER_PTR(latency, i, j, __eskel_rt.nproc),
                   sizeof(double), 1, __eskel_summary_file);
        }
    }
    return 0;
}

extern time_t __eskel_exec_time[2];
extern int __eskel_nmaps;
extern int *__eskel_maps;
int __eskel_write_exe(void) {
    int i, j;

    /* Set section offset. */
    sec_offset[__ESKEL_SUMM_EXE] = ftell(__eskel_summary_file);

    /* Write the execution times. */
    fwrite(&__eskel_exec_time, sizeof(time_t), 2, __eskel_summary_file);

    /* Write number of possible mappings. */
    fwrite(&__eskel_nmaps, sizeof(int), 1, __eskel_summary_file);

    /* Write the number of tasks. */
    fwrite(&__eskel_rt.nleaves, sizeof(int), 1, __eskel_summary_file);

    /* Write the mappings. */
    for (i = 0; i < __eskel_nmaps; i++) {
        for (j = 0; j < __eskel_rt.nleaves; j++) {
            fwrite(__ESKEL_2DMBER_PTR(__eskel_maps, i, j, __eskel_rt.nleaves),
                   sizeof(int), 1, __eskel_summary_file);
        }
    }

    /* Write the recorded task durations. */
    for (i = 0; i < __eskel_rt.nleaves; i++) {
        fwrite(&(__eskel_rt.sstab[i]->duration), sizeof(double),
               1, __eskel_summary_file);
    }    
    return 0;
}

/* Write the hierarchy tree. */
void __eskel_write_htree(__eskel_node_t *n) {
    __eskel_child_t *c;
    int i;
    if (n) {
        fwrite(&(n->mtype), sizeof(__eskel_comp_t),
               1, __eskel_summary_file);
        switch(n->mtype) {
        case TASK:
            i = strlen(n->name);
            fwrite(&i, sizeof(int),
                   1, __eskel_summary_file);
            fwrite(n->name, sizeof(char),
                   i, __eskel_summary_file);
            break;
        case DEAL:
        case PIPE:
            fwrite(&(n->nchr), sizeof(int),
                   1, __eskel_summary_file);
            break;
        default:
            ;
        }
        c = n->clist;
        for (i = 0; c && i < n->nchr; i++) {
            __eskel_write_htree(c->child);
            c = c->next;
        }
    }
}

int __eskel_write_ske(void) {
    /* Set section offset. */
    sec_offset[__ESKEL_SUMM_SKE] = ftell(__eskel_summary_file);

    /* Write the number of nodes in the hierarchy tree. */
    fwrite(&__eskel_rt.nnodes, sizeof(int), 1, __eskel_summary_file);

    /* Write the hierarchy tree. */
    __eskel_write_htree(__eskel_rt.skel_tree);
    return 0;
}

extern __eskel_rmon_t *__eskel_rmon;
int __eskel_write_dyn(int map) {
    /* Write what mapping. */
    fwrite(&map, sizeof(int), 1, __eskel_summary_file);

    /* Write the resource information for this cycle. */
    fwrite(__eskel_rmon, sizeof(__eskel_rmon_t),
           __eskel_rt.nproc, __eskel_summary_file);

    return 0;
}
