/*++

Copyright (c) 2004, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

--*/
/*
 * Copyright (c) 2022 Igel Co., Ltd
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

#ifndef _DEVICE_PATH_HELPER_H
#define _DEVICE_PATH_HELPER_H

#include <Protocol/DevicePath.h>

/*
 * EDK2 implements these macros as functions. Importing those C files is
 * tedious. It is easier to port old macros from old EDK instead. They are
 * good enough for our use cases.
 */

#define EFI_END_ENTIRE_DEVICE_PATH            END_DEVICE_PATH_TYPE
#define EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE    END_ENTIRE_DEVICE_PATH_SUBTYPE
#define EFI_END_INSTANCE_DEVICE_PATH          END_INSTANCE_DEVICE_PATH_SUBTYPE
#define EFI_END_DEVICE_PATH_LENGTH            (sizeof (EFI_DEVICE_PATH_PROTOCOL))

#define EfiDevicePathNodeLength(a)            (((a)->Length[0]) | ((a)->Length[1] << 8))
#define EfiNextDevicePathNode(a)              ((EFI_DEVICE_PATH_PROTOCOL *) (((UINT8 *) (a)) + EfiDevicePathNodeLength (a)))

#define EfiDevicePathType(a)                  (((a)->Type) & 0x7f)
#define EfiIsDevicePathEndType(a)             (EfiDevicePathType (a) == 0x7f)

#define EfiIsDevicePathEndSubType(a)          ((a)->SubType == EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE)
#define EfiIsDevicePathEndInstanceSubType(a)  ((a)->SubType == EFI_END_INSTANCE_DEVICE_PATH)

#define EfiIsDevicePathEnd(a)                 (EfiIsDevicePathEndType (a) && EfiIsDevicePathEndSubType (a))
#define EfiIsDevicePathEndInstance(a)         (EfiIsDevicePathEndType (a) && EfiIsDevicePathEndInstanceSubType (a))

#endif
