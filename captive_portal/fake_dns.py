#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim: set expandtab tabstop=4 shiftwidth=4 :

# http://minidns.googlecode.com/hg/minidns
# Found by: Haxwithaxe

# Import Python modules.
import sys
import socket
import fcntl
import struct

# DNSQuery class from http://code.activestate.com/recipes/491264-mini-fake-dns-server/
class DNSQuery:
  # 'data' is the actual DNS resolution request from the client.
  def __init__(self, data):
    self.data=data
    self.domain=''

    tipo = (ord(data[2]) >> 3) & 15   # Opcode bits

    # Determine if the client is making a standard resolution request.
    # Otherwise, don't do anything because it's not a resolution request.
    if tipo == 0:
      ini=12
      lon=ord(data[ini])
      while lon != 0:
        self.domain+=data[ini+1:ini+lon+1]+'.'
        ini+=lon+1
        lon=ord(data[ini])

  # Build a reply packet for the client.
  def respuesta(self, ip):
    packet=''
    if self.domain:
      packet+=self.data[:2] + "\x81\x80"

      # Question and answer counts.
      packet+=self.data[4:6] + self.data[4:6] + '\x00\x00\x00\x00'

      # A copy of the original resolution query from the client.
      packet+=self.data[12:]

      # Pointer to the domain name.
      packet+='\xc0\x0c'

      # Response type, TTL of the reply, and length of data in reply.
      packet+='\x00\x01'  # TYPE: A record
      packet+='\x00\x01'  # CLASS: IN (Internet)
      packet+='\x00\x00\x00\x0f'  # TTL: 15 sec
      packet+='\x00\x04'  # Length of data: 4 bytes

      # The IP address of the server the DNS is running on.
      packet+=str.join('',map(lambda x: chr(int(x)), ip.split('.')))
    return packet

# get_ip_address code from http://code.activestate.com/recipes/439094-get-the-ip-address-associated-with-a-network-inter/
# Method that acquires the IP address of a network interface on the system
# this daemon is running on.  It will only be invoked if an IP address is not
# passed on the command line to the daemon.
def get_ip_address(ifname):
  # > LOOK
  # You are in a maze of twisty passages, all alike.
  # > GO WEST
  s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
  try:
    # It is dark here.  You are likely to be eaten by a grue.
    # > _
    return socket.inet_ntoa(fcntl.ioctl(
        s.fileno(),
        0x8915,  # SIOCGIFADDR
        struct.pack('256s', ifname[:15])
    )[20:24])
  except:
    return None

# Display usage information to the user.
def usage():
  print "Usage:"
  print "\t# minidns [ip | interface]\n"
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

# Core code.
if __name__ == '__main__':
  # Set defaults for basic operation.  Hopefully these will be overridden on
  # the command line.
  ip = None
  iface = 'eth0'

  # Parse the argument vector.
  if len(sys.argv) == 2:
    if sys.argv[-1] == '-h' or sys.argv[-1] == '--help':
      usage()
    else:
      if len(sys.argv[-1].split('.')) == 4:
        ip=sys.argv[-1]
      else:
        iface = sys.argv[-1]

  # In the event that an interface name was given but not an IP address, get
  # the IP address.
  if ip is None:
    ip = get_ip_address(iface)

  # If the IP address can't be gotten somehow, carp.
  if ip is None:
    print "ERROR: Invalid IP address or interface name specified!"
    usage()

  # Open a socket to listen on.  Haxwithaxe set this to port 31339/udp because
  # this is the DNS hijacker bit of the captive portal.  Only clients that
  # aren't in the whitelist will see it.
  try:
    udps = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udps.bind(('',31339))
  except Exception, e:
    print "Failed to create socket on UDP port 31339:", e
    sys.exit(1)

  # Print something for anyone watching a TTY.  All 'A' records this daemon
  # serves up have a TTL of 15 seconds.
  print 'miniDNS :: * 15 IN A %s\n' % ip

  # The do-stuff loop.
  try:
    while 1:
      # Receive a DNS resolution request from a client.
      data, addr = udps.recvfrom(1024)

      # Generate the response.
      p=DNSQuery(data)

      # Send the response to the client.
      udps.sendto(p.respuesta(ip), addr)
      print 'Request: %s -> %s' % (p.domain, ip)
  except KeyboardInterrupt:
    print '\nBye!'
    udps.close()

# Fin.
