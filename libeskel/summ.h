/*********************************************************************

  THE ENHANCE PROJECT
  School of Informatics,
  University of Edinburgh,
  Edinburgh - EH9 3JZ
  United Kingdom

  Written by: Gagarine Yaikhom

*********************************************************************/

#ifndef __ESKEL_COMPILE
#error "Please do not include 'summ.h'; include 'eskel.h' instead."
#endif

#ifndef __ESKEL_SUMM_H
#define __ESKEL_SUMM_H

extern int __eskel_init_summary(char *fname);
extern int __eskel_final_summary(void);
extern int __eskel_write_env(void);
extern int __eskel_write_exe(void);
extern int __eskel_write_ske(void);
extern int __eskel_write_dyn(int map);

#endif /* __ESKEL_SUMM_H */
