/*
  Copyright (c) 2010, Gilles BERNARD lordikc at free dot fr
  All rights reserved.
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  * Neither the name of the Author nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Work based on
// rfc1035
// http://code.activestate.com/recipes/491264-mini-fake-dns-server/
// http://www.netfor2.com/dns.htm

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h> 
#include <string.h> 

#define PORT 53
#define MSG_SIZE 512

void usage(const char* prog)
{
  fprintf(stderr,"Usage: %s IP1 IP2 IP3 IP4\n",prog);
  fprintf(stderr,"Example:\n   %s 192 168 1 1\n",prog);
  fprintf(stderr,"Fake DNS returning the same IP address for any DNS request.\n");
}

int main(int argc, char *argv[]) 
{
  struct sockaddr_in addr, server;
  char msg[MSG_SIZE];

  if(argc!=5)
    {
      usage(argv[0]);
      return 1;
    }
  int ip1=atoi(argv[1]);
  int ip2=atoi(argv[2]);
  int ip3=atoi(argv[3]);
  int ip4=atoi(argv[4]);

  if(ip1<0||ip1>255||ip2<0||ip2>255||
     ip3<0||ip3>255||ip4<0||ip4>255) {
      usage(argv[0]);
      return 1;
    }

  // socket creation 
  int sd=socket(AF_INET, SOCK_DGRAM, 0);
  if(sd<0) {
    fprintf(stderr,"%s: cannot open socket \n",argv[0]);
    return 1;
  }

  // bind local server port 
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(PORT);
  int rc = bind (sd, (struct sockaddr *) &server,sizeof(server));
  if(rc<0) {
    fprintf(stderr,"%s: cannot bind port number %d \n", 
	   argv[0], PORT);
    return 1;
  }

  int len = sizeof(addr);
  int flags=0;
  while(1) {
    // receive message 
    int n = recvfrom(sd, msg, MSG_SIZE, flags,
		 (struct sockaddr *) &addr, &len);
      
    if(n<0) {continue;}
      
    // Same Id
    msg[2]=0x81;msg[3]=0x80; // Change Opcode and flags 
    msg[6]=0;msg[7]=1; // One answer
    msg[8]=0;msg[9]=0; // NSCOUNT
    msg[10]=0;msg[11]=0; // ARCOUNT
      
    // Keep request in message and add answer
    msg[n++]=0xC0;msg[n++]=0x0C; // Offset to the domain name
    msg[n++]=0x00;msg[n++]=0x01; // Type 1
    msg[n++]=0x00;msg[n++]=0x01; // Class 1
    msg[n++]=0x00;msg[n++]=0x00;msg[n++]=0x00;msg[n++]=0x3c; // TTL
    msg[n++]=0x00;msg[n++]=0x04; // Size --> 4
    msg[n++]=ip1;msg[n++]=ip2;msg[n++]=ip3;msg[n++]=ip4; // IP
    // Send the answer
    sendto(sd,msg,n,flags,(struct sockaddr *)&addr,len);
  }
  return 0;
}
