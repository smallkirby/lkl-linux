#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>
#include <asm/host_ops.h>
#include <asm/cpu.h>
#include <asm/sched.h>

void lkl_set_host_task(struct task_struct *p);

static int init_ti(struct thread_info *ti)
{
	ti->sched_sem = lkl_ops->sem_alloc(0);
	if (!ti->sched_sem)
		return -ENOMEM;

	ti->dead = false;
	ti->prev_sched = NULL;
	ti->tid = 0;
	ti->rump.client = NULL;
	ti->rump.remote = false;

	return 0;
}

unsigned long *alloc_thread_stack_node(struct task_struct *task, int node)
{
	struct thread_info *ti;

	ti = kmalloc(sizeof(*ti), GFP_KERNEL);
	if (!ti)
		return NULL;

	if (init_ti(ti)) {
		kfree(ti);
		return NULL;
	}
	ti->task = task;


	return (unsigned long *)ti;
}

/*
 * The only new tasks created are kernel threads that have a predefined starting
 * point thus no stack copy is required.
 */
void setup_thread_stack(struct task_struct *p, struct task_struct *org)
{
	struct thread_info *ti = task_thread_info(p);
	struct thread_info *org_ti = task_thread_info(org);

	ti->flags = org_ti->flags;
	ti->preempt_count = org_ti->preempt_count;
	ti->addr_limit = org_ti->addr_limit;
	memcpy(&ti->regs, &org_ti->regs, sizeof(ti->regs));
}

static void kill_thread(struct thread_info *ti)
{
	if (!test_ti_thread_flag(ti, TIF_HOST_THREAD)) {
		ti->dead = true;
		lkl_ops->sem_up(ti->sched_sem);
		lkl_ops->thread_join(ti->tid);
	}
	lkl_ops->sem_free(ti->sched_sem);

}

void free_thread_stack(struct task_struct *tsk)
{
	struct thread_info *ti = task_thread_info(tsk);

	kill_thread(ti);
	kfree(ti);
}

struct thread_info *_current_thread_info = &init_thread_union.thread_info;

/*
 * schedule() expects the return of this function to be the task that we
 * switched away from. Returning prev is not going to work because we are
 * actually going to return the previous taks that was scheduled before the
 * task we are going to wake up, and not the current task, e.g.:
 *
 * swapper -> init: saved prev on swapper stack is swapper
 * init -> ksoftirqd0: saved prev on init stack is init
 * ksoftirqd0 -> swapper: returned prev is swapper
 */
static struct task_struct *abs_prev = &init_task;

struct task_struct *__switch_to(struct task_struct *prev,
				struct task_struct *next)
{
	struct thread_info *_prev = task_thread_info(prev);
	struct thread_info *_next = task_thread_info(next);
	unsigned long _prev_flags = _prev->flags;
	struct lkl_jmp_buf _prev_jb;

	_current_thread_info = task_thread_info(next);
	_next->prev_sched = prev;
	abs_prev = prev;

	BUG_ON(!_next->tid);
	lkl_cpu_change_owner(_next->tid);

	if (test_bit(TIF_SCHED_JB, &_prev_flags)) {
		/* Atomic. Must be done before wakeup next */
		clear_ti_thread_flag(_prev, TIF_SCHED_JB);
		_prev_jb = _prev->sched_jb;
	}

	lkl_ops->sem_up(_next->sched_sem);
	if (test_bit(TIF_SCHED_JB, &_prev_flags)) {
		lkl_ops->jmp_buf_longjmp(&_prev_jb, 1);
	} else {
		lkl_ops->sem_down(_prev->sched_sem);
	}

	if (_prev->dead)
		lkl_ops->thread_exit();

	return abs_prev;
}

int host_task_stub(void *unused)
{
	return 0;
}

void switch_to_host_task(struct task_struct *task)
{
	if (WARN_ON(!test_tsk_thread_flag(task, TIF_HOST_THREAD)))
		return;

	task_thread_info(task)->tid = lkl_ops->thread_self();

	if (current == task)
		return;

	wake_up_process(task);
	thread_sched_jb();
	lkl_ops->sem_down(task_thread_info(task)->sched_sem);
	schedule_tail(abs_prev);
}

struct thread_bootstrap_arg {
	struct thread_info *ti;
	int (*f)(void *);
	void *arg;
};

