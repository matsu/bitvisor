.PHONY : all user

all :
	$(MAKE) -C linux
	$(MAKE) -C minios_init
ifeq ($(USER),root)
	$(MAKE) -C initrd
else
	fakeroot $(MAKE) -C initrd
endif

user :
	$(MAKE) -C minios_init
ifeq ($(USER),root)
	$(MAKE) -C initrd
else
	fakeroot $(MAKE) -C initrd
endif
