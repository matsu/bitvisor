#include <core/arith.h>
#include <core/time.h>
#include "ip_sys.h"

unsigned int
ip_sys_now (void)
{
	u64 tmp[2];

	tmp[0] = get_time ();
	tmp[1] = 0;
	mpudiv_128_32 (tmp, 1000, tmp);
	return tmp[0];
}
