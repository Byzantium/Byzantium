#!/bin/bash
# script by fanthom

# Variables
c='\e[36m'
r='\e[31m'
e=`tput sgr0`
dir="$1/base/000-kernel.xzm"
md5=`which md5sum`
out=/tmp/md5sums.txt$$

# Let's start
if [ "$md5" = "" ]; then
    echo -e "${r}md5sum utility is missing, exiting..."$e
    exit
fi

if [ ! -e "$dir" ]; then
    echo -e "${r}Please enter the full path to where you placed your porteus data folder, for example: /mnt/sdb3/porteus"$e | fmt -w 80
    exit
fi

cd $1
for x in `find ../boot/initrd.xz` `find ../boot/vmlinuz`; do
    md5sum $x >> $out
done

for x in `find ./base/* | sort`; do
    [ -d $x ] || md5sum $x >> $out
done

echo
num=`grep -c / md5sums.txt`; x=1
while [ $x -le $num ]; do
    file=`awk NR==$x md5sums.txt |  cut -d " " -f2-`
    first=`awk NR==$x md5sums.txt |  cut -d " " -f1`
    second=`grep $file $out |  cut -d " " -f1`
    if [ "$first" == "$second" ];then
	echo -e "checking md5sum for $file    ${c}ok"$e
    else
	echo -e "checking md5sum for $file   ${r}fail!"$e
	fail="yes"
    fi
    let x=x+1
done
rm $out

echo
if [ "$fail" ]; then
echo -e "${r}Some md5sums don't match or some files from default ISO are missing, please check above...
Be aware that vmlinuz, initrd.xz, 000-kernel.xzm and 001-core.xzm are the most important, and booting wont be possible when they are missing."$e | fmt -w 80

else
    echo -e "${c}All md5sums are ok, you should have no problems with booting Porteus!"$e
fi

echo -e "${c}Press enter to exit"$e
read
