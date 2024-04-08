DIR ?= .
V ?= 0
include Makefile.common
include $(CONFIG)

arch-default-$(CONFIG_ARCH_DFLT_X86) = x86
ARCH ?= $(arch-default-1)

.PHONY : all
all : pre-build-all

.PHONY : clean
clean : pre-clean-all

NAME   = bitvisor
FORMAT = $(FORMAT_ARCH)
elf    = $(NAME).elf
map    = $(NAME).map
lds    = $(NAME).lds
target = $(elf)

subdirs-1 += core drivers
subdirs-$(CONFIG_STORAGE) += storage
subdirs-$(CONFIG_STORAGE_IO) += storage_io
subdirs-$(CONFIG_VPN) += vpn
subdirs-$(CONFIG_IDMAN) += idman
subdirs-1 += net
subdirs-$(CONFIG_IP) += ip
asubdirs-$(CONFIG_CRYPTO) += crypto
psubdirs-1 += process

process-depends-$(CONFIG_CRYPTO) += $(dir)crypto/$(outa_p)
process-depends-$(CONFIG_IDMAN) += $(dir)idman/$(outo_p)
process-depends-$(CONFIG_STORAGE) += $(dir)storage/$(outo_p)
process-depends-$(CONFIG_STORAGE_IO) += $(dir)storage_io/$(outo_p)
process-depends-$(CONFIG_VPN) += $(dir)vpn/$(outo_p)

$(dir)$(elf) : $(defouto) $(dir)$(lds)
	$(V-info) LD $(dir)$(elf)
	$(CC) $(LDFLAGS) $(LDFLAGS_ELF) -Wl,--gc-sections -Wl,-T,$(dir)$(lds) \
		-Wl,--cref -Wl,-Map,$(dir)$(map) -o $(dir)$(elf) $(defouto)
	$(OBJCOPY) --output-format $(FORMAT) $(dir)$(elf)

.PHONY : pre-build-all
pre-build-all : $(CONFIG) defconfig
	$(MAKE) $(V-makeopt-$(V)) -f Makefile build-all

.PHONY : build-all
build-all :
	case "`$(SED) 1q $(DIR)/$(depends) 2>/dev/null`" in \
	''): >> $(DIR)/$(depends);; esac
	$(MAKE) $(V-makeopt-$(V)) -f Makefile.build build-dir DIR=$(DIR) \
		V=$(V) ARCH=$(ARCH)
	$(SIZE) $(dir)$(elf)

.PHONY : pre-clean-all
pre-clean-all : $(CONFIG) defconfig
	$(MAKE) $(V-makeopt-$(V)) -f Makefile clean-all

.PHONY : clean-all
clean-all :
	$(MAKE) $(V-makeopt-$(V)) -f Makefile.clean clean-dir DIR=$(DIR) \
		V=$(V) ARCH=$(ARCH)
	$(RM) compile_commands.json

.PHONY : config
config : $(CONFIG)
	$(MAKE) -f Makefile.config config

$(CONFIG) : Makefile.config
	: >> $(CONFIG)
	$(MAKE) -f Makefile.config update-config

defconfig :
	cp defconfig.tmpl defconfig

$(dir)process/$(outp_p) : $(process-depends-1)

compile_commands.json :
	bear -- $(MAKE)
	compdb list > $@.tmp
	mv $@.tmp $@
