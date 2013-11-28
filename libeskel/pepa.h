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

#ifndef __ESKEL_COMPILE
#error "Please do not include 'pepa.h'; include 'eskel.h' instead."
#endif

#ifndef __ESKEL_PEPA_H
#define __ESKEL_PEPA_H

/* This specifies the maximum number of mappings that could be used
   for automatic generation. */
#define __ESKEL_MAX_MAPPINGS 512

extern int __eskel_pepa_init(void);
extern int __eskel_pepa_final(void);
extern int __eskel_pepa_schedule(int *s);
extern int __eskel_pepa_prop(void);

/* Communication latency between PEPA processes. */
extern double *latency;

#endif /* __ESKEL_PEPA_H */
