#include <linux/gfp.h>

/* XXX: when initcall3 (arch_initcall) is missing, linker locates
 * the section after initcall4, which makes do_one_initcall() puzzled.
 */
static int dummy(void)
{
	return 0;
}
arch_initcall(dummy);

/* XXX: if can do weak alias then this can be deleted  */
long probe_kernel_read(void *dst, const void *src, size_t size)
{
	return 0;
}
