#!/bin/bash

set -e

cd $1
source ./$1.info
wget $DOWNLOAD
cd ../../pkgs
export OUTPUT=`pwd`
cd -
sh $1.SlackBuild

