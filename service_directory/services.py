#!/usr/bin/env python

import _services
import _utils

conf = _utils.Config()


def has_internet():
	'''
	determine whether there is an internet connection available with a reasonable amount of certainty.
	return bool True if there is an internet connection False if not
	'''
	# insert magic to determine if there is an internet gateway here
	return False

def main():
	service_entry = _utils.file2str('tmpl/services_entry.tmpl')
	page = _utils.file2str('tmpl/services_page.tmpl')
	internet_connected_html = config.no_internet_msg
	if has_intnernet(): internet_connected_html = config.has_internet_msg
	services_list = _services.get_services_list()
	if len(services_list) < 1:
		page = page % {'service-list':conf.no_services_msg,'internet-connected':internet_connected_html}
	else:
		services_html = ''
		for entry in services_list:
			services_html += service_entry % entry
		page = page % {'service-list':services_html,'internet-connected':internet_connected_html}
	return page

if __name__ == '__main__':
	print('Content-type: text/html\n\n')
	print(main())
