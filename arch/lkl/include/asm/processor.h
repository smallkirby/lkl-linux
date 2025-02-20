#ifndef _ASM_LKL_PROCESSOR_H
#define _ASM_LKL_PROCESSOR_H

struct task_struct;

static inline void cpu_relax(void)
{
	unsigned long flags;

	/* since this is usually called in a tight loop waiting for some
	 * external condition (e.g. jiffies) lets run interrupts now to allow
	 * the external condition to propagate */
	local_irq_save(flags);
	local_irq_restore(flags);
}

#define current_text_addr() ({ __label__ _l; _l: &&_l; })

static inline unsigned long thread_saved_pc(struct task_struct *tsk)
{
	return 0;
}

static inline void release_thread(struct task_struct *dead_task)
{
}

static inline void prepare_to_copy(struct task_struct *tsk)
{
}

static inline unsigned long get_wchan(struct task_struct *p)
{
	return 0;
}

static inline void flush_thread(void)
{
}

static inline void trap_init(void)
{
}

static inline void start_thread(struct pt_regs *regs, unsigned long pc,
				unsigned long sp)
{
#ifdef __x86_64
	asm("mov %0, %%rsp" :: "m"(sp));
	asm("mov %0, %%rax" :: "m"(pc));
	asm("jmp *%rax");
#endif
}

struct thread_struct { };

#define INIT_THREAD { }

#define task_pt_regs(tsk) ((struct pt_regs *)&((task_thread_info(tsk))->regs))

/* We don't have strict user/kernel spaces */
#define TASK_SIZE ((unsigned long)-1)
#define TASK_UNMAPPED_BASE	0
/* XXX: need for binfmt_elf */
#define STACK_TOP      0x7ffffffff000 - PAGE_SIZE

#define KSTK_EIP(tsk)	(0)
#define KSTK_ESP(tsk)	(0)

#endif
