#!/usr/bin/env python

# http://minidns.googlecode.com/hg/minidns

import sys
import socket
import fcntl
import struct

# DNSQuery class from http://code.activestate.com/recipes/491264-mini-fake-dns-server/
class DNSQuery:
  def __init__(self, data):
    self.data=data
    self.domain=''

    tipo = (ord(data[2]) >> 3) & 15   # Opcode bits
    if tipo == 0:                     # Standard query
      ini=12
      lon=ord(data[ini])
      while lon != 0:
        self.domain+=data[ini+1:ini+lon+1]+'.'
        ini+=lon+1
        lon=ord(data[ini])

  def respuesta(self, ip):
    packet=''
    if self.domain:
      packet+=self.data[:2] + "\x81\x80"
      packet+=self.data[4:6] + self.data[4:6] + '\x00\x00\x00\x00'   # Questions and Answers Counts
      packet+=self.data[12:]                                         # Original Domain Name Question
      packet+='\xc0\x0c'                                             # Pointer to domain name
      packet+='\x00\x01\x00\x01\x00\x00\x00\x3c\x00\x04'             # Response type, ttl and resource data length -> 4 bytes
      packet+=str.join('',map(lambda x: chr(int(x)), ip.split('.'))) # 4bytes of IP
    return packet

# get_ip_address code from http://code.activestate.com/recipes/439094-get-the-ip-address-associated-with-a-network-inter/
def get_ip_address(ifname):
  s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

  try:
    return socket.inet_ntoa(fcntl.ioctl(
        s.fileno(),
        0x8915,  # SIOCGIFADDR
        struct.pack('256s', ifname[:15])
    )[20:24])
  except:
    return None

def usage():
  print "Usage:"
  print "\t# minidns [ip | interface]"
  print ""
  print "Description:"
  print "\tMiniDNS will respond to all DNS queries with a single IPv4 address."
  print "\tYou may specify the IP address to be returned as the first argument"
  print "\ton the command line:\n"
  print "\t\t# minidns 1.2.3.4\n"
  print "\tAlternatively, you may specify an interface name and MiniDNS will"
  print "\tuse the IP address currently assigned to that interface:\n"
  print "\t\t# minidns eth0\n"
  print "\tIf no interface or IP address is specified, the IP address of eth0"
  print "\twill be used."

  sys.exit(1)

if __name__ == '__main__':
  ip = None
  iface = 'eth0'

  if len(sys.argv) == 2:
    if sys.argv[-1] == '-h' or sys.argv[-1] == '--help':
      usage()
    else:
      if len(sys.argv[-1].split('.')) == 4:
        ip=sys.argv[-1]
      else:
        iface = sys.argv[-1]

  if ip is None:
    ip = get_ip_address(iface)

  if ip is None:
    print "ERROR: Invalid IP address or interface name specified!"
    usage()

  try:
    udps = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udps.bind(('',31339))  # Listen on 31339 instead of 53 (for captive guests)
  except Exception, e:
    print "Failed to create socket on UDP port 31339:", e
    sys.exit(1)

  print 'miniDNS :: * 60 IN A %s\n' % ip
  
  try:
    while 1:
      data, addr = udps.recvfrom(1024)
      p=DNSQuery(data)
      udps.sendto(p.respuesta(ip), addr)
      print 'Request: %s -> %s' % (p.domain, ip)
  except KeyboardInterrupt:
    print '\nBye!'
    udps.close()

