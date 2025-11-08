#ifndef _MACHINE_ENDIAN_H_
#define _MACHINE_ENDIAN_H_

#include <sys/types.h>

/*
 * Definitions for byte order,
 * according to byte significance from low address to high.
 */
#define	LITTLE_ENDIAN	1234	/* least-significant byte first (x86) */
#define	BIG_ENDIAN	4321	/* most-significant byte first (IBM, net) */
#define	PDP_ENDIAN	3412	/* LSB first in word, MSW first in long (pdp) */

#define	BYTE_ORDER	LITTLE_ENDIAN	/* byte order on x86_64/amd64 */

/*
 * Macros for network/external number representation conversion.
 * x86_64 is little-endian, so we need to swap bytes for network order.
 */

#if defined(__GNUC__) || defined(__clang__)

#define	__bswap16(x)	__builtin_bswap16(x)
#define	__bswap32(x)	__builtin_bswap32(x)
#define	__bswap64(x)	__builtin_bswap64(x)

#else

static inline uint16_t
__bswap16(uint16_t x)
{
	return ((x << 8) | (x >> 8));
}

static inline uint32_t
__bswap32(uint32_t x)
{
	return ((x << 24) | ((x << 8) & 0x00ff0000) |
	        ((x >> 8) & 0x0000ff00) | (x >> 24));
}

static inline uint64_t
__bswap64(uint64_t x)
{
	return ((x << 56) | ((x << 40) & 0x00ff000000000000ULL) |
	        ((x << 24) & 0x0000ff0000000000ULL) |
	        ((x << 8)  & 0x000000ff00000000ULL) |
	        ((x >> 8)  & 0x00000000ff000000ULL) |
	        ((x >> 24) & 0x0000000000ff0000ULL) |
	        ((x >> 40) & 0x000000000000ff00ULL) |
	        (x >> 56));
}

#endif /* __GNUC__ || __clang__ */

#define	ntohl(x)	__bswap32(x)
#define	ntohs(x)	__bswap16(x)
#define	htonl(x)	__bswap32(x)
#define	htons(x)	__bswap16(x)

#define	ntohll(x)	__bswap64(x)
#define	htonll(x)	__bswap64(x)

#define	NTOHL(x)	((x) = ntohl((uint32_t)(x)))
#define	NTOHS(x)	((x) = ntohs((uint16_t)(x)))
#define	HTONL(x)	((x) = htonl((uint32_t)(x)))
#define	HTONS(x)	((x) = htons((uint16_t)(x)))

/*
 * Host byte order to little/big endian conversions
 */
#define	htole16(x)	((uint16_t)(x))
#define	htole32(x)	((uint32_t)(x))
#define	htole64(x)	((uint64_t)(x))
#define	le16toh(x)	((uint16_t)(x))
#define	le32toh(x)	((uint32_t)(x))
#define	le64toh(x)	((uint64_t)(x))

#define	htobe16(x)	__bswap16(x)
#define	htobe32(x)	__bswap32(x)
#define	htobe64(x)	__bswap64(x)
#define	be16toh(x)	__bswap16(x)
#define	be32toh(x)	__bswap32(x)
#define	be64toh(x)	__bswap64(x)

#endif /* !_MACHINE_ENDIAN_H_ */