#ifndef __LKL_MACH_LINUX_COMPILER_TYPES_H
#define __LKL_MACH_LINUX_COMPILER_TYPES_H

#include "../../../../../include/linux/compiler_types.h"

/* Mach-O specific section name */
#define QUOT(x) #x
#define QUOTE(x) QUOT(x)

#undef __section
# define __section(T,S) __attribute__ ((__section__(QUOTE((T,S)))))

# define __sectionT(S) __attribute__ ((__section__(("__TEXT," #S ",regular,pure_instructions"))))
# define __sectionD(S) __attribute__ ((__section__(("__DATA," #S))))

#endif /* __LKL_MACH_LINUX_COMPILER_TYPES_H */
