# A good idea whenever kernel modules are added or changed:
if [ -x sbin/depmod ]; then
  /sbin/depmod -a 1> /dev/null 2> /dev/null
fi

