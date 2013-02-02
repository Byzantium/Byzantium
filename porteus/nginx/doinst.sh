config() {
  NEW="$1"
  OLD="$(dirname $NEW)/$(basename $NEW .new)"
  # If there's no config file by that name, mv it over:
  if [ ! -r $OLD ]; then
    mv $NEW $OLD
  elif [ "$(cat $OLD | md5sum)" = "$(cat $NEW | md5sum)" ]; then
    # toss the redundant copy
    rm $NEW
  fi
  # Otherwise, we leave the .new copy for the admin to consider...
}

preserve_perms() {
  NEW="$1"
  OLD="$(dirname ${NEW})/$(basename ${NEW} .new)"
  if [ -e ${OLD} ]; then
    cp -a ${OLD} ${NEW}.incoming
    cat ${NEW} > ${NEW}.incoming
    mv ${NEW}.incoming ${NEW}
  fi
  config ${NEW}
}

preserve_perms etc/rc.d/rc.nginx.new
config etc/logrotate.d/nginx.new
config etc/nginx/fastcgi_params.new 
config etc/nginx/fastcgi.conf.new 
config etc/nginx/mime.types.new 
config etc/nginx/nginx.conf.new 
config etc/nginx/koi-utf.new 
config etc/nginx/koi-win.new 
config etc/nginx/scgi_params.new
config etc/nginx/uwsgi_params.new
config etc/nginx/win-utf.new

