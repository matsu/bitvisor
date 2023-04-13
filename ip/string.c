#include <cc.h>
#include <string.h>

void *
memmove (void *dest, const void *src, size_t n)
{
	size_t i;
	char *d = dest;
	const char *s = src;

	if (s < d) {
		for (i = n - 1; i >= 0; i--)
			d[i] = s[i];
	} else {
		for (i = 0; i < n; i++)
			d[i] = s[i];
	}

	return d;
}
