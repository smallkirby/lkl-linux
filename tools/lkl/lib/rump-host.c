/*
 * Rump hypercall interface for LKL
 * Copyright (c) 2015 Hajime Tazaki
 *
 * Author: Hajime Tazaki <thehajime@gmail.com>
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>

#include <unistd.h>
#include <poll.h>
//#include <sys/uio.h>

#ifndef RUMPRUN
#include <thread.h>
#endif

#undef container_of
#undef LIST_HEAD
#include <scripts/kconfig/list.h>

#include <lkl_host.h>
#include "iomem.h"
#include "jmp_buf.h"
#include "rump.h"


/* FIXME */
#define BIT(x) (1ULL << x)
#define NSEC_PER_SEC	1000000000L

/* FIXME */
int *__errno(void);
#undef errno
#define errno (*__errno())

/* FIXME */
#ifdef RUMPRUN
struct bmk_thread *bmk_sched_create(const char *, void *, int,
				    void (*)(void *), void *,
				    void *, unsigned long);
#endif

/* for timer thread */
static void *stack;
#define STACKSIZE 65535

/* console */
static void rump_print(const char *str, int len)
{
	while (len-- > 0) {
		rumpuser_putchar(*str);
		str++;
	}
}


/* semaphore/mutex */
struct rumpuser_sem {
	struct rumpuser_mtx *lock;
	int count;
	struct rumpuser_cv *cond;
};

struct lkl_mutex {
	struct rumpuser_mtx *mutex;
};

struct lkl_sem {
	struct rumpuser_sem sem;
};

static struct lkl_sem *rump_sem_alloc(int count)
{
	struct lkl_sem *sem;

	rumpuser_malloc(sizeof(*sem), 0, (void **)&sem);
	if (!sem)
		return NULL;

	rumpuser_mutex_init(&sem->sem.lock, RUMPUSER_MTX_SPIN);
	sem->sem.count = count;
	rumpuser_cv_init(&sem->sem.cond);

	return sem;
}

static void rump_sem_free(struct lkl_sem *_sem)
{
	struct rumpuser_sem *sem = (struct rumpuser_sem *)&_sem->sem;

	rumpuser_cv_destroy(sem->cond);
	rumpuser_mutex_destroy(sem->lock);
	rumpuser_free(sem, 0);
}

static void rump_sem_up(struct lkl_sem *_sem)
{
	struct rumpuser_sem *sem = (struct rumpuser_sem *)&_sem->sem;

	rumpuser_mutex_enter(sem->lock);
	sem->count++;
	if (sem->count > 0)
		rumpuser_cv_signal(sem->cond);
	rumpuser_mutex_exit(sem->lock);
}

static void rump_sem_down(struct lkl_sem *_sem)
{
	struct rumpuser_sem *sem = (struct rumpuser_sem *)&_sem->sem;

	rumpuser_mutex_enter(sem->lock);
	while (sem->count <= 0)
		rumpuser_cv_wait(sem->cond, sem->lock);
	sem->count--;
	rumpuser_mutex_exit(sem->lock);
}

static struct lkl_mutex *rump_mutex_alloc(int recursive)
{
	struct lkl_mutex *_mutex;

	rumpuser_malloc(sizeof(*_mutex), 0, (void **)&_mutex);
	if (!_mutex)
		return NULL;

	rumpuser_mutex_init(&_mutex->mutex, RUMPUSER_MTX_SPIN);

	return _mutex;
}

static void rump_mutex_lock(struct lkl_mutex *_mutex)
{
	rumpuser_mutex_enter(_mutex->mutex);
}

static void rump_mutex_unlock(struct lkl_mutex *_mutex)
{
	rumpuser_mutex_exit(_mutex->mutex);
}

static void rump_mutex_free(struct lkl_mutex *_mutex)
{
	rumpuser_mutex_destroy(_mutex->mutex);
	rumpuser_free(_mutex, 0);
}

/* XXX: dummy TLS */
struct lkl_tls_key {
	void *data;
	void (*destructor)(void *);
};

static struct lkl_tls_key *rump_tls_alloc(void (*destructor)(void *))
{
	struct lkl_tls_key *key;

