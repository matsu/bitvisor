/**
 * @file	drivers/ieee1394.c
 * @brief	generic IEEE1394 para pass-through driver based on ehci.c
 * @author	K. Matsubara, H. Eiraku
 */
#include <core.h>
#include "pci.h"

static const char driver_name[] = "ieee1394_generic_driver";
static const char driver_longname[] = 
	"Generic IEEE1394 para pass-through driver 0.1";
static bool ieee1394_disable;

static void 
ieee1394_new(struct pci_device *pci_device)
{
	printf("An IEEE1394 host controller found. Disable it.\n");
	return;
}

static int 
ieee1394_config_read(struct pci_device *pci_device, 
		 core_io_t io, u8 offset, union mem *data)
{
	if (!ieee1394_disable)
		return CORE_IO_RET_DEFAULT;
	/* provide fake values 
	   for reading the PCI configration space. */
	data->dword = 0UL;
	return CORE_IO_RET_DONE;
}

static int 
ieee1394_config_write(struct pci_device *pci_device, 
		  core_io_t io, u8 offset, union mem *data)
{
	if (!ieee1394_disable)
		return CORE_IO_RET_DEFAULT;
	/* do nothing, ignore any writing. */
	return CORE_IO_RET_DONE;
}

static struct pci_driver ieee1394_driver = {
	.name		= driver_name,
	.longname	= driver_longname,
	.id		= { PCI_ID_ANY, PCI_ID_ANY_MASK },
	.class		= { 0x0C0010, 0xFFFFFF },
	.new		= ieee1394_new,	
	.config_read	= ieee1394_config_read,
	.config_write	= ieee1394_config_write,
};

static void
ieee1394_init_boot (void)
{
	ieee1394_disable = true;
}

/**
 * @brief	driver init function automatically called at boot time
 */
void 
ieee1394_init(void) __initcode__
{
#if defined(IEEE1394_CONCEALER)
	ieee1394_disable = true;
	pci_register_driver(&ieee1394_driver);
#endif
#if defined (FWDBG)
	ieee1394_disable = false;
#endif
	return;
}
PCI_DRIVER_INIT(ieee1394_init);
INITFUNC ("bootdat0", ieee1394_init_boot);
