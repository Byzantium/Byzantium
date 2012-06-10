#!/usr/bin/env python

import _services
from _utils import *

conf = config()

def main():
	service_entry = file2str('tmpl/services_entry.tmpl')
	page = file2str('tmpl/services_page.tmpl')
	services_list = _services.get_services_list()
	if len(services_list) < 1:
		page = page % {'service-list':conf.no_services_msg}
	else:
		services_html = ''
		for entry in services_list:
			services_html += service_entry % entry
		page = page % {'service-list':services_html}
	return page

if __name__ == '__main__':
	print('Content-type: text/html\n\n')
	print(main())
