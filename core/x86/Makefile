CONSTANTS-$(CONFIG_DEBUG_GDB) += -DDEBUG_GDB
CONSTANTS-$(CONFIG_TTY_SERIAL) += -DTTY_SERIAL
CONSTANTS-$(CONFIG_CPU_MMU_SPT_1) += -DCPU_MMU_SPT_1
CONSTANTS-$(CONFIG_CPU_MMU_SPT_2) += -DCPU_MMU_SPT_2
CONSTANTS-$(CONFIG_CPU_MMU_SPT_3) += -DCPU_MMU_SPT_3
CONSTANTS-$(CONFIG_CPU_MMU_SPT_USE_PAE) += -DCPU_MMU_SPT_USE_PAE
CONSTANTS-$(CONFIG_CPU_MMU_SPT_DISABLE) += -DCPU_MMU_SPT_DISABLE
CONSTANTS-$(CONFIG_PS2KBD_F11PANIC) += -DF11PANIC
CONSTANTS-$(CONFIG_PS2KBD_F12MSG) += -DF12MSG
CONSTANTS-$(CONFIG_STATUS) += -DSTATUS
CONSTANTS-$(CONFIG_PS2KBD_F10USB) += -DF10USBTEST
CONSTANTS-$(CONFIG_PS2KBD_F12USB) += -DF12DUMPEHCI
CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_VTD_TRANS) += -DVTD_TRANS
CONSTANTS-$(CONFIG_DISABLE_TCG_BIOS) += -DDISABLE_TCG_BIOS
CONSTANTS-$(CONFIG_TCG_BIOS) += -DTCG_BIOS
CONSTANTS-$(CONFIG_BACKTRACE) += -DBACKTRACE
CONSTANTS-$(CONFIG_SHIFT_KEY_DEBUG) += -DSHIFT_KEY_DEBUG
CONSTANTS-$(CONFIG_DISABLE_VTD) += -DDISABLE_VTD
CONSTANTS-$(CONFIG_DMAR_PASS_THROUGH) += -DDMAR_PASS_THROUGH
CONSTANTS-$(CONFIG_USE_SYSCALL64) += -DUSE_SYSCALL64
CONSTANTS-$(CONFIG_DEBUG_SERIAL) += -DDEBUG_SERIAL

CFLAGS += -Icore/include
CFLAGS += $(CFLAGS_EDK2_INC)

objs-1 += acpi_dmar.o
objs-1 += acpi_iohook.o
objs-1 += ap.o
objs-1 += arith.o
objs-1 += asm.o
objs-1 += beep.o
objs-1 += cache.o
objs-1 += callrealmode.o
objs-1 += callrealmode_asm.o
objs-1 += calluefi.o
objs-1 += calluefi_asm.o
objs-1 += cpu_emul.o
objs-1 += cpu_interpreter.o
objs-1 += cpu_mmu.o
objs-1 += cpu_mmu_spt.o
objs-1 += cpu_seg.o
objs-1 += cpu_stack.o
objs-1 += cpuid.o
objs-1 += cpuid_pass.o
objs-1 += current.o
objs-1 += currentcpu.o
objs-1 += debug.o
objs-1 += entry.o
objs-1 += exint.o
objs-1 += exint_pass.o
objs-1 += gmm.o
objs-1 += gmm_access.o
objs-1 += gmm_pass.o
objs-1 += guest_bioshook.o
objs-1 += i386-stub.o
objs-1 += initipi.o
objs-1 += initipi_pass.o
objs-1 += int.o
objs-1 += int_handler.o
objs-1 += io_io.o
objs-1 += io_iohook.o
objs-1 += io_iopass.o
objs-1 += keyboard.o
objs-1 += loadbootsector.o
objs-1 += localapic.o
objs-1 += main.o
objs-1 += mm.o
objs-1 += mmio.o
objs-1 += mmioclr.o
objs-1 += msr.o
objs-1 += msr_pass.o
objs-1 += nmi.o
objs-1 += nmi_handler.o
objs-1 += nmi_pass.o
objs-1 += osloader.o
objs-1 += panic.o
objs-1 += pcpu.o
objs-1 += pmap.o
objs-1 += process.o
objs-1 += process_sysenter.o
objs-1 += random.o
objs-1 += reboot.o
objs-1 += savemsr.o
objs-1 += seg.o
objs-1 += serial.o
objs-1 += sleep.o
objs-1 += string.o
objs-1 += svm.o
objs-1 += svm_exitcode.o
objs-1 += svm_init.o
objs-1 += svm_io.o
objs-1 += svm_main.o
objs-1 += svm_msr.o
objs-1 += svm_np.o
objs-1 += svm_paging.o
objs-1 += svm_panic.o
objs-1 += svm_regs.o
objs-1 += sx_handler.o
objs-1 += tcg.o
objs-1 += thread.o
objs-1 += thread_switch.o
objs-1 += time.o
objs-1 += tty.o
objs-1 += uefi.o
objs-1 += vcpu.o
objs-1 += vmm_mem.o
objs-1 += vmmcall.o
objs-1 += vramwrite_pc.o
objs-1 += vt.o
objs-1 += vt_ept.o
objs-1 += vt_exitreason.o
objs-1 += vt_init.o
objs-1 += vt_io.o
objs-1 += vt_main.o
objs-1 += vt_msr.o
objs-1 += vt_paging.o
objs-1 += vt_panic.o
objs-1 += vt_regs.o
objs-1 += vt_shadow_vt.o
objs-1 += wakeup.o
objs-1 += wakeup_entry.o
objs-1 += xsetbv.o
objs-1 += xsetbv_pass.o
