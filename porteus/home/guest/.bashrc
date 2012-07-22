
## this will set kdesu to use sudo instead of su in the event it needs to be set again
kwriteconfig --file kdesurc --group super-user-command --key super-user-command sudo
