#!/bin/bash

# byzantium defaults
server_port=8089
server_address=::1
username=nobody
babel_address=::1
babel_port=33123
update_interval=3000

/opt/babelweb-0.2.4/bin/babelweb "serverPort=${server_port}" "serverAddress=${server_address}" "user=${username}" "babelAddress=${babel_address}" "babelPort=${babel_port}" "updateIval=${update_interval}"
