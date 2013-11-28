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
  Test source code.
  
  Written by: Gagarine Yaikhom
  University of Edinburgh
*/

#include <eskel.h>

#define MAXARRAY 100
long int m[MAXARRAY];
double a[MAXARRAY];

void work (void) {
    int i, count = 0;
    for (i = 0; i < MAXARRAY; i++) {
        a[i] = (double) random();
        m[i] = random();
    }
    
    for (i = 0; i < MAXARRAY; i++) {
        a[i] = a[MAXARRAY - i] * m[i];
        if (i == MAXARRAY - 1) {
            i = 0;
            count++;
            if (count == 999) break;
        }
    }
}

int first (void) {
    static int d = 0; /* State variables should be defined static. */
    eskel_state_register(&d, sizeof(int), ESKEL_PERSISTENT);
    eskel_begin();
    while (d < 100) {
        work();
        eskel_give(&d, 1);
        d++;
        if ((d % 10) == 0) printf("%d ", d);
    }
    printf("(First stage concludes...)\n");
    eskel_end();
}

int second (void) {
    int d;
    eskel_begin();
    while (1) {
        eskel_take(&d, 1);
        work();
        eskel_give(&d, 1);
        if (d == 99) break;
    }
    printf("(Second stage concludes...)\n");
    eskel_end();
}

int third (void) {
    int d;
    eskel_begin();
    while (1) {
        eskel_take(&d, 1);
        work();
        if (d == 99) break;
    }
    printf("(Third stage concludes...)\n");
    eskel_end();
}

int main (int argc, char *argv[]) {

    nice(10);

    eskel_init(&argc, &argv); /* Initialise skeleton library. */
    /* Start defining the skeleton hierarchy. */
    eskel_pipe(3);
    eskel_task(first, MPI_INT, MPI_INT);
    eskel_task(second, MPI_INT, MPI_INT);
    eskel_task(third, MPI_INT, MPI_INT);

    eskel_htree_commit(); /* Commit skeleton hierarchy tree. */
    eskel_htree_display(SHOW_INDEX|SHOW_NAME); /* Display hierarchy. */

    /* Attach scheduler. */
    eskel_set_scheduler(__eskel_pepa_schedule,
                        __eskel_pepa_init,
                        __eskel_pepa_final);

    eskel_exec(1); /* Execute skeleton hierarchy. */
    eskel_final(); /* Finalise skeleton library. */
    return 0;
}