	rumpuser_malloc(sizeof(struct lkl_tls_key), 0, (void **)&key);
	key->destructor = destructor;
	key->data = NULL;

	return key;
}

static void rump_tls_free(struct lkl_tls_key *key)
{
	rumpuser_free(key, sizeof(struct lkl_tls_key));
}

struct lkl_tls_key *tls_key;

static int rump_tls_set(struct lkl_tls_key *key, void *data)
{
	struct lkl_tls_key *new = rumpuser_getcookie();

	if (!new)
		rumpuser_malloc(sizeof(struct lkl_tls_key), 0, (void **)&new);

	new->destructor = key->destructor;
	new->data = data;
	rumpuser_setcookie(new);

	return 0;
}

static void *rump_tls_get(struct lkl_tls_key *key)
{
	struct lkl_tls_key *new = rumpuser_getcookie();

	return new ? new->data : NULL;
}

/* memory */
static void *rump_mem_alloc(size_t size)
{
	void *mem;

	rumpuser_malloc(size + sizeof(size_t), 0, (void **)&mem);
	memcpy(mem, &size, sizeof(size_t));

	return mem + sizeof(size_t);
}

static void rump_mem_free(void *mem)
{
	size_t size;

	size = *(size_t *)(mem - sizeof(size_t));
	rumpuser_free(mem - sizeof(size_t), size + sizeof(size_t));
}

/* thread */
static lkl_thread_t rump_thread_create(void (*fn)(void *), void *arg)
{
	void *thrid;
	int ret;

	ret = rumpuser_thread_create((void * (*)(void *))fn, arg,
				     "lkl_thr", 1, 1, -1, &thrid);
	if (ret)
		return 0;

	return (lkl_thread_t) thrid;
}

static void rump_thread_detach(void)
{
	/* NOP */
}

static void rump_thread_exit(void)
{
	struct lkl_tls_key *key = rumpuser_getcookie();

	if (key) {
		key->destructor(key->data);
		rumpuser_free(key, sizeof(struct lkl_tls_key));
	}

	rumpuser_thread_exit();
}

static int rump_thread_join(lkl_thread_t tid)
{
	return rumpuser_thread_join((void *)tid);
}

static lkl_thread_t rump_thread_self(void)
{
	return (lkl_thread_t)rumpuser_thread_self();
}

static int rump_thread_equal(lkl_thread_t a, lkl_thread_t b)
{
	return a == b;
}

/* time/timer */
static bool threads_are_go;
static struct rumpuser_mtx *thrmtx;
static struct rumpuser_cv *thrcv;

struct thrdesc {
	struct list_head list;
	void (*f)(void *);
	void *arg;
	int stopped;
	void *thrid;
	struct timespec timeout;
	struct rumpuser_mtx *mtx;
	struct rumpuser_cv *cv;
};


static void rump_timer_stop(void *timer)
{
	struct thrdesc *td = timer;

	if (td->stopped)
		return;

	td->stopped = 1;
	rumpuser_mutex_enter(td->mtx);
	rumpuser_cv_signal(td->cv);
	rumpuser_mutex_exit(td->mtx);
}

/* from src-netbsd/sys/rump/librump/rumpkern/thread.c */
static void rump_thread_allow(struct lwp *l)
{
	rumpuser_mutex_enter(thrmtx);
	if (l == NULL)
		threads_are_go = true;

	rumpuser_cv_broadcast(thrcv);
	rumpuser_mutex_exit(thrmtx);
}

static unsigned long long time_ns(void)
{
	struct timespec ts;

	rumpuser_clock_gettime(RUMPUSER_CLOCK_RELWALL, (int64_t *)&ts.tv_sec,
			       &ts.tv_nsec);

	return ((unsigned long long) ts.tv_sec * NSEC_PER_SEC) + ts.tv_nsec;
}

#ifdef RUMPRUN			/* rumprun */
static LIST_HEAD(pending_timer);

