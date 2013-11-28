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
   
   Generate combination C(n,k) from a given unique set of items, where
   n is the cardinality of the item set, and k is the number of items
   that are chosen for each combination. We use the cool-lex algorithm
   due to Frank Ruskey and Aaron Williams.
   
   Ruskey, F. and Williams, A., "Generating combination by prefix
   shifts", (extended abstract) COCOON 2005. */

#ifndef __ESKEL_COMPILE
#error "Please do not include 'coollex.h'; include 'eskel.h' instead."
#endif

#ifndef __ESKEL_COOLLEX_H
#define __ESKEL_COOLLEX_H

/* The function fills in the possible mappings. The arguments are:
   mtab  - The mapping table which contains the process indices.
   nrem  - The number of processors remaining unused.
   nused - The number of processors used (equals the number of tasks). */
extern int __eskel_coollex(unsigned int *mtab,
                           unsigned int nrem,
                           unsigned int nused);

#endif /* __ESKEL_COOLLEX_H */
