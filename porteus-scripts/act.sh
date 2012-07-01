#!/bin/bash
# Script by Brokenman
# Script to activate modules in the folder of your choice and sub folders.
# You can prepend all your OFFICE apps with 'office' (e.g office-gcalc.xzm)
# and activate them with <script> <office>
# or you can put all your office apps in a folder called 'office'
# and they will all be activated with <script> <office>

if [ `whoami` != "root" ]; then
    echo "Please enter root's password below"
    mod=`readlink -f $1`
    su - -c "/opt/porteus-scripts/act.sh $mod"
    exit
fi

script=${0##*/}
USER=`whoami`
CONFPATH="/etc/modtools"
CONFIG="$CONFPATH/modtoolsrc"


usage()
{
cat <<EOF

##################
USAGE:
$script <module>
$script <folder>
##################

Just open a terminal and enter the above commands. This
script works independently of any other porteusscripts script.

If the module exists in your chosen module directory then 
module will be activated.

If the folder exists in your optional dir then all modules
within folder will be activated.

The script uses a wildcard.
$script bluetoo = $script bluetooth

In this case any folder called bluetooth will have modules
inside activated, or modules called bluetooth.xzm found
anywhere within the optional folder (or sub folders) will
be activated.

To see this help ... enter $script with nothing following.
##################

EOF
exit
}

modpath()
{
echo
echo "*****************************************************************************"
echo "Please type in the path to your Porteus optional module folder and press enter. "
echo "This is the folder that you keep all of your modules in. You will only need to "
echo "enter this path once and a configuration file will be saved."
echo
read MODFOLDER

if [ ! -d "$MODFOLDER" ]; then
	echo
	echo "You must select a module folder to continue. "
	echo "You will only have to enter this path once"
	exit
		else 
		mkdir -p $CONFPATH
		touch $CONFIG
		echo "[modfolder]" > $CONFIG
		echo $MODFOLDER >> $CONFIG
	fi
}

# Check for config file
if [ -f $CONFIG ]; then
	MODFOLDER=$(grep -A1 "[modfolder]" $CONFIG | tail -1)
		else
		modpath
	MODFOLDER=$(grep -A1 '[modfolder]' $CONFIG | tail -1)
fi

# if nothing is entered after script then show usage
if [ $# -eq 0 ]; then
        usage
        exit 1
fi

# If the input matches a directory then activate all within, otherwise activate specified module only
if [ -d $MODFOLDER/$1* ]; then

for fold in $( find $MODFOLDER/$1* -name "*.*" );
        do activate $fold;
done
exit

else

for mod in $( find $MODFOLDER -name $1*.xzm);
        do activate $mod;
done
exit
fi

# If this point is reached then the input entered did not match
# a module or a folder and the script will exit gracefully.
echo
echo "Nothing found !!"
echo
exit