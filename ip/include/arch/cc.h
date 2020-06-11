#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

#define strcmp __builtin_strcmp
#define strlen __builtin_strlen
#define memcmp __builtin_memcmp
#define memcpy __builtin_memcpy
#define memset __builtin_memset
#define strncmp __builtin_strncmp

#define BYTE_ORDER  LITTLE_ENDIAN

#define LWIP_ERR_T  int

#define U16_F "hu"
#define S16_F "hd"
#define X16_F "hx"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"

#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__ ((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define LWIP_PLATFORM_DIAG(x) do {	\
		printf x;		\
	} while (0)

#define LWIP_PLATFORM_ASSERT(x) do {				\
		panic ("Assert \"%s\" failed at line %d in %s",	\
		       x, __LINE__, __FILE__);			\
	} while (0)

#define LWIP_NO_STDINT_H 1
#define LWIP_NO_INTTYPES_H 1

/* --- Debug --- */
#define ETHARP_DEBUG LWIP_DBG_ON
#define DHCP_DEBUG LWIP_DBG_ON
#define TCPIP_DEBUG LWIP_DBG_ON
#define TIMERS_DEBUG LWIP_DBG_ON
#define ETHARP_DEBUG LWIP_DBG_ON
#define AUTOIP_DEBUG LWIP_DBG_OFF
#define LWIP_DBG_TYPES_ON (LWIP_DBG_TRACE|LWIP_DBG_STATE)
#define LWIP_DBG_MIN_LEVEL LWIP_DBG_LEVEL_ALL

/* Optimization */
#define lwip_htons(x) inline_lwip_htons (x)
#define lwip_htonl(x) inline_lwip_htonl (x)

typedef unsigned char u8_t;
typedef char s8_t;
typedef unsigned short u16_t;
typedef short s16_t;
typedef unsigned int u32_t;
typedef int s32_t;

typedef long ptrdiff_t;
typedef unsigned long mem_ptr_t;
typedef unsigned long int size_t;

int printf (const char *format, ...)
	__attribute__ ((format (printf, 1, 2)));
void panic (char *format, ...)
	__attribute__ ((format (printf, 1, 2), noreturn));

static inline u16_t
inline_lwip_htons (u16_t n)
{
	return n << 8 | n >> 8;
}

static inline u32_t
inline_lwip_htonl (u32_t n)
{
	asm ("bswap %0" : "+r" (n));
	return n;
}

#endif /* __ARCH_CC_H__ */
