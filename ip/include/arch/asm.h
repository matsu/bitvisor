#ifndef _IP_INCLUDE_ARCH_ASM_H
#define _IP_INCLUDE_ARCH_ASM_H

#if defined (__i386__) || defined (__x86_64__)

static inline void
asm_store_barrier (void)
{
	asm volatile ("sfence" : : : "memory");
}

#elif defined (__aarch64__)

static inline void
asm_store_barrier (void)
{
	asm volatile ("dsb ish; isb" : : : "memory");
}

#else
#error "Unsupported architecture"
#endif

#endif
