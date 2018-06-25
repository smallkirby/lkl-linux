#ifndef _MACH_LKL_LINUX_MODULE_PARAMS_H
#define _MACH_LKL_LINUX_MODULE_PARAMS_H

#include "../../../../../include/linux/moduleparam.h"

#undef __module_param_call
#define __module_param_call(prefix, name, ops, arg, perm, level, flags)	\
	/* Default value instead of permissions? */			\
	static const char __param_str_##name[] = prefix #name;		\
	static struct kernel_param __moduleparam_const __param_##name	\
	__used								\
    __attribute__ ((unused,__section__ ("__DATA,__param"),aligned(sizeof(void *)))) \
	= { __param_str_##name, THIS_MODULE, ops,			\
	    VERIFY_OCTAL_PERMISSIONS(perm), level, flags, { arg } }

#endif /* _MACH_LKL_LINUX_MODULE_PARAMS_H */
