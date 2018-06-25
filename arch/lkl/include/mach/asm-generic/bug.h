#ifndef _LKL_MACH_ASM_GENERIC_BUG_H
#define _LKL_MACH_ASM_GENERIC_BUG_H

#include "../../../../../include/asm-generic/bug.h"


#ifdef CONFIG_BUG
# define __sectionD(S) __attribute__ ((__section__(("__DATA," #S))))

#undef WARN_ON_ONCE
#define WARN_ON_ONCE(condition)	({				\
	static bool __sectionD(.data.once) __warned;	\
	int __ret_warn_once = !!(condition);			\
								\
	if (unlikely(__ret_warn_once && !__warned)) {		\
		__warned = true;				\
		WARN_ON(1);					\
	}							\
	unlikely(__ret_warn_once);				\
})

#undef WARN_ONCE
#define WARN_ONCE(condition, format...)	({			\
	static bool __sectionD(.data.once) __warned;		\
	int __ret_warn_once = !!(condition);			\
								\
	if (unlikely(__ret_warn_once && !__warned)) {		\
		__warned = true;				\
		WARN(1, format);				\
	}							\
	unlikely(__ret_warn_once);				\
})

#undef WARN_TAINT_ONCE
#define WARN_TAINT_ONCE(condition, taint, format...)	({	\
	static bool __sectionD(.data.once) __warned;	\
	int __ret_warn_once = !!(condition);			\
								\
	if (unlikely(__ret_warn_once && !__warned)) {		\
		__warned = true;				\
		WARN_TAINT(1, taint, format);			\
	}							\
	unlikely(__ret_warn_once);				\
})

#endif

#endif
