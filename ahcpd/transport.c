/*
Copyright (c) 2008, 2009 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ahcpd.h"
#include "monotonic.h"
#include "transport.h"

unsigned myseqno;

#define NUMDUPLICATE 64

struct duplicate {
    struct timeval time;
    unsigned char src[8];
    unsigned char dest[8];
    unsigned char nonce[4];
};

struct duplicate duplicate[NUMDUPLICATE];
int next_duplicate = 0;

static int
really_send_packet(struct sockaddr *sin, int sinlen,
                   unsigned char hopcount, unsigned char original_hopcount,
                   const unsigned char *nonce,
                   const unsigned char *src, const unsigned char *dest,
                   const unsigned char *data, size_t datalen)
{
    struct iovec iovec[2];
    struct msghdr msg;
    unsigned char header[24];
    int rc, ret;

    header[0] = 43;
    header[1] = 1;
    header[2] = hopcount;
    header[3] = original_hopcount;
    memcpy(header + 4, nonce, 4);
    memcpy(header + 8, src, 8);
    memcpy(header + 16, dest ? dest : ones, 8);
    iovec[0].iov_base = (void*)header;
    iovec[0].iov_len = sizeof(header);
    iovec[1].iov_base = (void*)data;
    iovec[1].iov_len = datalen;

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iovec;
    msg.msg_iovlen = 2;
    if(sin) {
        msg.msg_name = sin;
        msg.msg_namelen = sinlen;
        rc = sendmsg(protocol_socket, &msg, 0);
        if(rc < 0) {
            int save = errno;
            perror("send");
            errno = save;
            return -1;
        }
        return 1;
    } else {
        int i, saved_errno = EHOSTUNREACH;
        ret = -1;
        for(i = 0; i < numnetworks; i++) {
            struct sockaddr_in6 sin6;
            if(networks[i].ifindex <= 0)
                continue;
            memset(&sin6, 0, sizeof(sin6));
            sin6.sin6_family = AF_INET6;
            memcpy(&sin6.sin6_addr, &protocol_group, 16);
            sin6.sin6_port = htons(protocol_port);
            sin6.sin6_scope_id = networks[i].ifindex;
            msg.msg_name = (struct sockaddr*)&sin6;
            msg.msg_namelen = sizeof(sin6);
            rc = sendmsg(protocol_socket, &msg, 0);
            if(rc < 0) {
                saved_errno = errno;
                perror("send");
                /* but don't reset ret -- only return -1 if all sends failed. */
            } else {
                ret = 1;
            }
            errno = saved_errno;
        }
        return ret;
    }
}

int send_packet(struct sockaddr *sin, int sinlen,
                const unsigned char *dest, int hopcount,
                const unsigned char *buf, size_t bufsize)
{
    unsigned char nonce[4];

    if(hopcount <= 0)
        return 0;

    memcpy(nonce, &myseqno, 4);
    myseqno++;

    return really_send_packet(sin, sinlen,
                              hopcount, hopcount, nonce, myid, dest,
                              buf, bufsize);
}

static int
check_duplicate(const unsigned char *header)
{
    int i;
    struct timeval now;
    gettime(&now, NULL);
    for(i = 0; i < NUMDUPLICATE; i++) {
        if(duplicate[i].time.tv_sec < now.tv_sec - 120)
            continue;
        if(memcmp(header + 4, duplicate[i].nonce, 4) == 0 &&
           memcmp(header + 8, duplicate[i].src, 8) == 0 &&
           memcmp(header + 16, duplicate[i].dest, 8) == 0)
            return 1;
    }
    return 0;
}

static void
record_duplicate(const unsigned char *header)
{
    memcpy(duplicate[next_duplicate].nonce, header + 4, 4);
    memcpy(duplicate[next_duplicate].src, header + 8, 8);
    memcpy(duplicate[next_duplicate].dest, header + 16, 8);
    gettime(&duplicate[next_duplicate].time, NULL);

    next_duplicate = (next_duplicate + 1) % NUMDUPLICATE;
}

/* Take an incoming packet, forward it if necessary, return 2 if it needs
   to be handled by the local node. */
int
handle_packet(int ll, const unsigned char *buf, size_t buflen)
{
    if(buflen < 2) {
        debugf(1, "Received truncated packet.\n");
        return 0;
    }

    if(buf[0] != 43) {
        debugf(1, "Received corrupted packet.\n");
        return 0;
    }

    if(buf[1] != 1) {
        debugf(2, "Received packet with version %d.\n", buf[1]);
        return 0;
    }

    if(buflen < 24) {
        debugf(1, "Received truncated packet.\n");
        return 0;
    }

    if(buf[2] <= 0 || buf[3] <= 0 || buf[2] > buf[3]) {
        debugf(1, "Received packet with zero hop count.\n");
        return 0;
    }

    if(memcmp(buf + 8, zeroes, 8) == 0 || memcmp(buf + 8, ones, 8) == 0) {
        debugf(1, "Received packet with martian source.\n");
        return 0;
    }

    if(memcmp(buf + 16, zeroes, 8) == 0) {
        debugf(1, "Received packet with martian destination.\n");
        return 0;
    }

    /* The following tests do trigger in normal operation. */

    if(memcmp(buf + 8, myid, 8) == 0) {
        debugf(3, "Suppressed packet from self.\n");
        return 0;
    }

    if(check_duplicate(buf)) {
        debugf(3, "Suppressed duplicate.\n");
        return 0;
    }

    record_duplicate(buf);

    if(memcmp(buf + 16, myid, 8) == 0)
        return 2;

    if(buf[2] >= 2) {
        debugf(2, "Forwarding packet, %d/%d hops left.\n",
               buf[2] - 1, buf[3]);

        usleep(random() % 50000);
        really_send_packet(NULL, 0,
                           buf[2] - 1, buf[3],
                           buf + 4, buf + 8, buf + 16,
                           buf + 24, buflen - 24);
    }

    if(memcmp(buf + 16, ones, 8) == 0)
        return 2;
    else
        return 1;
}
