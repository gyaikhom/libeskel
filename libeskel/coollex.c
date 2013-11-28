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

#include <eskel.h>

int __eskel_coollex(unsigned int *mtab,
                    unsigned int nrem,
                    unsigned int nused) {
    unsigned int *b, st, tt, n, i, j, x, y, c = 0;
    st = nrem;
    tt = nused;
    n = st + tt;
    b = (unsigned int *) __eskel_malloc(sizeof(unsigned int) * n);

    /* Initialise. */
    for (i = 0; i < tt; i++) b[i] = 1;
    for (; i < n; i++) b[i] = 0;

    /* Fill in the first mapping. */
    for (i = 0, j = 0; i < n; i++)
        if (b[i] == 1) {
            mtab[c*__eskel_rt.nleaves + j] = i + 1;
            j++;
        }
    c++;

    /* Cool-lex. */
    x = 0;
    y = 0;
    do {
        if (x == 0) {
            x = 1;
            b[tt] = 1;
            b[0] = 0;
        } else {
            b[x++] = 0;
            b[y++] = 1;
            if (b[x] == 0) {
                b[x] = 1;
                b[0] = 0;
                if (y > 1) x = 1;
                y = 0;
            }
        }

        /* Fill in the mapping. */
        for (i = 0, j = 0; i < n; i++)
            if (b[i] == 1) {
                mtab[c*__eskel_rt.nleaves + j] = i + 1;
                j++;
            }
        c++;
    } while(x < (n - 1));
    __eskel_free(b);
    return 0;
}