static void *rump_timer_trampoline(void *arg)
{
	struct thrdesc *td = arg;
	int err;

	/* wait for specified timing */
	rumpuser_mutex_enter(td->mtx);
	err = rumpuser_cv_timedwait(td->cv, td->mtx,
				    td->timeout.tv_sec,
				    td->timeout.tv_nsec);
	rumpuser_mutex_exit(td->mtx);
	/* FIXME: we should not use rumpuser__errtrans here
	 * 60==ETIMEDOUT(netbsd), rumpuser__errtrans(ETIMEDOUT))
	 */
	if (err && err != 60) {
		rumpuser_dprintf("timedwait return w/o timedout (%d)\n",
				 err);
		return NULL;
	}

	td->f(td->arg);

	rumpuser_mutex_destroy(td->mtx);
	rumpuser_cv_destroy(td->cv);
	rumpuser_free(td, 0);
	rumpuser_thread_exit();
	return NULL;
}

static void *rump_timer_helper(void *arg)
{
	struct thrdesc *td = arg, *td2, *tmp;

	/* notify the bootstrap done */
	rump_thread_allow(NULL);

	while (!td->stopped) {
		/* wait for an event */
		rumpuser_mutex_enter(td->mtx);
		rumpuser_cv_wait(td->cv, td->mtx);
		rumpuser_mutex_exit(td->mtx);

		if (td->stopped)
			break;

		list_for_each_entry_safe(td2, tmp, &pending_timer, list) {
			td2->thrid = bmk_sched_create(
				"timer", NULL, 0,
				(void (*)(void *))rump_timer_trampoline,
				td2, NULL, 0);
			list_del(&td2->list);
		}
	}

	/* end of timer helper */
	rumpuser_mutex_destroy(td->mtx);
	rumpuser_cv_destroy(td->cv);
	rumpuser_free(td, 0);

	return NULL;
}
#else				/* frankenlibc */
static void *rump_timer_trampoline(void *arg)
{
	struct thrdesc *td = arg;

	td->f(td->arg);
	rumpuser_thread_exit();
	return NULL;
}

static void *rump_timer_helper(void *arg)
{
	struct thrdesc *td = arg, td2;
	int err;

	/* notify the bootstrap done */
	rump_thread_allow(NULL);

	while (!td->stopped) {
		/* wait for an event */
		rumpuser_mutex_enter(td->mtx);
		rumpuser_cv_wait(td->cv, td->mtx);
		rumpuser_mutex_exit(td->mtx);

		if (td->stopped)
			break;

restart:
		/* wait for specified timing */
		rumpuser_mutex_enter(td->mtx);
		err = rumpuser_cv_timedwait(td->cv, td->mtx,
					    td->timeout.tv_sec,
					    td->timeout.tv_nsec);
		rumpuser_mutex_exit(td->mtx);
		/* FIXME: we should not use rumpuser__errtrans here
		 * 60==ETIMEDOUT(netbsd), rumpuser__errtrans(ETIMEDOUT))
		 */
		if (err && err != 60) {
			rumpuser_dprintf("timedwait return w/o timedout (%d)\n",
					 err);
			goto restart;
		}

		td2.f = td->f;
		td2.arg = td->arg;

#ifdef FIXME
		/* use reused stack to avoid mmap(2) in create_thread */
		td2.thrid =
			create_thread("timer", NULL,
				      (void (*)(void *))rump_timer_trampoline,
				      &td2, stack, STACKSIZE, 0);
#else
		/* XXX: not in use
		 * reused stack will avoid mmap(2) in create_thread
		 * and reduce context switch, but can't do now coz
		 * multiple timer.  stack pool ?
		 */
		td2.thrid =
			create_thread("timer", NULL,
				      (void (*)(void *))rump_timer_trampoline,
				      &td2, NULL, 0, 0);
#endif
	}

	/* end of timer helper */
	rumpuser_mutex_destroy(td->mtx);
	rumpuser_cv_destroy(td->cv);
	rumpuser_free(td, 0);

	return NULL;
}
#endif