void inline lkl_restore_register(struct task_struct *task)
{
#ifdef __x86_64
	void *newrsp;
	unsigned long newrbp, stack_size;

#define RESTORE_REG(r)				\
	asm("mov %0, %%"#r :: "m"(task_pt_regs(task)->regs.r));

	/* XXX: copy & restore sp, need to free... */
#define FORK_STACK_SIZE 1200
	stack_size = STACK_TOP - 8 - task_pt_regs(task)->regs.sp;
	stack_size = stack_size > FORK_STACK_SIZE ? FORK_STACK_SIZE : stack_size;
	pr_info("regs.sp=%lx, ssize=%lu", task_pt_regs(task)->regs.sp, stack_size);
	newrsp = kmalloc(stack_size, GFP_KERNEL);
	memcpy(newrsp, (void *)task_pt_regs(task)->regs.sp, stack_size);
	asm("mov %0, %%rsp" :: "m"(newrsp));

	RESTORE_REG(r15);
	RESTORE_REG(r14);
	RESTORE_REG(r13);
	RESTORE_REG(r12);
//	RESTORE_REG(bx);

	/* rip, temporaly uses r10? */
	asm("mov %0, %%r10" :: "m"(task_pt_regs(task)->regs.ip));

	/* rbp */
	newrbp = (unsigned long)newrsp
		+ (task_pt_regs(task)->regs.bp - task_pt_regs(task)->regs.sp);
	asm("mov %0, %%rbp" :: "m"(newrbp));

	/* case for (v)fork: child should return 0 */
	asm("mov $0, %rax");
	asm("jmp *%r10");
#endif /* __x86_64 */
}

static void thread_bootstrap(void *_tba)
{
	struct thread_bootstrap_arg *tba = (struct thread_bootstrap_arg *)_tba;
	struct thread_info *ti = tba->ti;
	int (*f)(void *) = tba->f;
	void *arg = tba->arg;

	lkl_ops->sem_down(ti->sched_sem);
	kfree(tba);
	if (ti->prev_sched)
		schedule_tail(ti->prev_sched);

	/* kernel thread */
	if (f) {
		f(arg);
	}
	/* fork-ed process */
	else {
		lkl_cpu_put();
		lkl_set_host_task(ti->task);
		lkl_restore_register(ti->task);
	}
	do_exit(0);
}

int copy_thread(unsigned long clone_flags, unsigned long esp,
		unsigned long unused, struct task_struct *p)
{
	struct thread_info *ti = task_thread_info(p);
	struct thread_bootstrap_arg *tba;

	if ((int (*)(void *))esp == host_task_stub) {
		set_ti_thread_flag(ti, TIF_HOST_THREAD);
		return 0;
	}

	tba = kmalloc(sizeof(*tba), GFP_KERNEL);
	if (!tba)
		return -ENOMEM;

	tba->f = (int (*)(void *))esp;
	tba->arg = (void *)unused;
	tba->ti = ti;

	ti->tid = lkl_ops->thread_create(thread_bootstrap, tba);
	if (!ti->tid) {
		kfree(tba);
		return -ENOMEM;
	}

	return 0;
}

void show_stack(struct task_struct *task, unsigned long *esp)
{
}

/**
 * This is called before the kernel initializes, so no kernel calls (including
 * printk) can't be made yet.
 */
void threads_init(void)
{
	int ret;
	struct thread_info *ti = &init_thread_union.thread_info;

	ret = init_ti(ti);
	if (ret < 0)
		lkl_printf("lkl: failed to allocate init schedule semaphore\n");

	ti->tid = lkl_ops->thread_self();
}

void threads_cleanup(void)
{
	struct task_struct *p, *t;

	for_each_process_thread(p, t) {
		struct thread_info *ti = task_thread_info(t);

		if (t->pid != 1 && !test_ti_thread_flag(ti, TIF_HOST_THREAD))
			WARN(!(t->flags & PF_KTHREAD),
			     "non kernel thread task %s\n", t->comm);
		WARN(t->state == TASK_RUNNING,
		     "thread %s still running while halting\n", t->comm);

		kill_thread(ti);
	}

	lkl_ops->sem_free(init_thread_union.thread_info.sched_sem);
}
