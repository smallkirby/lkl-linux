#ifndef _MACH_LKL_LINUX_PERCPU_DEFS_H
#define _MACH_LKL_LINUX_PERCPU_DEFS_H

#include "../../../../../include/linux/percpu-defs.h"

#undef __PCPU_ATTRS
#define __PCPU_ATTRS(sec)						\
	__percpu __attribute__((__section__("__DATA,data")))		\
	PER_CPU_ATTRIBUTES

#endif /* _MACH_LKL_LINUX_PERCPU_DEFS_H */
