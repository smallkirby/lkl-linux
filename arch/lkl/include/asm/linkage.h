/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_LKL_LINKAGE_H
#define _ASM_LKL_LINKAGE_H

#include <asm/types.h>

#ifdef __APPLE__
#define cond_syscall(x)							\
  long __weak x(void) {							\
    return sys_ni_syscall();						\
      }
#endif

#endif/* _ASM_LKL_LINKAGE_H */