static void *timer_alloc(void (*fn)(void *), void *arg)
{
	struct thrdesc *td;
	int ret;

	rumpuser_malloc(sizeof(*td), 0, (void **)&td);

	memset(td, 0, sizeof(*td));
	td->f = fn;
	td->arg = arg;

	rumpuser_mutex_init(&td->mtx, RUMPUSER_MTX_SPIN);
	rumpuser_cv_init(&td->cv);

	ret = rumpuser_thread_create(rump_timer_helper, td, "timer_helper",
				     1, 0, -1, &td->thrid);

	/* from src-netbsd/sys/rump/librump/rumpkern/thread.c */
	/* don't allow threads to run before all CPUs have fully attached */
	if (!threads_are_go) {
		rumpuser_mutex_enter_nowrap(thrmtx);
		while (!threads_are_go)
			rumpuser_cv_wait_nowrap(thrcv, thrmtx);
		rumpuser_mutex_exit(thrmtx);
	}

	return ret ? NULL : td;
}

#ifdef RUMPRUN			/* rumprun */
static int timer_set_oneshot(void *_timer, unsigned long ns)
{
	struct thrdesc *td, *master = _timer;

	rumpuser_malloc(sizeof(*td), 0, (void **)&td);
	memcpy(td, _timer, sizeof(*td));
	rumpuser_mutex_init(&td->mtx, RUMPUSER_MTX_SPIN);
	rumpuser_cv_init(&td->cv);

	/* This should override the current issued timer (but not fired)
	 * so that do like POSIX timer_settime(2).
	 */
	td->timeout = (struct timespec){ .tv_sec = ns / NSEC_PER_SEC,
					 .tv_nsec = ns % NSEC_PER_SEC};

	/* notify the helper */
	list_add_tail(&td->list, &pending_timer);

	rumpuser_mutex_enter(master->mtx);
	rumpuser_cv_signal(master->cv);
	rumpuser_mutex_exit(master->mtx);

	return 0;
}
#else  /* frankenlibc */
static int timer_set_oneshot(void *_timer, unsigned long ns)
{
	struct thrdesc *td = _timer;

	/* This should override the current issued timer (but not fired)
	 * so that do like POSIX timer_settime(2).
	 */
	td->timeout = (struct timespec){ .tv_sec = ns / NSEC_PER_SEC,
					 .tv_nsec = ns % NSEC_PER_SEC};

	/* notify the helper */
	rumpuser_mutex_enter(td->mtx);
	rumpuser_cv_signal(td->cv);
	rumpuser_mutex_exit(td->mtx);

	return 0;
}
#endif

static void timer_free(void *_timer)
{
	rump_timer_stop(_timer);
}

static void panic(void)
{
	rumpuser_exit(RUMPUSER_PANIC);
}

struct lkl_host_operations lkl_host_ops = {
	.panic = panic,
	.thread_create = rump_thread_create,
	.thread_detach = rump_thread_detach,
	.thread_exit = rump_thread_exit,
	.thread_join = rump_thread_join,
	.thread_self = rump_thread_self,
	.thread_equal = rump_thread_equal,
	.sem_alloc = rump_sem_alloc,
	.sem_free = rump_sem_free,
	.sem_up = rump_sem_up,
	.sem_down = rump_sem_down,
	.mutex_alloc = rump_mutex_alloc,
	.mutex_free = rump_mutex_free,
	.mutex_lock = rump_mutex_lock,
	.mutex_unlock = rump_mutex_unlock,
	.tls_alloc = rump_tls_alloc,
	.tls_free = rump_tls_free,
	.tls_set = rump_tls_set,
	.tls_get = rump_tls_get,
	.time = time_ns,
	.timer_alloc = timer_alloc,
	.timer_set_oneshot = timer_set_oneshot,
	.timer_free = timer_free,
	.print = rump_print,
	.mem_alloc = rump_mem_alloc,
	.mem_free = rump_mem_free,
	.ioremap = lkl_ioremap,
	.iomem_access = lkl_iomem_access,
	.jmp_buf_set = jmp_buf_set,
	.jmp_buf_longjmp = jmp_buf_longjmp,
#ifdef CONFIG_PCI
	.irq_request = rump_pci_irq_request,
	.irq_release = rump_pci_irq_release,
#endif
	.getparam = (int (*)(const char *, void *, int))rumpuser_getparam,
#ifndef RUMPRUN
	.virtio_devices = lkl_virtio_devs,
#endif
	.sp_copyin = rump_sp_copyin,
	.sp_copyout = rump_sp_copyout,
};

