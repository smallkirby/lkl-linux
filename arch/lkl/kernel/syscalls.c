#include <linux/stat.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/jhash.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/net.h>
#include <linux/task_work.h>
#include <linux/syscalls.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <asm/host_ops.h>
#include <asm/syscalls.h>
#include <asm/syscalls_32.h>
#include <asm/cpu.h>
#include <asm/sched.h>

static asmlinkage long sys_virtio_mmio_device_add(long base, long size,
						  unsigned int irq);

typedef long (*syscall_handler_t)(long arg1, ...);

#undef __SYSCALL
#define __SYSCALL(nr, sym) [nr] = (syscall_handler_t)sym,

syscall_handler_t syscall_table[__NR_syscalls] = {
	[0 ... __NR_syscalls - 1] =  (syscall_handler_t)sys_ni_syscall,
#include <asm/unistd.h>

#if __BITS_PER_LONG == 32
#include <asm/unistd_32.h>
#endif
};

static long run_syscall(long no, long *params)
{
	long ret;

	if (no < 0 || no >= __NR_syscalls)
		return -ENOSYS;

	ret = syscall_table[no](params[0], params[1], params[2], params[3],
				params[4], params[5]);

	task_work_run();

#ifdef RUMPUSER
	/* XXX: maybe need lkl_ops->thread_yield() ? */
	void __sched_schedule(void);
	__sched_schedule();
#endif

	return ret;
}


#define CLONE_FLAGS (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_THREAD |	\
		     CLONE_SIGHAND | SIGCHLD)

static int host_task_id;
static struct task_struct *host0;

static int new_host_task(struct task_struct **task)
{
	pid_t pid;

	switch_to_host_task(host0);

	pid = kernel_thread(host_task_stub, NULL, CLONE_FLAGS);
	if (pid < 0)
		return pid;

	rcu_read_lock();
	*task = find_task_by_pid_ns(pid, &init_pid_ns);
	rcu_read_unlock();

	host_task_id++;

	snprintf((*task)->comm, sizeof((*task)->comm), "host%d", host_task_id);

	return 0;
}
static void exit_task(void)
{
	do_exit(0);
}

static void del_host_task(void *arg)
{
	struct task_struct *task = (struct task_struct *)arg;
	struct thread_info *ti = task_thread_info(task);

	if (lkl_cpu_get() < 0)
		return;

	switch_to_host_task(task);
	host_task_id--;
	set_ti_thread_flag(ti, TIF_SCHED_JB);
	lkl_ops->jmp_buf_set(&ti->sched_jb, exit_task);
}

static struct lkl_tls_key *task_key;

