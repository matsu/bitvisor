DIR ?= .
V ?= 0
include Makefile.common
include $(CONFIG)

arch-default-$(CONFIG_ARCH_DFLT_X86) = x86
arch-default-$(CONFIG_ARCH_DFLT_AARCH64) = aarch64
ARCH ?= $(arch-default-y)

.PHONY : all
all : pre-build-all

.PHONY : clean
clean : pre-clean-all

NAME   = bitvisor
FORMAT = $(FORMAT_ARCH)
elf    = $(NAME).elf
map    = $(NAME).map
lds    = $(NAME)_$(ARCH).lds
target = $(elf)

subdirs-y += core drivers
subdirs-$(CONFIG_STORAGE) += storage
subdirs-$(CONFIG_STORAGE_IO) += storage_io
subdirs-$(CONFIG_VPN) += vpn
subdirs-$(CONFIG_IDMAN) += idman
subdirs-y += net
subdirs-$(CONFIG_IP) += ip
subdirs-$(CONFIG_DEVICETREE) += devtree
asubdirs-$(CONFIG_CRYPTO) += crypto
psubdirs-y += process

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
	$(MAKE) $(V-makeopt-$(V)) -f Makefile.config check-old-config V=$(V)
	$(MAKE) $(V-makeopt-$(V)) -f Makefile.config check-empty-config V=$(V)
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
config :
	$(MAKE) $(V-makeopt-$(V)) -f Makefile.config menuconfig V=$(V)

$(CONFIG) : Kconfig
	$(MAKE) $(V-makeopt-$(V)) -f Makefile.config default-config V=$(V)

defconfig :
	cp defconfig.tmpl defconfig

$(dir)process/$(outp_p) : $(process-depends-y)

compile_commands.json :
	bear -- $(MAKE)
	compdb list > $@.tmp
	mv $@.tmp $@
