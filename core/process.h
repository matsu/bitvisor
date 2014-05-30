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

#ifndef _CORE_PROCESS_H
#define _CORE_PROCESS_H

#include <core/process.h>
#include "types.h"

#define NUM_OF_PID		32
#define NUM_OF_MSGDSC		32
#define NUM_OF_MSGDSCRECV	8
#define PROCESS_NAMELEN		32
#define MAXNUM_OF_MSGBUF	32

bool own_process64_msrs (void (*func) (void *data), void *data);
void process_kill (bool (*func) (void *data), void *data);
ulong sys_msgsetfunc (ulong ip, ulong sp, ulong num, ulong si, ulong di);
ulong sys_msgregister (ulong ip, ulong sp, ulong num, ulong si, ulong di);
ulong sys_msgopen (ulong ip, ulong sp, ulong num, ulong si, ulong di);
ulong sys_msgclose (ulong ip, ulong sp, ulong num, ulong si, ulong di);
ulong sys_msgsendint (ulong ip, ulong sp, ulong num, ulong si, ulong di);
ulong sys_msgret (ulong ip, ulong sp, ulong num, ulong si, ulong di);
ulong sys_msgsenddesc (ulong ip, ulong sp, ulong num, ulong si, ulong di);
ulong sys_newprocess (ulong ip, ulong sp, ulong num, ulong si, ulong di);
ulong sys_msgsendbuf (ulong ip, ulong sp, ulong num, ulong si, ulong di);
ulong sys_msgunregister (ulong ip, ulong sp, ulong num, ulong si, ulong di);
ulong sys_exitprocess (ulong ip, ulong sp, ulong num, ulong si, ulong di);

#endif
