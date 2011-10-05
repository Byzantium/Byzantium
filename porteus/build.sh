#!/bin/bash
set -x
set -e

cd $1
source ./$1.info
for i in `cat ./$1.info | cut -d = -f 1` ; do
    export $i
done

wget --no-check-certificate -nc $DOWNLOAD
cd ../../pkgs
export OUTPUT=`pwd`
cd -
sh $1.SlackBuild

