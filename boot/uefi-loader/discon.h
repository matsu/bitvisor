/*
 * Copyright (c) 2013 Igel Co., Ltd
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Tsukuba nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Partition Driver in 2018 Mac firmware apparently has a bug in its
 * Stop() function that calls UninstallMultipleProtocolInterfaces()
 * with an incorrect interface pointer for BlockIoCryptoProtocol.  On
 * such machines, disconnecting drivers from NVMe devices makes system
 * unstable.  A workaround that fixes the incorrect pointer in the
 * UninstallMultipleProtocolInterfaces() during disconnection is
 * enabled if there are handles that support BlockIoCryptoProtocol. */

static EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES orig_uninst_multi_prot_if;

static void
fix_interface (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab, EFI_HANDLE handle,
	       EFI_GUID *protocol, VOID **interface)
{
	VOID *correct_interface;
	EFI_STATUS status;

	status = systab->BootServices->
		OpenProtocol (handle, protocol, &correct_interface, image,
			      NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR (status))
		return;
	if (*interface != correct_interface) {
		print (systab, L"UninstallMultipleProtocolInterfaces:"
		       " incorrect interface ", (UINT64)*interface);
		print (systab, L" is changed to ",
		       (UINT64)correct_interface);
		*interface = correct_interface;
	}
	systab->BootServices->CloseProtocol (handle, protocol, image, NULL);
}

EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES
uninstall_multiple_protocol_interfaces_workaround_main (VOID **args)
{
	EFI_HANDLE image;
	EFI_SYSTEM_TABLE *systab;
	EFI_HANDLE handle;
	EFI_GUID *protocol;

	image = saved_image;
	systab = saved_systab;
	handle = *args++;
	while (*args) {
		protocol = *args;
		if (protocol->Data1 == BlockIoCryptoProtocol.Data1 &&
		    protocol->Data2 == BlockIoCryptoProtocol.Data2 &&
		    protocol->Data3 == BlockIoCryptoProtocol.Data3 &&
		    protocol->Data4[0] == BlockIoCryptoProtocol.Data4[0] &&
		    protocol->Data4[1] == BlockIoCryptoProtocol.Data4[1] &&
		    protocol->Data4[2] == BlockIoCryptoProtocol.Data4[2] &&
		    protocol->Data4[3] == BlockIoCryptoProtocol.Data4[3] &&
		    protocol->Data4[4] == BlockIoCryptoProtocol.Data4[4] &&
		    protocol->Data4[5] == BlockIoCryptoProtocol.Data4[5] &&
		    protocol->Data4[6] == BlockIoCryptoProtocol.Data4[6] &&
		    protocol->Data4[7] == BlockIoCryptoProtocol.Data4[7])
			fix_interface (image, systab, handle, protocol,
				       args + 1);
		args += 2;
	}
	return orig_uninst_multi_prot_if;
}

EFI_STATUS EFIAPI
uninstall_multiple_protocol_interfaces_workaround (EFI_HANDLE Handle, ...);

/* Calls the C function with a pointer of arguments to allow modifying
 * the variable arguments. */
asm ("uninstall_multiple_protocol_interfaces_workaround: \n"
     " mov %rcx,8*1(%rsp) \n"
     " mov %rdx,8*2(%rsp) \n"
     " mov %r8,8*3(%rsp) \n"
     " mov %r9,8*4(%rsp) \n"
     " lea 8*1(%rsp),%rcx \n"
     " sub $0x28,%rsp \n"	/* +8 for 16-byte alignment */
     " call uninstall_multiple_protocol_interfaces_workaround_main \n"
     " add $0x28,%rsp \n"
     " mov 8*1(%rsp),%rcx \n"
     " mov 8*2(%rsp),%rdx \n"
     " mov 8*3(%rsp),%r8 \n"
     " mov 8*4(%rsp),%r9 \n"
     " jmp *%rax \n");

static EFI_STATUS EFIAPI
disconnect_controller_workaround (EFI_HANDLE ControllerHandle,
				  EFI_HANDLE DriverImageHandle,
				  EFI_HANDLE ChildHandle)
{
	EFI_SYSTEM_TABLE *systab;
	EFI_STATUS ret;

	systab = saved_systab;
	orig_uninst_multi_prot_if =
		systab->BootServices->UninstallMultipleProtocolInterfaces;
	systab->BootServices->UninstallMultipleProtocolInterfaces =
		uninstall_multiple_protocol_interfaces_workaround;
	ret = systab->BootServices->DisconnectController (ControllerHandle,
							  DriverImageHandle,
							  ChildHandle);
	systab->BootServices->UninstallMultipleProtocolInterfaces =
		orig_uninst_multi_prot_if;
	return ret;
}

static void *
get_disconnect_controller (EFI_SYSTEM_TABLE *systab)
{
	EFI_STATUS status;
	UINTN buffer_size = 0;

	status = systab->BootServices->LocateHandle (ByProtocol,
						     &BlockIoCryptoProtocol,
						     NULL, &buffer_size, NULL);
	if (status == EFI_NOT_FOUND)
		return systab->BootServices->DisconnectController;
	return disconnect_controller_workaround;
}
