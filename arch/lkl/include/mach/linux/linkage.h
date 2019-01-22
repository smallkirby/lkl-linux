#ifndef _MACH_LKL_LINUX_LINKAGE_H
#define _MACH_LKL_LINUX_LINKAGE_H

#include "../../../../../include/linux/linkage.h"

#undef cond_syscall
#define cond_syscall(x)	asm(				\
	".weak_definition " VMLINUX_SYMBOL_STR(x) "\n\t"		\
	".set  " VMLINUX_SYMBOL_STR(x) ","		\
		 VMLINUX_SYMBOL_STR(sys_ni_syscall))

#endif
