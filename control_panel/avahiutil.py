# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

import os.path
import _utils

services_store_dir = _utils.Config().services_store_dir
services_live_dir = _utils.Config().services_live_dir


def _mksname(name):
	return name.lower().replace(' ','_')


def add(name,port,host = '',domain = '',stype = '_http._tcp',subtype = None,protocol = None,text = []):
	''' Adds service file to storage dir
		@param	name	service name
		@param	port	integer port number. (udp: 0-65535, tcp: 1-65535)
		@param	host	hostname. multicast or unicast DNS findable host name.
		@param	domain	FQDN eg: myname.local or google.com
		@param	stype	see http://www.dns-sd.org/ServiceTypes.html (default: '_http._ftp')
		@param	subtype	service subtype
		@param	protocol	[any|ipv4|ipv6] (default: any; as per spec in "man avahi.service")
		@param	text	list/tuple of <text-record/> values
		USAGE: add('service name',9007) all other parameters are optional
	'''
	## notes for haxwithaxe
	# See http://www.dns-sd.org/ServiceTypes.html
	# name: <name>1
	# stype: <service>+
	# 	protocol: <service ?protocol="ipv4|ipv6|any">
	# 	domain: <domain-name>?
	# 	host: <host-name>? (explicit FQDN eg: me.local)
	#  subtype: <subtype>?
	#  text: <txt-record>*
	#  port: <port>? (uint)

	service_tmpl = file2str(config().service_template)
	service_path = os.path.join(services_store_dir, _mksname(name)+'.service')
	stext = ''
	for i in text:
		stext += '<text-record>'+i.strip()+'</text-record>'
	service_conf = service_tmpl % {'name':name,'port':port,'host':host,'domain':domain,'stype':stype,'subtype':subtype or '','protocol':protocol or 'any','text':stext}
	_utils.str2file(service_conf, service_path)
	# activate here?

def activate(name):
	service_file = os.path.join(services_store_dir,_mksname(name)+'.service')
	service_link = os.path.join(services_live_dir,_mksname(name)+'.service')
	if service_file != service_link:
		if os.path.exists(service_file):
			if os.path.exists(os.path.split(service_link)[0]):
				try:
					os.symlink(service_file,service_link)
				except Exception as sym_e:
					return {'code':False,'message':repr(sym_e)}
			else:
				return {'code':False,'message':'Directory not found: "%s"' % os.path.split(service_link)[0]}
		else:
				return {'code':False,'message':'Directory not found: "%s"' % service_file}
	reload_avahi_daemon()
	return {'code':True,'message':'Activated'}

def deactivate(name):
	service_file = os.path.join(services_store_dir,_mksname(name)+'.service')
	service_link = os.path.join(services_live_dir,_mksname(name)+'.service')
	if service_file != service_link:
		if os.path.exists(service_link):
			try:
				os.remove(service_link)
			except Exception as rm_sym_e:
				return {'code':False,'message':repr(rm_sym_e)}
		else:
				return {'code':False,'message':'Directory not found: "%s"' % service_link}
		reload_avahi_daemon()
		return {'code':True,'message':'Deactivated'}
	else:
		return {'code':False,'message':'Service file is the same as service link (file:"%s", link:"%s")' % (service_file,service_link)}

def reload_avahi_daemon():
	import subprocess
	cmd = ['/usr/sbin/avahi-daemon','--reload']
	subprocess.call(cmd)
