#!/usr/bin/env bash

ln -sf /vagrant /root/Byzantium

cat >/root/.profile <<'EOF'
export BUILD_HOME=/root
EOF

ln -sf /root/.profile /root/.bashrc

pushd /root
svn co http://svn.virtadpt.net/byzantium/v0.5b/i386/ byzantium
popd
