/*
 * Copyright (c) 2007, 2008 University of Tsukuba
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

#include <core/config.h>
#include <core/printf.h>
#include <core/string.h>

int
IDMan_StReadSetData (char *pMemberName, char *pInfo, unsigned long int *len)
{
	int i;

	if (!strcmp (pMemberName, "CRL01")) {
		memcpy (pInfo, config.idman.crl01, 4096);
		for (i = 0; i < 4096; i++) {
			if (config.idman.crl01[i]) {
				*len = 4096;
				return 0;
			}
		}
		/* return success */
		*len = 0;
		return 0;
	}
	if (!strcmp (pMemberName, "CRL02")) {
		memcpy (pInfo, config.idman.crl02, 4096);
		for (i = 0; i < 4096; i++) {
			if (config.idman.crl02[i]) {
				*len = 4096;
				return 0;
			}
		}
		/* return success */
		*len = 0;
		return 0;
	}
	if (!strcmp (pMemberName, "CRL03")) {
		memcpy (pInfo, config.idman.crl03, 4096);
		for (i = 0; i < 4096; i++) {
			if (config.idman.crl03[i]) {
				*len = 4096;
				return 0;
			}
		}
		/* return success */
		*len = 0;
		return 0;
	}
	if (!strcmp (pMemberName, "TrustAnchorCert01")) {
		for (i = 0; i < 4096; i++) {
			if (config.idman.pkc01[i]) {
				memcpy (pInfo, config.idman.pkc01, 4096);
				*len = 1105;//4096;
				return 0;
			}
		}
		return -1;
	}
	if (!strcmp (pMemberName, "TrustAnchorCert02")) {
		for (i = 0; i < 4096; i++) {
			if (config.idman.pkc02[i]) {
				memcpy (pInfo, config.idman.pkc02, 4096);
				*len = 4096;
				return 0;
			}
		}
		return -1;
	}
	if (!strcmp (pMemberName, "TrustAnchorCert03")) {
		for (i = 0; i < 4096; i++) {
			if (config.idman.pkc03[i]) {
				memcpy (pInfo, config.idman.pkc03, 4096);
				*len = 4096;
				return 0;
			}
		}
		return -1;
	}
	if (!strcmp (pMemberName, "RandomSeedSize")) {
		*len = snprintf (pInfo, 16, "%u", config.idman.randomSeedSize);
		return 0;
	}
	if (!strcmp (pMemberName, "MaxPinLen")) {
		*len = snprintf (pInfo, 16, "%u", config.idman.maxPinLen);
		return 0;
	}
	if (!strcmp (pMemberName, "MinPinLen")) {
		*len = snprintf (pInfo, 16, "%u", config.idman.minPinLen);
		return 0;
	}
	return -1;
}
