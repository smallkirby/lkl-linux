#ifndef _MACH_LKL_LINUX__INIT_TASK_H
#define _MACH_LKL_LINUX__INIT_TASK_H

#include "../../../../../include/linux/init_task.h"

#undef __init_task_data
#define __init_task_data __attribute__((__section__("__DATA,.init_task")))

/* Attach to the thread_info data structure for proper alignment */
#undef __init_thread_info
#define __init_thread_info					\
	__attribute__((__section__("__DATA,.data..init_thr")))


#endif
