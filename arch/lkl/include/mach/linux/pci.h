#ifndef MACH_LKL_LINUX_PCI_H
#define MACH_LKL_LINUX_PCI_H

#include "../../../../../include/linux/pci.h"

#undef DECLARE_PCI_FIXUP_SECTION
#define DECLARE_PCI_FIXUP_SECTION(section, name, vendor, device, class,	\
				  class_shift, hook)			\
	static const struct pci_fixup __PASTE(__pci_fixup_##name,__LINE__) __used	\
	__attribute__((__section__("__DATA,"#section), aligned((sizeof(void *)))))    \
		= { vendor, device, class, class_shift, hook };

#endif /* MACH_LKL_LINUX_PCI_H */
