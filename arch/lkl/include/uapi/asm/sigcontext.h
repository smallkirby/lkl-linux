#ifndef _ASM_UAPI_LKL_SIGCONTEXT_H
#define _ASM_UAPI_LKL_SIGCONTEXT_H

#include <asm/ptrace.h>

/* from arch/x86/include/asm/ptrace.h */
struct lkl_pt_regs {
/*
 * C ABI says these regs are callee-preserved. They aren't saved on kernel entry
 * unless syscall needs a complete, fully filled "struct pt_regs".
 */
	unsigned long r15;
	unsigned long r14;
	unsigned long r13;
	unsigned long r12;
	unsigned long bp;
	unsigned long bx;
/* These regs are callee-clobbered. Always saved on kernel entry. */
	unsigned long r11;
	unsigned long r10;
	unsigned long r9;
	unsigned long r8;
	unsigned long ax;
	unsigned long cx;
	unsigned long dx;
	unsigned long si;
	unsigned long di;
/*
 * On syscall entry, this is syscall#. On CPU exception, this is error code.
 * On hw interrupt, it's IRQ number:
 */
	unsigned long orig_ax;
/* Return frame for iretq */
	unsigned long ip;
	unsigned long cs;
	unsigned long flags;
	unsigned long sp;
	unsigned long ss;
/* top of stack page */
};

struct pt_regs {
	struct lkl_pt_regs regs;
	void *irq_data;
};

struct sigcontext {
	struct pt_regs regs;
	unsigned long oldmask;
};

#endif
