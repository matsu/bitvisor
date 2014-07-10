#include "lwip/sys.h"
#include <ip_sys.h>

u32_t
sys_now (void)
{
	return ip_sys_now ();
}
