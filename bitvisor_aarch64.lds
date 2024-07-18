ENTRY(entry)
virt = 0; /* To make relocation simple */
phys = 0x00100000;
entry_max = 512 * 1024; /* Relocation table can be large */
PHDRS {
	all PT_LOAD;
}
SECTIONS {
	. = virt;
	head = .;
	. = virt + SIZEOF_HEADERS;
	.got : AT (phys + (_got - head)) {
		_got = .;
		*(.got)
		*(.got.*)
	}
	.rela.dyn : AT (phys + (_rela_start - head)) {
		_rela_start = .;
		*(.rela)
		*(.rela.*)
		_rela_end = .;
	}
	.entry.data : AT (phys + (entry_data - head)) {
		entry_data = .;
		*(.entry.data)
		*(.entry.data.*)
	}
	. = ALIGN (4096);
	.entry : AT (phys + (code - head)) {
		code = .;
		*(.entry)
		*(.entry.text)
		*(.entry.text.*)
	} :all
	_entry_size = . - head;
	ASSERT (_entry_size <= entry_max, "Error: .entry is larger than 512KB")
	.text : AT (phys + (text - head)) {
		text = .;
		*(.text)
		*(.text.*)
		codeend = .;
	}
	. = ALIGN (4096);
	.data : AT (phys + (data - head)) {
		data = .;
		*(.data)
		*(.data.*)
		. = ALIGN (8);
		__initfunc_start = .;
		KEEP (*(.initfunc))
		__initfunc_end = .;
		. = ALIGN (8);
		__process_builtin = .;
		KEEP (*(.processes))
		__process_builtin_end = .;
		dataend = .;
	}
	. = ALIGN (8);
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
