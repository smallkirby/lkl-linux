#ifndef __LKL_MACH_LINUX_CACHE_H
#define __LKL_MACH_LINUX_CACHE_H

#include "../../../../../include/linux/cache.h"

#undef __ro_after_init
#define __ro_after_init __attribute__((__section__("__DATA,.ro_after_init")))

#undef __cacheline_aligned
#define __cacheline_aligned					\
  __attribute__((__aligned__(SMP_CACHE_BYTES),			\
		 __section__("__DATA,.data..cl")))

#endif /* __LKL_MACH_LINUX_CACHE_H */
