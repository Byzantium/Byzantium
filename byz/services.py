#!/usr/bin/env python

import os

service_list_file = '/etc/byzantium/services.csv'
def file2str(fname,mode='r'):
	fobj = open(fname,mode)
	fstr = fobj.read()
	fobj.close()
	return fstr

def get_services():
	output = ''
	fstr = file2str(service_list_file)
	for srv in fstr.strip().split('\n'):
		port, name, desc = srv.strip().split(',',2)
		output += '<a href="'+os.environ['SERVER_PROTOCOL'].split('/')[0]+'://'+os.environ['SERVER_NAME']+':'+port+'">'+name+'</a> '+desc+'<br/>'
	return output


print('Content-type: text/html\n\n')
print('<html><head><title>Mesh Services</title><head/><body>')
print(get_services())
#print(os.environ.keys())
print('</body></html>')
