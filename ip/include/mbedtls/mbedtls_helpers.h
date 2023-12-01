/*
 * Copyright (c) 2023 Igel Co., Ltd
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
 * 3. Neither the name of the copyright holder nor the names of its
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

#ifndef __IP_INCLUDE_MBEDTLS_HELPERS_H
#define __IP_INCLUDE_MBEDTLS_HELPERS_H

#include <core/config.h>
#include "lwip/altcp_tls.h"

#define ECHO_TLS_CA_CRT	    config.tls.ca_cert
#define ECHO_TLS_SERVER_CRT config.tls.srv_cert
#define ECHO_TLS_SERVER_KEY config.tls.srv_key
/* Define descriptive constants for the error codes */
#define PEM_MISSING_BEGIN		-1
#define PEM_MISSING_END			-2
#define PEM_INVALID_STRUCTURE		-3
#define PEM_OUTPUT_BUFFER_TOO_SMALL	-4

struct altcp_tls_config *altcp_tls_create_config_server_privkey_cert_cacert (
	const u8_t *privkey, size_t privkey_len, const u8_t *privkey_pass,
	size_t privkey_pass_len, const u8_t *cert, size_t cert_len,
	const u8_t *cacert, size_t cacert_len);
int convert_pem_to_der (const unsigned char *input, size_t ilen,
			unsigned char *output, size_t *olen);
int mbedtls_rand (void *rng_state, unsigned char *output, size_t len);
int mbedtls_f_source (void *data, unsigned char *output, size_t len,
		      size_t *olen);
void debug_print_func (void *context, int level, const char *fname,
		       int line_number, const char *msg);

#endif /* __IP_INCLUDE_MBEDTLS_HELPERS_H */