/* stub calls */
#define RUMP_TEMP_STUB
#ifdef RUMP_TEMP_STUB
enum rump_etfs_type {
	RUMP_ETFS_REG,
	RUMP_ETFS_BLK,
	RUMP_ETFS_CHR,
	RUMP_ETFS_DIR,
	RUMP_ETFS_DIR_SUBDIRS
};

int
rump_pub_netconfig_dhcp_ipv4_oneshot(const char *arg1)
{
	/* not supported */
	return -EOPNOTSUPP;
}

int
rump_pub_netconfig_ipv4_ifaddr_cidr(const char *ifname, const char *addr,
				    int mask)
{
	int rv, ifindex;
	unsigned int v4, d1, d2, d3, d4;

	/* XXX: ifname should be like "eth%d", where ifindex will be
	 * started from 2.
	 */
	ifindex = strtoul(ifname + 3, NULL, 10) + 2;

	lkl_if_up(ifindex);

	rv = sscanf(addr, "%u.%u.%u.%u", &d1, &d2, &d3, &d4);
	if (rv != 4) {
		lkl_printf("addr \"%s\" invalid format\n", addr);
		return -1;
	}

	v4 = htonl((d1 << 24) + (d2 << 16) + (d3 << 8) + d4);

	rv = lkl_if_set_ipv4(ifindex, v4, mask);
	if (rv < 0)
		lkl_printf("ifconfig \"%s\" for \"%s/%d\" failed\n",
			   ifname, addr, mask);

	return rv;
}

int
rump_pub_netconfig_ipv4_gw(const char *gw)
{
	int rv;
	unsigned int v4, d1, d2, d3, d4;

	rv = sscanf(gw, "%u.%u.%u.%u", &d1, &d2, &d3, &d4);
	if (rv != 4) {
		lkl_printf("gw \"%s\" invalid format\n", gw);
		return -1;
	}

	v4 = htonl((d1 << 24) + (d2 << 16) + (d3 << 8) + d4);

	rv = lkl_set_ipv4_gateway(v4);
	if (rv != 0)
		lkl_printf("gw \"%s\" addition failed\n", gw);

	return rv;
}

int
rump_pub_netconfig_auto_ipv6(const char *arg1)
{
	/* not supported */
	return -EOPNOTSUPP;
}

int
rump_pub_netconfig_ipv6_ifaddr(const char *arg1, const char *arg2, int arg3)
{
	/* not supported */
	return -EOPNOTSUPP;
}

int
rump_pub_netconfig_ipv6_gw(const char *arg1)
{
	/* not supported */
	return -EOPNOTSUPP;
}

int
rump_pub_netconfig_ifcreate(const char *arg1)
{
	/* not supported */
	return -EOPNOTSUPP;
}

void rump_boot_setsigmodel(int rump_sigmodel)
{
}

int rump___sysimpl_reboot(int opt, char *bootstr)
{

	lkl_umount_all();
	rumpuser_sp_fini(NULL);
	lkl_sys_halt();
	rump_exit();

	/* XXX: should not reach, but... */
	return 0;
}

int rump_pub_etfs_register(const char *key, const char *hostpath,
			   enum rump_etfs_type ftype)
{
//	lkl_printf("warn: rump_pub_etfs_register_register is not supported.\n");
	return 0;
}

int rump_pub_etfs_register_withsize(const char *key, const char *hostpath,
				    enum rump_etfs_type ftype, uint64_t begin,
				    uint64_t size)
{
	panic();
	return 0;
}

int rump___sysimpl_mount50(const char *fstype, const char *path, int perm,
			   void *dum1, size_t dum2)
{
	int ret;

	ret = lkl_sys_mkdir(path, 0700);
	if (ret && ret != -LKL_EEXIST)
		lkl_printf("mount_fs mkdir (rv=%d)\n", ret);

	ret = lkl_sys_mount(NULL, (char *)path, (char *)fstype, 0, NULL);

	return ret;
}

int rump___sysimpl_dup2(int i, int j)
{
	return lkl_sys_dup2(i, j);
}

int rump___sysimpl_socket30(int i, int j, int k)
{
	panic();
	return 0;
}

int rump___sysimpl_unmount(const char *str, int flags)
{
	return lkl_sys_umount((char *)str, flags);
}

