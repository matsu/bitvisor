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

// Secure VM Project
// crypto library
// 
// Based on OpenSSL
// 
// By dnobori@cs.tsukuba.ac.jp

#include <openssl/bio.h>

#if BIO_FLAGS_UPLINK==0
/* Shortcut UPLINK calls on most platforms... */
#define	UP_stdin	stdin
#define	UP_stdout	stdout
#define	UP_stderr	stderr
#define	UP_fprintf	fprintf
#define	UP_fgets	fgets
#define	UP_fread	fread
#define	UP_fwrite	fwrite
#undef	UP_fsetmod
#define	UP_feof		feof
#define	UP_fclose	fclose

#define	UP_fopen	fopen
#define	UP_fseek	fseek
#define	UP_ftell	ftell
#define	UP_fflush	fflush
#define	UP_ferror	ferror
#define	UP_fileno	fileno

#define	UP_open		open
#define	UP_read		read
#define	UP_write	write
#define	UP_lseek	lseek
#define	UP_close	close
#endif
