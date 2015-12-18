#!/usr/bin/env bash
: <<.
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
.

printhelp(){
	echo "usage: $0 [-f] device lba1 lba2 loader elf [module1] [module2]"
	echo '  -f      first time (do not check existing data)'
	echo '  device  write to device (ex. /dev/sda)'
	echo "  lba1    install a boot record to device's lba1 (0 for MBR)"
	echo "  lba2    install images to device's lba2"
	echo '  loader  /path/to/bootloader'
	echo '  elf     bitvisor.elf'
	echo '  module1 login program (vmlinux)'
	echo '  module2 login data (initrd)'
}

printbin(){
	cnt=$1
	x=$(($2))
	while test $cnt -gt 0
	do
		a=$(($x%256))
		x=$(($x/256))
		printf \\`printf %o $a`
		cnt=$(($cnt-1))
	done
}

generatembr(){
	dd if="$loader" bs=1 count=4		#   0-   4 bytes
	printbin 4 $1				#   4-   4 bytes
	printbin 8 "$lba2"			#   8-   8 bytes
	printbin 4 $2				#  16-   4 bytes
	printbin 4 $3				#  20-   4 bytes
	dd if="$loader" bs=1 skip=24 count=416	#  24- 416 bytes
	dd if="$device" skip="$lba1" count=1 |	# 440-  70 bytes
		 dd bs=1 skip=440 count=70	#
	dd if="$loader" bs=1 skip=510 count=2	# 510-   2 bytes
}

getbsssize(){
	bsssize=0
	if ! dd if="$elf" bs=1 skip=0 count=4 | od |
		egrep -q '^0000000\s+042577\s+043114'
	then
		echo "ELF header not found in \`$elf'." >&2
		exit 1
	fi
	set -- $(dd if="$elf" bs=1 skip=28 count=4 | od -i)
	phoff=$2
	set -- $(dd if="$elf" bs=1 skip=42 count=2 | od -i)
	phentsize=$2
	set -- $(dd if="$elf" bs=1 skip=44 count=2 | od -i)
	phnum=$2
	while test $phnum -gt 0
	do
		phnum=$(($phnum-1))
		set -- $(($phoff+$phentsize*$phnum))
		set -- $(($1+16)) $(dd if="$elf" bs=1 skip=$1 count=4 | od -i)
		case "$3" in
		1)	;;
		*)	continue;;
		esac
		set -- $(($1+4)) $(dd if="$elf" bs=1 skip=$1 count=4 | od -i)
		set -- "$3" $(dd if="$elf" bs=1 skip=$1 count=4 | od -i)
		bsssize=$(($bsssize+$3-$1))
	done
}

first=0
device=
lba1=
lba2=
loader=
elf=
module1=
module2=
arg=.

while test $# -gt 0
do
	case "$1" in
	-f)	first=1;;
	-*)	echo "unrecognized option \`$1'" >&2
		exit 1;;
	*)	case $arg in
		.)		device="$1";;
		..)		lba1="$1";;
		...)		lba2="$1";;
		....)		loader="$1";;
		.....)		elf="$1";;
		......)		module1="$1";;
		.......)	module2="$1";;
		........)	echo "extra operand \`$1'" >&2
				exit 1;;
		esac
		arg=.$arg;;
	esac
	shift
done

case $arg in
.)	printhelp
	exit 1;;
..)	echo "missing operand after \`$device'" >&2
	exit 1;;
...)	echo "missing operand after \`$lba1'" >&2
	exit 1;;
....)	echo "missing operand after \`$lba2'" >&2
	exit 1;;
.....)	echo "missing operand after \`$loader'" >&2
	exit 1;;
esac

getbsssize

case $first in
0)	if dd if="$device" skip="$lba2" count=1 | od |
		egrep -q '^0000000\s+042577\s+043114'
	then
		echo "found ELF header." >&2
	else
		echo "ELF header not found. use -f option to install." >&2
		exit 1
	fi;;
esac

size0=$((`cat "$elf" | wc -c`+$bsssize))
size1=0
size2=0
off1=0
off2=0
set -- "$elf"
case "$module1" in ?*)
	off1=$size0
	size1=`cat "$module1" | wc -c`
	set -- "$@" "$module1"
esac
case "$module2" in ?*)
	off2=$(($size0 + $size1))
	size2=`cat "$module2" | wc -c`
	set -- "$@" "$module2"
esac
total=$(($size0 + $size1 + $size2))
generatembr $total $off1 $off2 | dd of="$device" seek="$lba1" conv=notrunc
{
	cat "$1"
	shift
	if test $bsssize -ge 512
	then
		dd if=/dev/zero count=$(($bsssize/512))
		bsssize=$(($bsssize%512))
	fi
	if test $bsssize -gt 0
	then
		dd if=/dev/zero bs=1 count=$bsssize
	fi
	if test $# -gt 0
	then
		cat "$@"
	fi
} | dd of="$device" seek="$lba2" conv=notrunc
