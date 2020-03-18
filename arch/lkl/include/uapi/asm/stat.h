#ifndef _ASM_UAPI_LKL_STAT_H
#define _ASM_UAPI_LKL_STAT_H

#include <asm/bitsperlong.h>

/* copied from arch/x86/include/uapi/asm/stat.h  */
/* XXX: how to handle other case ? */
#ifdef __x86_64
struct stat {
	unsigned long	st_dev;
	unsigned long	st_ino;
	unsigned long	st_nlink;

	unsigned int		st_mode;
	unsigned int		st_uid;
	unsigned int		st_gid;
	unsigned int		__pad0;
	unsigned long	st_rdev;
	long		st_size;
	long		st_blksize;
	long		st_blocks;	/* Number 512-byte blocks allocated. */

#ifdef _KERNEL
	unsigned long	st_atime;
	unsigned long	st_atime_nsec;
	unsigned long	st_mtime;
	unsigned long	st_mtime_nsec;
	unsigned long	st_ctime;
	unsigned long	st_ctime_nsec;
#else
	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
#endif
	long		__unused[3];
};
#else /* !__x86_64 */
struct stat {
	unsigned long long st_dev;	/* Device.  */
	unsigned long long st_ino;	/* File serial number.  */
	unsigned int	st_mode;	/* File mode.  */
	unsigned int	st_nlink;	/* Link count.  */
	unsigned int	st_uid;		/* User ID of the file's owner.  */
	unsigned int	st_gid;		/* Group ID of the file's group. */
	unsigned long long st_rdev;	/* Device number, if device.  */
	unsigned long long __pad1;
	long long	st_size;	/* Size of file, in bytes.  */
	int		st_blksize;	/* Optimal block size for I/O.  */
	int		__pad2;
	long long	st_blocks;	/* Number 512-byte blocks allocated. */
#ifdef _KERNEL
	int		st_atime;	/* Time of last access.  */
	unsigned int	st_atime_nsec;
	int		st_mtime;	/* Time of last modification.  */
	unsigned int	st_mtime_nsec;
	int		st_ctime;	/* Time of last status change.  */
	unsigned int	st_ctime_nsec;
#else
	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
#endif
	unsigned int	__unused4;
	unsigned int	__unused5;
};
#endif /* !__x86_64 */

/* copied from uapi/asm-generic/stat.h */
/* This matches struct stat64 in glibc2.1. Only used for 32 bit. */
#if __BITS_PER_LONG != 64 || defined(__ARCH_WANT_STAT64)
struct stat64 {
	unsigned long long st_dev;	/* Device.  */
	unsigned long long st_ino;	/* File serial number.  */
	unsigned int	st_mode;	/* File mode.  */
	unsigned int	st_nlink;	/* Link count.  */
	unsigned int	st_uid;		/* User ID of the file's owner.  */
	unsigned int	st_gid;		/* Group ID of the file's group. */
	unsigned long long st_rdev;	/* Device number, if device.  */
	unsigned long long __pad1;
	long long	st_size;	/* Size of file, in bytes.  */
	int		st_blksize;	/* Optimal block size for I/O.  */
	int		__pad2;
	long long	st_blocks;	/* Number 512-byte blocks allocated. */
#ifdef _KERNEL
	int		st_atime;	/* Time of last access.  */
	unsigned int	st_atime_nsec;
	int		st_mtime;	/* Time of last modification.  */
	unsigned int	st_mtime_nsec;
	int		st_ctime;	/* Time of last status change.  */
	unsigned int	st_ctime_nsec;
#else
	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
#endif
	unsigned int	__unused4;
	unsigned int	__unused5;
};
#endif	/* __BITS_PER_LONG != 64 || defined(__ARCH_WANT_STAT64) */

#endif /* _ASM_UAPI_LKL_STAT_H */
