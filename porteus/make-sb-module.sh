#!/bin/sh
# make-sb-module.sh
# makes a new barebones module tree from a SlackBuilds .tar.gz file

if [ $# -lt 1 ] ; then
    echo "Usage: $0 sbo-tarball.tar.gz"
    exit 1
fi

arg=`basename $1 .tar.gz`
tball="$1"

if [ -d "$arg" -o -e "$arg" ] ; then
    echo "Error: module $arg exists"
    continue
fi
mkdir -p "$arg/src"
cp tor/Makefile "$arg/Makefile"
cp tor/src/Makefile "$arg/src/Makefile"
tar -C "$arg/src" -xvzf "$tball"
. "$arg/src/$arg/$arg.info"
echo "$PRGNAM-$VERSION-i486-1_SBo" > "$arg/$arg.pkglist"
