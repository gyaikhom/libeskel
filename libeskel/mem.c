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

   These interfaces are used to monitor memory allocation and
   deallocations  within the eSkel skeleton library. Very important
   for ensuring that there are no memory leaks from the library
   run-time system. Used only when memory debugging is requested by
   defining the corresponding macro in "eskel.h". To reduce impact on
   the performance of execution, user applications avoid these
   functions by using native memory interfaces (see "mem.h"). */

#include <eskel.h>

#ifdef __ESKEL_DEBUG_MEMORY
static struct __eskel_mem_s {
    void *ptr;
    size_t sz;
    int usage;
    int line[2];
    char file[2][16];
} __eskel_mem[512];
static int __eskel_mem_count = -1;

void *__eskel_x_malloc(size_t sz, char *fname, int line) {
    void *ptr;
    int i = 0;
    ptr = malloc(sz);
    
    if (ptr && (__eskel_mem_count < 512)) {
        /* Check if the address has already been used.
           Assumption: malloc() does not allocate the same address
           twice. Hence, if malloc() returned the same address, the
           previous allocation was freed by the time this was invoked. */
        __eskel_mem_count++;
        while (i < __eskel_mem_count) {
            if (__eskel_mem[i].ptr == ptr)
                break;
            i++;
        }

        /* No, this is the first use of this address. */
        if (i == __eskel_mem_count) {
            __eskel_mem[i].ptr = ptr;
            __eskel_mem[i].usage = 1;
        } else __eskel_mem[i].usage++;

        __eskel_mem[i].sz = sz;
        __eskel_mem[i].line[0] = line;
        __eskel_mem[i].line[1] = -1;
        strcpy(__eskel_mem[i].file[0], fname);
        strcpy(__eskel_mem[i].file[1], "*");
    }
    return ptr;
}

void __eskel_x_free(void *ptr, char *fname, int line) {
    int i = 0;
    if (!ptr) return;
    while (i < __eskel_mem_count) {
        if (__eskel_mem[i].ptr == ptr) {
            __eskel_mem[i].usage--;
            __eskel_mem[i].line[1] = line;
            strcpy(__eskel_mem[i].file[1], fname);
            break;
        }
        i++;
    }
    free(ptr);
}

void __eskel_x_check_leaks(int flag) {
    int i = 0;
    int leaks = 0;
    while (i < __eskel_mem_count) {
        if ((__eskel_mem[i].usage != 0) || flag) {
            printf("[%d]   mem - [0x%X] %4d bytes +[%s: %d] "
                   "-[%s: %d] %2d\n", __eskel_rt.rank,
                   (unsigned int)__eskel_mem[i].ptr,
                   __eskel_mem[i].sz, __eskel_mem[i].file[0],
                   __eskel_mem[i].line[0], __eskel_mem[i].file[1],
                   __eskel_mem[i].line[1], __eskel_mem[i].usage);
            if (__eskel_mem[i].usage != 0)
                leaks += __eskel_mem[i].sz;
        }
        i++;
    }
    if (leaks)
        printf("[%d]   mem - Total leaks: %d bytes\n",
               __eskel_rt.rank, leaks);
}
#endif /* __ESKEL_DEBUG_MEMORY */
