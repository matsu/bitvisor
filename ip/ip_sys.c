#include <core/arith.h>
#include <core/random.h>
#include <core/time.h>
#include "ip_sys.h"

#define RAND_RETRY	20

unsigned int
ip_sys_now (void)
{
	u64 tmp[2];

	tmp[0] = get_time ();
	tmp[1] = 0;
	mpudiv_128_32 (tmp, 1000, tmp);
	return tmp[0];
}

void
epoch_now (long long *second, int *microsecond)
{
	return get_epoch_time (second, microsecond);
}

unsigned int
ip_sys_rand (void)
{
	unsigned int num, success;

	success = random_num_hw (RAND_RETRY, &num);
	if (!success)
		num = random_num_sw ();

	return num;
}
