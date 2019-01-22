#ifndef _MACH_LKL_LINUX_MODULE_H
#define _MACH_LKL_LINUX_MODULE_H

#include "../../../../../include/linux/module.h"

#if defined(MODULE) || !defined(CONFIG_SYSFS)
#else
#undef MODULE_VERSION
#define MODULE_VERSION(_version)					\
	static struct module_version_attribute ___modver_attr = {	\
		.mattr	= {						\
			.attr	= {					\
				.name	= "version",			\
				.mode	= S_IRUGO,			\
			},						\
			.show	= __modver_version_show,		\
		},							\
		.module_name	= KBUILD_MODNAME,			\
		.version	= _version,				\
	};								\
	static const struct module_version_attribute			\
	__used __attribute__ ((__section__ ("__DATA,__modver")))		\
	* __moduleparam_const __modver_attr = &___modver_attr
#endif

#endif /* _MACH_LKL_LINUX_MODULE_H */
