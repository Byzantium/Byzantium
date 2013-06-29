#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :
# Copyright (C) 2013 Project Byzantium
# This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.

__license__ = 'GPL v3'

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
    services_list = _services.get_services_list()
    if not services_list:
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
