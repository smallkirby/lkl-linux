#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/major.h>

static void console_write(struct console *con, const char *str, unsigned len)
{
	static char buf[256];
	static int verbose;

	/* when console isn't NULL (not called from file_write() */
	if (con && !verbose) {
		if (!lkl_ops->getparam)
			verbose = 1;
		else if (lkl_ops->getparam("RUMP_VERBOSE", buf, sizeof(buf))
			 == 0)
			if (*buf != 0)
				verbose = 1;
	}

	if (con && !verbose)
		return;

	if (lkl_ops->print)
		lkl_ops->print(str, len);
}

#ifdef CONFIG_LKL_EARLY_CONSOLE
static struct console lkl_boot_console = {
	.name	= "lkl_boot_console",
	.write	= console_write,
	.flags	= CON_PRINTBUFFER | CON_BOOT,
	.index	= -1,
};

int __init lkl_boot_console_init(void)
{
	register_console(&lkl_boot_console);
	return 0;
}
early_initcall(lkl_boot_console_init);
#endif

static struct console lkl_console = {
	.name	= "lkl_console",
	.write	= console_write,
	.flags	= CON_PRINTBUFFER,
	.index	= -1,
};

int __init lkl_console_init(void)
{
	register_console(&lkl_console);
	return 0;
}
core_initcall(lkl_console_init);

#ifdef RUMPUSER
#include <linux/fdtable.h>
struct rumpuser_iovec {
	void *iov_base;
	size_t iov_len;
};
int rumpuser_iovread(int, struct rumpuser_iovec *, size_t, int64_t, size_t *);
int rumpuser_iovwrite(int, const struct rumpuser_iovec *, size_t,
		      int64_t, size_t *);


static int _fp_cmp(const void *v, struct file *fp, unsigned fd)
{
	if (v == fp)
		return fd;
	return 0;
}

static void _lookfor_private_fd(struct file *file)
{
	int fd;

	fd = iterate_fd(current->files, 0, _fp_cmp, file);
	if (!fd)
		pr_warn("no fd found\n");

	if (file != fcheck(fd))
		pr_warn("invalid fp found (fd=%d)\n", fd);

	file->private_data = (uintptr_t)fd;

}

static ssize_t lkl_file_write(struct file *fp, const char __user *buf,
			      size_t size, loff_t *ppos)
{
	int err = 0;
	/* XXX: need to use iovwrite in host_ops (not directly from rump hypercall) */
	struct rumpuser_iovec iov;
	ssize_t ret;

	if (unlikely(fp->private_data == (void *)-1))
		_lookfor_private_fd(fp);

	iov.iov_base = (void *)buf;
	iov.iov_len = size;

	err = rumpuser_iovwrite((int)fp->private_data, &iov, 1, 0, &ret);
	if (err == 0)
		return ret;

	return -err;
}

static ssize_t lkl_file_read(struct file *fp, char __user *buf, size_t size,
			 loff_t *ppos)
{
	int err = 0;
	/* XXX: need to use iovread in host_ops (not directly from rump hypercall) */
	struct rumpuser_iovec iov;
	ssize_t ret;

	if (unlikely(fp->private_data == (void *)-1))
		_lookfor_private_fd(fp);

	iov.iov_base = buf;
	iov.iov_len = size;

	err = rumpuser_iovread((int)fp->private_data, &iov, 1, 0, &ret);
	if (err == 0)
		return ret;

	return -err;
}

static int lkl_file_open(struct inode *inode, struct file *file)
{
	/* initialize */
	file->private_data = (uintptr_t)-1;
	return 0;
}

static const struct file_operations lkl_stdio_fops = {
	.owner		= THIS_MODULE,
	.open =	lkl_file_open,
	.write =	lkl_file_write,
	.read =	lkl_file_read,
};

static int __init lkl_stdio_init(void)
{
	int err;

	/* prepare /dev/console */
	err = register_chrdev(TTYAUX_MAJOR, "console", &lkl_stdio_fops);
	if (err < 0) {
		pr_err("can't register lkl stdio console.\n");
		return err;
	}

	return 0;
}
/* should be _before_ default_rootfs creation (noinitramfs.c) */
fs_initcall(lkl_stdio_init);
#endif /* RUMPUSER */

static int __init lkl_memdev_init(void)
{
	int err;

	/* prepare /dev/null */
	err = sys_mknod((const char __user __force *) "/dev/null",
			S_IFCHR | 0600, new_encode_dev(MKDEV(MEM_MAJOR, 3)));
	if (err < 0) {
		pr_err("can't register /dev/null.\n");
		return err;
	}

	return 0;
}
device_initcall(lkl_memdev_init);