void __assert13(const char *file, int line, const char *function,
		const char *failedexpr)
{
	panic();
}

int rump___sysimpl_close(int fd)
{
	return lkl_sys_close(fd);
}

int rump___sysimpl_ioctl(int fd, u_long com, void *data)
{
	panic();
	return -1;
}

int rump___sysimpl_mkdir(const char *path, mode_t mode)
{
	return lkl_sys_mkdir(path, mode);
}

int rump___sysimpl_open(const char *name, int flags, mode_t mode)
{
	return lkl_sys_open(name, flags, mode);
}

#endif /* RUMP_TEMP_STUB */

#ifndef RUMPRUN
static int fd_get_capacity(struct lkl_disk disk, unsigned long long *res)
{
	off_t off;

	off = lseek(disk.fd, 0, SEEK_END);
	if (off < 0)
		return -1;

	*res = off;
	return 0;
}

static int blk_request(struct lkl_disk disk, struct lkl_blk_req *req)
{
	int err = 0;
	struct iovec *iovec = (struct iovec *)req->buf;

	/* TODO: handle short reads/writes */
	switch (req->type) {
	case LKL_DEV_BLK_TYPE_READ:
		err = preadv(disk.fd, iovec, req->count, req->sector * 512);
		break;
	case LKL_DEV_BLK_TYPE_WRITE:
		err = pwritev(disk.fd, iovec, req->count, req->sector * 512);
		break;
	case LKL_DEV_BLK_TYPE_FLUSH:
	case LKL_DEV_BLK_TYPE_FLUSH_OUT:
		err = fsync(disk.fd);
		break;
	default:
		return LKL_DEV_BLK_STATUS_UNSUP;
	}

	if (err < 0)
		return LKL_DEV_BLK_STATUS_IOERR;

	return LKL_DEV_BLK_STATUS_OK;
}

struct lkl_dev_blk_ops lkl_dev_blk_ops = {
	.get_capacity = fd_get_capacity,
	.request = blk_request,
};

static int fs_request(struct lkl_9pfs fs, struct iovec *iov, int cnt)
{
	int err = 0;

	err = write(fs.fd, iov[0].iov_base, iov[0].iov_len);
	if (err < 0) {
		lkl_printf("rumpfd 9pfs: failed to write to fd %d, err=%d\n", fs.fd, errno);
		return LKL_DEV_BLK_STATUS_IOERR;
	}

	err = readv(fs.fd, &iov[1], cnt - 1);
	if (err < 0) {
		lkl_printf("rumpfd 9pfs: failed to read to fd %d\n", fs.fd);
		return LKL_DEV_BLK_STATUS_IOERR;
	}

	return err;
}

struct lkl_dev_9pfs_ops lkl_dev_9pfs_ops = {
	.request = fs_request,
};

struct lkl_netdev_rumpfd {
	struct lkl_netdev dev;
	/* TAP device */
	int fd;
};

static int rump_net_tx(struct lkl_netdev *nd,
		       struct iovec *iov, int cnt)
{
	struct lkl_netdev_rumpfd *nd_rumpfd =
		container_of(nd, struct lkl_netdev_rumpfd, dev);
	int ret;

	do {
		ret = writev(nd_rumpfd->fd, iov, cnt);
	} while (ret == -1 && (errno == EINTR));

	if (ret < 0)
		lkl_perror("write to rump fd netdev fails", errno);

	return ret;
}

static int rump_net_rx(struct lkl_netdev *nd,
		       struct iovec *iov, int cnt)
{
	struct lkl_netdev_rumpfd *nd_rumpfd =
		container_of(nd, struct lkl_netdev_rumpfd, dev);
	int ret;

	do {
		ret = readv(nd_rumpfd->fd, iov, cnt);
	} while (ret == -1 && errno == EINTR);

	if (ret <= 0)
		return -1;

	return ret;
}

