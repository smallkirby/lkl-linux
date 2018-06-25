#ifndef _MACH_LKL_LINUX_INTERRUPT_H
#define _MACH_LKL_LINUX_INTERRUPT_H

#include "../../../../../include/linux/interrupt.h"

#undef __softirq_entry
#define __softirq_entry  \
	__attribute__((__section__("__TEXT,.sirq.text")))

#endif
