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
  MPI data communication internal tags.

  Written by: Gagarine Yaikhom (g.yaikhom@sms.ed.ac.uk)
  School of Informatics, University of Edinburgh
*/

#ifndef __ESKEL_COMPILE
#error "Please do not include 'tags.h'; include 'eskel.h' instead."
#endif

#ifndef __ESKEL_TAGS_H
#define __ESKEL_TAGS_H

#define MAX_SKEL_NAME 16
#define MAX_STACK_SIZE 16
#define __ESKEL_SOURCE_IFACE -1
#define __ESKEL_SINK_IFACE -2
#define SOURCE_ANY -3
#define SINK_ANY -4
#define MAX_NSOURCES 100
#define MAX_NDESTINATIONS 100

/* Used int diaplsy tree. */
#define SHOW_INDEX 0x0001
#define SHOW_PRED  0x0002
#define SHOW_SUCC  0x0004
#define SHOW_NAME  0x0008

#define ESKEL_NO_TASK -1
#define ESKEL_SCHEDULER 0
#define ESKEL_DONT_RESCHEDULE 99
#define ESKEL_RESCHEDULE 98
#define ESKEL_COMPLETED  97
#define ESKEL_ACCEPTED 96
#define ESKEL_ACKNOWLEDGEMENT 95
#define ESKEL_CONTINUE 94
#define ESKEL_DATA 93          /* Data (no scheduling) */
#define ESKEL_NEW_SCHEDULE 92  /* No data (only schedule) */
#define ESKEL_DATA_SCHEDULE 91 /* Both data and schedule. */
#define ESKEL_STATE 90

#endif /* __ESKEL_TAGS_H */
