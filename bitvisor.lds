ENTRY(entry)
virt = 0x40100000;
phys = 0x00100000;
PHDRS {
	all PT_LOAD;
}
SECTIONS {
	. = virt;
	head = .;
	. = virt + SIZEOF_HEADERS;
	.entry : AT (phys + (code - head)) {
		code = .;
		*(.entry)
		*(.entry.text)
		*(.entry.text.*)
		*(.entry.data)
		*(.entry.data.*)
	} :all
	.text : AT (phys + (text - head)) {
		text = .;
		*(.text)
		*(.text.*)
		codeend = .;
	}
	.data : AT (phys + (data - head)) {
		data = .;
		*(.data)
		*(.data.*)
		. = ALIGN (8);
		__initfunc_start = .;
		KEEP (*(.initfunc))
		__initfunc_end = .;
		__process_builtin = .;
		KEEP (*(.processes))
		__process_builtin_end = .;
		dataend = .;
	}
	.bss : AT (phys + (bss - head)) {
		bss = .;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		. = ALIGN (4096);
	}
	end = .;
	/DISCARD/ : {
		*(.gomi)
	}
}
