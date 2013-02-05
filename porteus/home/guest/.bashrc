# add sbin's to path cause it's annoying as hell to not have them there
export PATH=${PATH}:/sbin:/usr/sbin:/usr/local/sbin

## this will set kdesu to use sudo instead of su in the event it needs to be set again
#kwriteconfig --file kdesurc --group super-user-command --key super-user-command sudo
