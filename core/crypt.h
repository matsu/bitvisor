/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (C) 2007, 2008
 *      National Institute of Information and Communications Technology
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

#ifndef _CORE_CRYPT_H
#define _CORE_CRYPT_H

#include "../crypto/chelp.h"
#include <ve_common.h>

#define	CRYPT_SEND_PACKET_LIFETIME		((UINT64)1000)
#define	CRYPT_MAX_LOG_QUEUE_LINES		32

typedef struct VPN_NIC VPN_NIC;
typedef struct VPN_CTX VPN_CTX;

void crypt_init_crypto_and_vpn();
void crypt_exec_test();
void *crypt_sys_memory_alloc(UINT size);
void *crypt_sys_memory_realloc(void *addr, UINT size);
void crypt_sys_memory_free(void *addr);
void crypt_sys_log(char *type, char *message);
UINT crypt_sys_get_current_cpu_id();
SE_HANDLE crypt_sys_new_lock();
void crypt_sys_lock(SE_HANDLE lock_handle);
void crypt_sys_unlock(SE_HANDLE lock_handle);
void crypt_sys_free_lock(SE_HANDLE lock_handle);
UINT crypt_sys_get_tick_count();
void crypt_timer_callback(void *handle, void *data);
SE_HANDLE crypt_sys_new_timer(SE_SYS_CALLBACK_TIMER *callback, void *param);
void crypt_sys_set_timer(SE_HANDLE timer_handle, UINT interval);
void crypt_sys_free_timer(SE_HANDLE timer_handle);
bool crypt_sys_save_data(char *name, void *data, UINT data_size);
bool crypt_sys_load_data(char *name, void **data, UINT *data_size);
void crypt_sys_free_data(void *data);
bool crypt_sys_rsa_sign(char *key_name, void *data, UINT data_size, void *sign, UINT *sign_buf_size);
void crypt_sys_get_physical_nic_info(SE_HANDLE nic_handle, SE_NICINFO *info);
void crypt_sys_send_physical_nic(SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes);
void crypt_sys_set_physical_nic_recv_callback(SE_HANDLE nic_handle, SE_SYS_CALLBACK_RECV_NIC *callback, void *param);
void crypt_sys_get_virtual_nic_info(SE_HANDLE nic_handle, SE_NICINFO *info);
void crypt_sys_send_virtual_nic(SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes);
void crypt_sys_set_virtual_nic_recv_callback(SE_HANDLE nic_handle, SE_SYS_CALLBACK_RECV_NIC *callback, void *param);
void crypt_nic_recv_packet(VPN_NIC *n, UINT num_packets, void **packets, UINT *packet_sizes);
void crypt_init_vpn();
VPN_NIC *crypt_init_physical_nic(VPN_CTX *ctx);
VPN_NIC *crypt_init_virtual_nic(VPN_CTX *ctx);
void crypt_flush_old_packet_from_queue(SE_QUEUE *q);
void crypt_add_log_queue(char *type, char *message);

void crypt_ve_init();
void crypt_ve_handler();
void crypt_ve_main(UCHAR *in, UCHAR *out);

#endif	// _CORE_CRYPT_H

