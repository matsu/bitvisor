#ifndef _CONFIG_H_
#define _CONFIG_H_

struct config_value {
	enum {
		CONFIG_TYPE_NULL,
		CONFIG_TYPE_INTEGER,
		CONFIG_TYPE_STRING,
		CONFIG_TYPE_BINARY,
		CONFIG_TYPE_ENUM,
	}	type;
	void	*value;
	int	size;
};

u64 config_get_interger(char *key);
char *config_get_string(char *key);
void *config_get_binary(char *key);
void config_free(void *value);

#endif
