
# Handle the incoming configuration files:
config() {
  for infile in \$1; do
    NEW="\$infile"
    OLD="\`dirname \$NEW\`/\`basename \$NEW .new\`"
    # If there's no config file by that name, mv it over:
    if [ ! -r \$OLD ]; then
      mv \$NEW \$OLD
    elif [ "\`cat \$OLD | md5sum\`" = "\`cat \$NEW | md5sum\`" ]; then
      # toss the redundant copy
      rm \$NEW
    fi
    # Otherwise, we leave the .new copy for the admin to consider...
  done
}

config etc/rc.d/rc.ifplugd.new
config etc/ifplugd/ifplugd.action.new
config etc/ifplugd/ifplugd.conf.new

append_to_rc_local() {
  # breaks existing rc.local when installed as module.
  cat - >> /etc/rc.local <<-EOM
	if [ -x /etc/rc.d/rc.ifplugd ]; then
	  # Start ifplugd
	  echo "Starting ifplugd:  /etc/rc.d/rc.ifplugd start"
	  /etc/rc.d/rc.ifplugd start
	fi
	EOM
}

# Update rc.local so that the ifplugd daemon will be started on boot
if ! grep "rc.ifplugd" etc/rc.d/rc.local 1>/dev/null 2>&1 ; then
	#append_to_rc_local
	echo "add me to your rc.local!"
fi
