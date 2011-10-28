#!/bin/bash
set -x
set -e

cd $1
source ./$1.info

wget --no-check-certificate -nc $DOWNLOAD
cd ../../pkgs
export OUTPUT=`pwd`
cd -
sh $1.SlackBuild

