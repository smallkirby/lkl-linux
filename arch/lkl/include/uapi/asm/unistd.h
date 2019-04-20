#include <linux/types.h>

#define __ARCH_WANT_SYSCALL_NO_AT
#define __ARCH_WANT_SYSCALL_DEPRECATED
#define __ARCH_WANT_SYSCALL_NO_FLAGS
#define __ARCH_WANT_RENAMEAT
#define __ARCH_WANT_SYS_VFORK

#if __BITS_PER_LONG == 64
#define __ARCH_WANT_SYS_NEWFSTATAT
#endif

#undef __NR_clock_gettime
#include <asm-generic/unistd.h>

#define __NR_virtio_mmio_device_add		(__NR_arch_specific_syscall + 0)
#define __NR_vfork				(__NR_arch_specific_syscall + 1)

/* XXX: busybox uses syscall(2) with x86_64 syscall number so,
 * temporary use this */
#undef __NR_clock_gettime
#define __NR_clock_gettime			228
__SC_COMP(__NR_clock_gettime, sys_clock_gettime, compat_sys_clock_gettime)
