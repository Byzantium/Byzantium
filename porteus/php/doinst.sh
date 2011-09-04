if [ ! -r etc/nginx/fastcgi.conf ]; then
  cp -a etc/nginx/fastcgi.conf.example etc/nginx/fastcgi.conf
elif [ "`cat etc/nginx/fastcgi.conf 2> /dev/null`" = "" ]; then
  cp -a etc/nginx/fastcgi.conf.example etc/nginx/fastcgi.conf
fi
if [ ! -r etc/php/php.ini ]; then
   cp -a etc/php/php.ini-production etc/php/php.ini
fi
