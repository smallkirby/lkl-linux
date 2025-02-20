#ifndef _LKL_LIB_ENDIAN_H
#define _LKL_LIB_ENDIAN_H

#if defined(__FreeBSD__)
#include <sys/endian.h>
#elif defined(__ANDROID__)
#include <sys/endian.h>
#elif __ARM_ARCH == 6
#include <endian.h>
#elif __ARM_ARCH == 7
#include <endian.h>
#elif defined(__ARMEL__)
#include <machine/endian.h>
#define le16toh(x) (x)
#define le32toh(x) (x)
#define le64toh(x) (x)
#define htole16(x) htons(x)
#define htole32(x) htonl(x)
#define htobe16(x) htons(x)
#define htobe32(x) htonl(x)
#define be32toh(x) ntohl(x)
#elif defined(__MINGW32__)
#include <winsock.h>
#define le32toh(x) (x)
#define le16toh(x) (x)
#define htole32(x) (x)
#define htole16(x) (x)
#define le64toh(x) (x)
#define htobe32(x) htonl(x)
#define htobe16(x) htons(x)
#define be32toh(x) ntohl(x)
#define be16toh(x) ntohs(x)
#elif defined(__APPLE__)
#define le32toh(x) (x)
#define le16toh(x) (x)
#define htole32(x) (x)
#define htole16(x) (x)
#define le64toh(x) (x)
#define htobe32(x) htonl(x)
#define htobe16(x) htons(x)
#define be32toh(x) ntohl(x)
#define be16toh(x) ntohs(x)
#else
#include <endian.h>
#endif

#ifndef htonl
#define htonl(x) htobe32(x)
#define htons(x) htobe16(x)
#define ntohl(x) be32toh(x)
#define ntohs(x) be16toh(x)
#endif

#endif /* _LKL_LIB_ENDIAN_H */
