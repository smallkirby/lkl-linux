#ifndef _ASM_LKL_ELF_H
#define _ASM_LKL_ELF_H

#include <asm/auxvec.h>

#define elf_check_arch(x) \
	((x)->e_machine == EM_X86_64)

/* XXX: should check e_flags */
//#define elf_check_fdpic(x) 1

#ifdef CONFIG_64BIT
#define ELF_CLASS ELFCLASS64
#else
#define ELF_CLASS ELFCLASS32
#endif

#define ELF_EXEC_PAGESIZE	PAGE_SIZE
/* XXX */
#define ELF_PLATFORM "x86_64"
#define ELF_HWCAP	0
#define ELF_ET_DYN_BASE 0

#define elf_gregset_t long
#define elf_fpregset_t double

long lkl_syscall(long no, long *params);
#define ARCH_DLINFO						\
	NEW_AUX_ENT(AT_PARENT_SYSCALL, (unsigned long)lkl_syscall);

#endif

