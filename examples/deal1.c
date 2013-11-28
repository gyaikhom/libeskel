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

int dummy_scheduler(int *s) {
    int sched[4][6] = {{1, 2, 3, 4, 5, 7},
                       {2, 1, 4, 3, 5, 6},
                       {2, 3, 1, 5, 6, 7},
                       {1, 3, 2, 4, 5, 6}};
    static int i = 0;
    int j;

    for (j = 0; j < 6; j++) {
        s[j] = sched[i][j];
    }
    i++;
    if (i == 1) i = 0;
    return 0;
}

int producer (void) {
    static int d = 0; /* State variables should be defined static. */
    eskel_state_register(&d, sizeof(int), ESKEL_PERSISTENT); /* Register state variable. */

    eskel_begin();
    while (d < 9000) {
        eskel_give(&d, 1);
        d++;
    }
    printf("(Producer concludes...)\n");
    eskel_end();
}

int distributor (void) {
    int d, i;

    eskel_begin(); /* Commit all the state variables. */
    while (1) {
        eskel_take(&d, 1);
        if (d == 8999) break;
        eskel_give(&d, 1);
    }
    for (i = 0; i < 3; i++)
        eskel_give(&d, 1);
    printf("(Distributor concludes...)\n");
    eskel_end();
}

int worker (void) {
    int d;

    eskel_begin();
    while (1) {
        eskel_take(&d, 1);
        if (d == 8999) {
            eskel_give(&d, 1);
            break;
        } else {
            d++;
            eskel_give(&d, 1);
        }
    }
    printf("(Worker concludes...)\n");
    eskel_end();
}

int consumer (void) {
    int d, i;

    eskel_begin();
    while (1) {
        eskel_take(&d, 1);
        if (d == 8999) break;
        /* printf("[%d] %d\n", __eskel_rt.rank, d); */
    }
    for (i = 0; i < 2; i++)
        eskel_take(&d, 1);    
    printf("(Consumer concludes...)\n");
    eskel_end();
}

int main (int argc, char *argv[]) {
    eskel_init (&argc, &argv); /* Initialise skeleton library. */
    /* Start defining the skeleton hierarchy. */
    eskel_pipe(4);
    eskel_task(producer, MPI_INT, MPI_INT);
    eskel_task(distributor, MPI_INT, MPI_INT);
    eskel_deal(3, worker, MPI_INT, MPI_INT);
    eskel_task(consumer, MPI_INT, MPI_INT);
    eskel_htree_commit(); /* Commit skeleton hierarchy tree. */
    eskel_htree_display(SHOW_NAME | SHOW_INDEX | SHOW_PRED | SHOW_SUCC); /* Display hierarchy. */

    /* Attach scheduler. */
    // eskel_set_scheduler(dummy_scheduler, NULL, NULL);

    // eskel_exec(); /* Execute skeleton hierarchy. */
    eskel_final(); /* Finalise skeleton library. */
    return 0;
}
