# -*- mode: ruby -*-
# vi: set ft=ruby et sw=2 ts=2 :

VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  config.vm.box = "BBE"
  config.vm.box_url = "https://s3.amazonaws.com/project-byzantium/BBEv1.box"
  config.vm.provision :shell, path: "vagrant-bootstrap.sh"
end
