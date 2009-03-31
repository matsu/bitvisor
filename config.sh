#!/bin/sh
set --
LANG=C
while read line; do
    name="${line%%#*}"
    value="${name#*=}"
    name="${name%%=*}"
    name="${name#CONFIG_}"
    help="${line#*#}"
    set -- "$@" "$name" "$help" "$value"
done < .config
exec 3>&1
ret="`whiptail --checklist \"VMM Configuration\" 22 75 16 -- \"$@\" 2>&1 1>&3 3>&-`" || exit 1
exec 3>&-
set -- $ret
mv .config .config.bak || exit 1
while read line; do
    name="${line%%#*}"
    value="${name#*=}"
    name="${name%%=*}"
    name="${name#CONFIG_}"
    help="${line#*#}"
    value=0
    for i; do
	case "$i" in \""$name"\")
	    value=1
	esac
    done
    echo "CONFIG_$name=$value#$help"
done < .config.bak > .config
