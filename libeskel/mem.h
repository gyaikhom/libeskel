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
  Memory accountant for the eSkel skeleton library.

  Written by: Gagarine Yaikhom (g.yaikhom@sms.ed.ac.uk)
  School of Informatics, University of Edinburgh
*/

#ifndef __ESKEL_COMPILE
#error "Please do not include 'mem.h'; include 'eskel.h' instead."
#endif

#ifndef __ESKEL_MEM_H
#define __ESKEL_MEM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The memory accountant is used to avoid memory leaks when the eSkel
   skeleton library is used. I think it is not good proctice to leave
   memory allocated after the skeleton library has concluded. Since
   this introduces some house keeping chores, it is better to undefine
   __ESKEL_DEBUG_MEMORY (see eskel.h) when everything related to the
   skeleton library is running as expected.

   Note: Valgrind is an excellent utility application for checking
   memory leaks for a small number of processes; however, it becomes
   very difficult to obtain the required information easily because
   the application should be run on top of valgrind (which is
   sometimes impractical). */

#ifdef __ESKEL_DEBUG_MEMORY
extern void *__eskel_x_malloc(size_t sz, char *fname, int line);
extern void __eskel_x_free(void *ptr, char *fname, int line);
extern void __eskel_x_check_leaks(int flag);
#define __eskel_malloc(X) __eskel_x_malloc((X), __FILE__, __LINE__)
#define __eskel_free(X) {                     \
    __eskel_x_free((X), __FILE__, __LINE__);  \
    (X) = NULL;                               \
}
#else
#define __eskel_malloc(X) malloc((X))
#define __eskel_free(X) if ((X)) { free((X)); (X) = NULL; }
#endif

#endif /* __ESKEL_MEM_H */
