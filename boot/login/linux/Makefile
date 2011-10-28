VERSION=2.6.31.6

.PHONY : all vmlinux linux-$(VERSION)/vmlinux

all : vmlinux

vmlinux : linux-$(VERSION)/vmlinux
	cp linux-$(VERSION)/vmlinux vmlinux

linux-$(VERSION)/vmlinux : linux-$(VERSION)/.config
	$(MAKE) -C linux-$(VERSION) ARCH=i386

linux-$(VERSION)/.config : linux-$(VERSION).tar.bz2
	cat linux-$(VERSION).tar.bz2 | bzip2 -dc | tar xf -
	cp -i config-$(VERSION) linux-$(VERSION)/.config
	$(MAKE) -C linux-$(VERSION) oldconfig ARCH=i386

linux-$(VERSION).tar.bz2 :
	wget http://maffin.ring.gr.jp/pub/linux/kernel.org/kernel/v2.6/linux-$(VERSION).tar.bz2
