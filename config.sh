#!/bin/sh
set --
LANG=C
while read line; do
    name="${line%%#*}"
    value="${name#*=}"
    name="${name%%=*}"
    name="${name#CONFIG_}"
    help="${line#*#}"
    case "$value" in
	1)
	    value=on
	    ;;
	*)
	    value=off
	    ;;
    esac
    set -- "$@" "$name" "$help" "$value"
done < .config
which whiptail && DIALOG=whiptail || DIALOG=dialog
exec 3>&1
ret="`$DIALOG --checklist \"VMM Configuration\" 0 78 0 -- \"$@\" 2>&1 1>&3 3>&-`" || exit 1
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