static int rump_net_poll(struct lkl_netdev *nd)
{
	struct lkl_netdev_rumpfd *nd_rumpfd =
		container_of(nd, struct lkl_netdev_rumpfd, dev);
	struct pollfd pfd = {
		.fd = nd_rumpfd->fd,
		.events = POLLIN | POLLPRI | POLLOUT
	};
	int ret = 0;


	while (1) {
		int err = poll(&pfd, 1, -1);

		if (err < 0 && errno == EINTR)
			continue;
		if (err > 0)
			break;
	}

	if (pfd.revents & (POLLHUP | POLLNVAL))
		return -1;

	if (pfd.revents & POLLIN)
		ret |= LKL_DEV_NET_POLL_RX;
	if (pfd.revents & POLLOUT)
		ret |= LKL_DEV_NET_POLL_TX;

	return ret;
}

struct lkl_dev_net_ops rumpfd_ops = {
	.tx = rump_net_tx,
	.rx = rump_net_rx,
	.poll = rump_net_poll,
};

struct lkl_netdev_rumpfd *rumpfds[16];

struct lkl_netdev *lkl_netdev_rumpfd_create(const char *ifname, int fd)
{
	struct lkl_netdev_rumpfd *nd;

	nd = (struct lkl_netdev_rumpfd *)
		malloc(sizeof(struct lkl_netdev_rumpfd));
	if (!nd) {
		lkl_printf("rumpfd: failed to register fd %d\n", fd);
		return NULL;
	}

	nd->fd = fd;
	nd->dev.ops = &rumpfd_ops;

	/* register to tmp list */
	int i;
	for (i = 0; i < 16; i++) {
		if (!rumpfds[i]) {
			rumpfds[i] = nd;
			break;
		}
	}
	if (i == 16)
		lkl_printf("rumpfd: can't register fd %d\n", fd);

	return (struct lkl_netdev *)nd;
}

/* ifparams takes a fd number */
struct lkl_netdev *lkl_netdev_rumpfd_lookup(const char *ifname, int offload)
{
	char buf[8];
	char env[32], *fdstr;

	snprintf(env, sizeof(env), "%s%s", "__RUMP_FDINFO_NET_", ifname);
	fdstr = getenv(env);
	if (!fdstr) {
		lkl_printf("FDINFO: %s%s is not founed in environ",
			   "__RUMP_FDINFO_NET_", ifname);
		return NULL;
	}

	int fd = atoi(fdstr), i;

	for (i = 0; i < 16 /* MAX_NET_DEVS */; i++) {
		if (rumpfds[i] && rumpfds[i]->fd == fd) {
			if (offload) {
				if (rumpuser_getparam("LKL_OFFLOAD",
					buf, sizeof(buf)) != 0)
					lkl_printf("WARN: LKL_OFFLOAD should be configured\n");
				rumpfds[i]->dev.has_vnet_hdr = 1;
			}

			return (struct lkl_netdev *)rumpfds[i];
		}
	}

	return NULL;
}

#endif

/* entry/exit points */
char *boot_cmdline;
static char buf[256];
static int verbose;

int rump_init(void)
{
	if (rumpuser_init(RUMPUSER_VERSION, &hyp) != 0) {
		rumpuser_dprintf("rumpuser init failed\n");
		return -EINVAL;
	}

	rumpuser_mutex_init(&thrmtx, RUMPUSER_MTX_SPIN);
	rumpuser_cv_init(&thrcv);
	threads_are_go = false;

	if (rumpuser_getparam("LKL_BOOT_CMDLINE", buf, sizeof(buf)) == 0)
		boot_cmdline = buf;
	else
		boot_cmdline = "mem=100M virtio-pci.force_legacy=1";

	if (!stack) {
		stack = rump_mem_alloc(STACKSIZE);
		if (!stack) {
			rumpuser_dprintf("rump_mem_alloc failiure\n");
			return -EINVAL;
		}
	}

	lkl_start_kernel(&lkl_host_ops, boot_cmdline);

	if (rumpuser_getparam("RUMP_VERBOSE", buf, sizeof(buf)) == 0) {
		if (*buf != 0)
			verbose = 1;
	}

	if (verbose)
		rumpuser_dprintf("rumpuser started.\n");
	return 0;
}

void rump_exit(void)
{
	if (verbose)
		rumpuser_dprintf("rumpuser finishing.\n");

	if (stack)
		rump_mem_free(stack);

	rumpuser_exit(0);
}
