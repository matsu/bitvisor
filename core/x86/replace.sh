#!/bin/sh
case $# in
0)
	echo 'example:'
	echo '1,$s/foo/bar/'
	echo '1,$s/aaa/bbb/g'
	echo '^D'
	;;
esac
a="`cat \"$@\"`"
c="$a
w
q"
echo 'ed command:'
echo "$c"
echo 'Are you sure?'
read b
case "$b" in
y*|Y*)
	;;
*)
	exit
	;;
esac
for i in *.c *.h; do
	echo "$c" | ed "$i"
done