int inline lkl_save_register(struct task_struct *task)
{
#ifdef __x86_64
	/* XXX: proper way ? */
	/* alt: register unsigned long rbp asm("rbp"); rbp + 16 */
	task_pt_regs(task)->regs.sp =
		(unsigned long)(__builtin_frame_address(0) + 16);
	task_pt_regs(task)->regs.bp =
		(unsigned long)__builtin_frame_address(1);
	task_pt_regs(task)->regs.ip =
		(unsigned long)__builtin_return_address(0);

#define SAVE_REG(r)						\
	asm("mov %%"#r ",%0" :: "m"(task_pt_regs(task)->regs.r));

	SAVE_REG(r15)
	SAVE_REG(r14)
	SAVE_REG(r13)
	SAVE_REG(r12)
	SAVE_REG(bx);

	return 0;
#else
	return -ENOSYS;
#endif /* __x86_64 */
}

void lkl_set_host_task(struct task_struct *p)
{
	if (lkl_ops->tls_get) {
		if (lkl_ops->tls_get(task_key)) {
			pr_err("host task already there");
		}
		lkl_ops->tls_set(task_key, p);
	}
}


long (*lkl_parent_syscall)(long no, long *params);

long lkl_syscall(long no, long *params)
{
	struct task_struct *task = host0;
	long ret;

	/* use parent proc's lkl_syscall instead */
	if (lkl_parent_syscall)
		return lkl_parent_syscall(no, params);

	if (!lkl_ops)
		return -ENOSYS;

	ret = lkl_cpu_get();
	if (ret < 0)
		return ret;

	if (lkl_ops->tls_get) {
		task = lkl_ops->tls_get(task_key);
		if (!task) {
			ret = new_host_task(&task);
			if (ret)
				goto out;
			lkl_ops->tls_set(task_key, task);
		}
	}

	switch_to_host_task(task);

	/* rentrant syscall */
	if (task_thread_info(task)->rump.count)
		lkl_cpu_put();

	task_thread_info(task)->rump.count++;

	if (no == __NR_vfork) {
		ret = lkl_save_register(task);
		if (ret < 0)
			return ret;
	}

#ifdef RUMPUSER
	void rump_platform_exit(void);
	if (no == __NR_exit || no == __NR_exit_group)
		rump_platform_exit();
#endif

	ret = run_syscall(no, params);

	/* rentrant syscall */
	if (task_thread_info(task)->rump.count >= 2)
		lkl_cpu_get();
	task_thread_info(task)->rump.count--;

	if (no == __NR_reboot) {
		thread_sched_jb();
		return ret;
	}

out:
	lkl_cpu_put();

	return ret;
}

static struct task_struct *idle_host_task;

/* called from idle, don't failed, don't block */
void wakeup_idle_host_task(void)
{
	if (!need_resched() && idle_host_task)
		wake_up_process(idle_host_task);
}

static int idle_host_task_loop(void *unused)
{
	struct thread_info *ti = task_thread_info(current);

	snprintf(current->comm, sizeof(current->comm), "idle_host_task");
	set_thread_flag(TIF_HOST_THREAD);
	idle_host_task = current;

	for (;;) {
		lkl_cpu_put();
		lkl_ops->sem_down(ti->sched_sem);
		if (idle_host_task == NULL) {
			lkl_ops->thread_exit();
			return 0;
		}
		schedule_tail(ti->prev_sched);
	}
}

void *lkl_sysproxy_fork(void *priv)
{
	struct task_struct *task;
	struct thread_info *ti;

	new_host_task(&task);
	switch_to_host_task(task);
	ti = task_thread_info(task);

	ti = task_thread_info(get_current());
	/* store struct spc_client */
	ti->rump.client = priv;

	return task;
}

void *lkl_sysproxy_newlwp(pid_t pid)
{
	/* find rump_task */
	struct thread_info *ti = NULL;
	struct task_struct *task;

	if (pid <= 0)
		return NULL;

	rcu_read_lock();
	task = find_task_by_pid_ns(pid, &init_pid_ns);
	rcu_read_unlock();

	if (pid == 1)
		return task;

	lkl_cpu_get();
	switch_to_host_task(task);
	lkl_cpu_put();

	ti = task_thread_info(task);
	ti->tid = lkl_ops->thread_self();

	lkl_ops->tls_set(task_key, ti->task);

	return ti->task;
}

void lkl_sysproxy_lwpexit(void *task)
{
#ifdef FIXME
	del_host_task(task);
#endif
}

pid_t lkl_sysproxy_getpid(void *task)
{
	return pid_vnr(get_task_pid(task, PIDTYPE_PID));
}

pid_t lkl_sysproxy_enter(void)
{
	struct thread_info *ti = task_thread_info(get_current());

	ti->rump.remote = true;

	return ti->task->pid;
}

void lkl_sysproxy_exit(pid_t pid)
{
	struct task_struct *task;
	struct thread_info *ti;

	rcu_read_lock();
	task = find_task_by_pid_ns(pid, &init_pid_ns);
	rcu_read_unlock();

	ti = task_thread_info(task);
	ti->rump.remote = false;
}


int syscalls_init(void)
{
	snprintf(current->comm, sizeof(current->comm), "host0");
	set_thread_flag(TIF_HOST_THREAD);
	host0 = current;

	if (lkl_ops->tls_alloc) {
		task_key = lkl_ops->tls_alloc(del_host_task);
		if (!task_key)
			return -1;
	}

	if (kernel_thread(idle_host_task_loop, NULL, CLONE_FLAGS) < 0) {
		if (lkl_ops->tls_free)
			lkl_ops->tls_free(task_key);
		return -1;
	}

	return 0;
}

void syscalls_cleanup(void)
{
	if (idle_host_task) {
		struct thread_info *ti = task_thread_info(idle_host_task);

		idle_host_task = NULL;
		lkl_ops->sem_up(ti->sched_sem);
		lkl_ops->thread_join(ti->tid);
	}

	if (lkl_ops->tls_free)
		lkl_ops->tls_free(task_key);
}

SYSCALL_DEFINE3(virtio_mmio_device_add, long, base, long, size, unsigned int,
		irq)
{
	struct platform_device *pdev;
	int ret;

	struct resource res[] = {
		[0] = {
		       .start = base,
		       .end = base + size - 1,
		       .flags = IORESOURCE_MEM,
		       },
		[1] = {
		       .start = irq,
		       .end = irq,
		       .flags = IORESOURCE_IRQ,
		       },
	};

	pdev = platform_device_alloc("virtio-mmio", PLATFORM_DEVID_AUTO);
	if (!pdev) {
		dev_err(&pdev->dev, "%s: Unable to device alloc for virtio-mmio\n", __func__);
		return -ENOMEM;
	}

	ret = platform_device_add_resources(pdev, res, ARRAY_SIZE(res));
	if (ret) {
		dev_err(&pdev->dev, "%s: Unable to add resources for %s%d\n", __func__, pdev->name, pdev->id);
		goto exit_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: Unable to add %s%d\n", __func__, pdev->name, pdev->id);
		goto exit_release_pdev;
	}

	return pdev->id;

exit_release_pdev:
	platform_device_del(pdev);
exit_device_put:
	platform_device_put(pdev);

	return ret;
}
