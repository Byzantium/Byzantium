#!/bin/bash
set -x
set -e

cd $1
source ./$1.info

wget --no-check-certificate -nc $DOWNLOAD
export OUTPUT="/tmp/pkgs"
mkdir -p $OUTPUT
sh $1.SlackBuild

