#include <common.h>



struct config_value *config_get_value(char *key)
{

}

void config_free(void *key)
{

}



#ifdef DEBUG

int main()
{
	printf("%x\n", config_get("vmm.idman.password.algorithm"));
}

#endif
