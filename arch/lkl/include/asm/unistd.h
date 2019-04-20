#include <uapi/asm/unistd.h>

__SYSCALL(__NR_virtio_mmio_device_add, sys_virtio_mmio_device_add)
__SYSCALL(__NR_vfork, sys_vfork)

#define __SC_ASCII(t, a) #t "," #a

#define __ASCII_MAP0(m,...)
#define __ASCII_MAP1(m,t,a) m(t,a)
#define __ASCII_MAP2(m,t,a,...) m(t,a) "," __ASCII_MAP1(m,__VA_ARGS__)
#define __ASCII_MAP3(m,t,a,...) m(t,a) "," __ASCII_MAP2(m,__VA_ARGS__)
#define __ASCII_MAP4(m,t,a,...) m(t,a) "," __ASCII_MAP3(m,__VA_ARGS__)
#define __ASCII_MAP5(m,t,a,...) m(t,a) "," __ASCII_MAP4(m,__VA_ARGS__)
#define __ASCII_MAP6(m,t,a,...) m(t,a) "," __ASCII_MAP5(m,__VA_ARGS__)
#define __ASCII_MAP(n,...) __ASCII_MAP##n(__VA_ARGS__)

#if defined(__MINGW32__)
#define SYSCALL_DEF_SECTION ".syscall_defs,\"n0\""
#define TEXT_SECTION ".text"
#elif defined(__APPLE__)
#define SYSCALL_DEF_SECTION "__TEXT,.syscall_defs"
#define TEXT_SECTION "__TEXT,.text"
#else
#define SYSCALL_DEF_SECTION ".syscall_defs,\"a\""
#define TEXT_SECTION ".text"
#endif


#define __SYSCALL_DEFINE_ARCH(x, name, ...)				\
	asm(".section " SYSCALL_DEF_SECTION "\n"			\
	    ".ascii \"#ifdef __NR" #name "\\n\"\n"			\
	    ".ascii \"SYSCALL_DEFINE" #x "(" #name ","			\
	    __ASCII_MAP(x, __SC_ASCII, __VA_ARGS__) ")\\n\"\n"		\
	    ".ascii \"#endif\\n\"\n"					\
	    ".section " TEXT_SECTION "\n");
