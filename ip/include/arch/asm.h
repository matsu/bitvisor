#ifndef _IP_INCLUDE_ARCH_ASM_H
#define _IP_INCLUDE_ARCH_ASM_H

static inline void
asm_store_barrier (void)
{
	asm volatile ("sfence" : : : "memory");
}

#endif
