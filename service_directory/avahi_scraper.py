# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

import select
import json
import sys
import os
import pybonjour
import _utils
import byzantium

conf = _utils.Config()
logging = _utils.get_logging()

SERVICE_TYPE = '__byz__._tcp'   #FIXME put this in the central config before finishing the MVC refit

timeout  = 5
resolved = []

class Avahi_Scraper(model.avahi):
    ''' '''

    def update(self, service):
        index = service.keys()[0]
        logging.debug(service)
        logging.debug('updating state storage')
        super.update(conf.avahi_storage_index_field, index, service)

    def resolve_callback(self ,sd_ref, flags, interface_index, error_code, full_name, host_target, port, txt_record):
        if error_code == pybonjour.kDNSServiceErr_NoError:
            logging.debug('adding record to state storage')
            self.update({'full_name':full_name, 'status': True, 'host':host_target,'port':port,'txt_record':txt_record})
            resolved.append(True)

    def browse_callback(self, sd_ref, flags, interface_index, error_code, service_name, reg_type, reply_domain):
        logging.debug(service_name)
        if errorCode != pybonjour.kDNSServiceErr_NoError:
            return

        if not (flags & pybonjour.kDNSServiceFlagsAdd):
            self.remove(service_name+'.'+reg_type+reply_domain)
            return

        resolve_sd_ref = pybonjour.DNSServiceResolve(0, interface_index, service_name, reg_type, reply_domain, resolve_callback)

        try:
            while not resolved:
                ready = select.select([resolve_sd_ref], [], [], timeout)
                if resolve_sd_ref not in ready[0]:
                    logging.debug('Resolve timed out',1)
                    break
                pybonjour.DNSServiceProcessResult(resolve_sd_ref)
            else:
                resolved.pop()
        finally:
            resolve_sd_ref.close()

    def run(self):
        ''' This runs the Avahi_Scraper and keeps it going.

            This method is the equivalent of __main__ or main() but named as
            run() to make porting to threading easier if needed
        '''

        #the variables "regtype" and "callBack" belong to pybonjour and are exempt
        # from style guide rules
        browse_sd_ref = pybonjour.DNSServiceBrowse(regtype = SERVICE_TYPE,
                                                    callBack = browse_callback)
        try:
            try:
                while True:
                    ready = select.select([browse_sd_ref], [], [])
                    if browse_sd_ref in ready[0]:
                        pybonjour.DNSServiceProcessResult(browse_sd_ref)
            except KeyboardInterrupt:
                pass
        finally:
            browse_sd_ref.close()

if __name__ == '__main__':
    Avahi_Scraper.run()
