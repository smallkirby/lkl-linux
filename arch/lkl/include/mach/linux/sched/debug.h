#ifndef _MACH_LKL_LINUX_SCHED_DEBUG_H
#define _MACH_LKL_LINUX_SCHED_DEBUG_H

#include "../../../../../../include/linux/sched/debug.h"

#undef __sched
#define __sched		__attribute__((__section__("__TEXT,.sched.text")))

#endif /* _MACH_LKL_LINUX_SCHED_DEBUG_H */
