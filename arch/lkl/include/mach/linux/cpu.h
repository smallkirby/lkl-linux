#ifndef _MACH_LKL_LINUX_CPU_H_
#define _MACH_LKL_LINUX_CPU_H_

#include "../../../../../include/linux/cpu.h"

#undef __cpuidle
#define __cpuidle	__attribute__((__section__("__TEXT,.cpuidle.text")))

#endif /* _MACH_LKL_LINUX_CPU_H_ */
