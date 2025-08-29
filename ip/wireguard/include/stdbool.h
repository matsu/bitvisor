#ifndef _STDBOOL_H_
#define _STDBOOL_H_

#include <arch/cc.h>

#if __STDC_VERSION__ < 202311L
#	define bool _Bool
#	define false 0
#	define true 1
#endif

#endif
