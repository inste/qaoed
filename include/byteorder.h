#ifdef __APPLE__
# include <machine/endian.h>
#endif 

#ifdef linux
# include <endian.h>
# define BYTE_ORDER __BYTE_ORDER
# define BIG_ENDIAN __BIG_ENDIAN
# define LITTLE_ENDIAN __LITTLE_ENDIAN
#endif


#define __swab16(x) \
             ((uint16_t)( \
	     (((uint16_t)(x) & (uint16_t)0x00ffU) << 8) | \
	     (((uint16_t)(x) & (uint16_t)0xff00U) >> 8) ))

#define __swab32(x) \
             ((uint32_t)( \
	     (((uint32_t)(x) & (uint32_t)0x000000ffUL) << 24) | \
	     (((uint32_t)(x) & (uint32_t)0x0000ff00UL) <<  8) | \
	     (((uint32_t)(x) & (uint32_t)0x00ff0000UL) >>  8) | \
	     (((uint32_t)(x) & (uint32_t)0xff000000UL) >> 24) ))

#define __swab64(x) \
	((uint64_t)( \
	(uint64_t)(((uint64_t)(x) & (uint64_t)0x00000000000000ffULL) << 56) | \
	(uint64_t)(((uint64_t)(x) & (uint64_t)0x000000000000ff00ULL) << 40) | \
	(uint64_t)(((uint64_t)(x) & (uint64_t)0x0000000000ff0000ULL) << 24) | \
	(uint64_t)(((uint64_t)(x) & (uint64_t)0x00000000ff000000ULL) <<  8) | \
	(uint64_t)(((uint64_t)(x) & (uint64_t)0x000000ff00000000ULL) >>  8) | \
	(uint64_t)(((uint64_t)(x) & (uint64_t)0x0000ff0000000000ULL) >> 24) | \
	(uint64_t)(((uint64_t)(x) & (uint64_t)0x00ff000000000000ULL) >> 40) | \
	(uint64_t)(((uint64_t)(x) & (uint64_t)0xff00000000000000ULL) >> 56) ))


#if BYTE_ORDER == BIG_ENDIAN
# define cpu_to_le64(x) __swab64((x))
# define le64_to_cpu(x) __swab64((x))
# define cpu_to_le32(x) __swab32((x))
# define le32_to_cpu(x) __swab32((x))
# define cpu_to_le16(x) __swab16((x))
# define le16_to_cpu(x) __swab16((x))
#endif 

#if BYTE_ORDER == LITTLE_ENDIAN
# define cpu_to_le64(x) (x)
# define le64_to_cpu(x) (x)
# define cpu_to_le32(x) (x)
# define le32_to_cpu(x) (x)
# define cpu_to_le16(x) (x)
# define le16_to_cpu(x) (x)
#endif 
