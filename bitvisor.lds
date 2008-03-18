ENTRY(entry)
virt = 0x40100000;
phys = 0x00100000;
PHDRS {
	all PT_LOAD AT(phys);
}
SECTIONS {
	.text virt : AT (phys) {
		code = .;
		*(.entry)
		*(.text)
	} :all
	.data : AT (phys + (data - code)) {
		data = .;
		*(.data)
		. = ALIGN (8);
                __initfunc_start = .;
                *(.initfunc)
                __initfunc_end = .;
	}
	.bss : AT (phys + (bss - code)) {
		bss = .;
		*(.bss)
		*(COMMON)
	}
	end = .;
	/DISCARD/ : {
		*(.gomi)
	}
}
