#ifndef ECHOCTL_COMMON_H
#define ECHOCTL_COMMON_H

enum protocol {
	PURE_TCP,
};

int echoctl_main (char *ctl_type, int proto);

#endif /* ECHOCTL_COMMON_H */
