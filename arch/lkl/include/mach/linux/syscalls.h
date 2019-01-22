#ifndef _MACH_LKL_LINUX_SYSCALLS_H
#define _MACH_LKL_LINUX_SYSCALLS_H

#include "../../../../../include/linux/syscalls.h"

#undef __SYSCALL_DEFINEx
#define __PROTECT(...) asmlinkage_protect(__VA_ARGS__)
#define __SYSCALL_DEFINEx(x, name, ...)					\
	asmlinkage long sys##name(__MAP(x,__SC_DECL,__VA_ARGS__))	\
		__attribute__((alias(__stringify(SyS##name))));		\
	static inline long SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__));	\
	asmlinkage long SyS##name(__MAP(x,__SC_LONG,__VA_ARGS__));	\
	asmlinkage long SyS##name(__MAP(x,__SC_LONG,__VA_ARGS__))	\
	{								\
		long ret = SYSC##name(__MAP(x,__SC_CAST,__VA_ARGS__));	\
		__MAP(x,__SC_TEST,__VA_ARGS__);				\
		__PROTECT(x, ret,__MAP(x,__SC_ARGS,__VA_ARGS__));	\
		return ret;						\
	}								\
	/* due to lack of alias support on mach-o */			\
	asmlinkage long sys##name(__MAP(x,__SC_DECL,__VA_ARGS__))	\
	{								\
		long ret = SYSC##name(__MAP(x,__SC_CAST,__VA_ARGS__));	\
		__MAP(x,__SC_TEST,__VA_ARGS__);				\
		__PROTECT(x, ret,__MAP(x,__SC_ARGS,__VA_ARGS__));	\
		return ret;						\
	}								\
	static inline long SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__))

#endif
