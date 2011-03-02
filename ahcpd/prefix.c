/*
Copyright (c) 2007-2010 by Juliusz Chroboczek

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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "prefix.h"

void
free_prefix_list(struct prefix_list *l)
{
    free(l);
}

struct prefix_list *
copy_prefix_list(struct prefix_list *l)
{
    struct prefix_list *c = malloc(sizeof(struct prefix_list));
    if(c == NULL)
        return NULL;
    memcpy(c, l, sizeof(struct prefix_list));
    return c;
}

int
prefix_list_eq(struct prefix_list *l1, struct prefix_list *l2)
{
    return l1->n == l2->n &&
        memcmp(l1->l, l2->l, l1->n * sizeof(struct prefix)) == 0;
}

int
prefix_list_v4(struct prefix_list *l)
{
    int i;
    for(i = 0; i < l->n; i++) {
        if(l->l[i].plen < 96 ||
           memcmp(l->l[i].p, v4prefix, 12) != 0)
            return 0;
    }
    return 1;
}

void
prefix_list_extract6(unsigned char *dest, struct prefix_list *p)
{
    if(!p || p->n == 0)
        memset(dest, 0, 16);
    memcpy(dest, p->l[0].p, 16);
}

void
prefix_list_extract4(unsigned char *dest, struct prefix_list *p)
{
    if(!p || p->n == 0)
        memset(dest, 0, 4);
    memcpy(dest, p->l[0].p + 12, 4);
}

static void
parse_a6(struct prefix *p, const unsigned char *data)
{
    memcpy(p->p, data, 16);
    p->plen = 0xFF;
}

static void
parse_a4(struct prefix *p, const unsigned char *data)
{
    memcpy(p->p, v4prefix, 12);
    memcpy(p->p + 12, data, 4);
    p->plen = 0xFF;
}

static void
parse_p6(struct prefix *p, const unsigned char *data)
{
    memcpy(p->p, data, 16);
    p->plen = data[16];
}

static void
parse_p4(struct prefix *p, const unsigned char *data)
{
    memcpy(p->p, v4prefix, 12);
    memcpy(p->p + 12, data, 4);
    p->plen = data[4] + 96;
}

/* Note that this returns an empty prefix list, rather than NULL, when len
   is 0.  We rely on this in order to keep track of requested options. */
struct prefix_list *
raw_prefix_list(const unsigned char *data, int len, int kind)
{
    struct prefix_list *l = calloc(1, sizeof(struct prefix_list));
    int i, size;
    void (*parser)(struct prefix *, const unsigned char*) = NULL;

    if(l == NULL)
        return NULL;

    switch(kind) {
    case IPv6_ADDRESS: size = 16; parser = parse_a6; break;
    case IPv4_ADDRESS: size = 4; parser = parse_a4; break;
    case IPv6_PREFIX: size = 17; parser = parse_p6; break;
    case IPv4_PREFIX: size = 5; parser = parse_p4; break;
    default: abort();
    }

    if(len % size != 0)
        return NULL;

    i = 0;
    while(i < len) {
        if(l->n >= MAX_PREFIX)
            break;
        parser(&l->l[l->n], data + i);
        l->n++;
        i += size;
    }
    return l;
}

/* Uses a static buffer. */
char *
format_prefix_list(struct prefix_list *p, int kind)
{
    static char buf[120];
    int i, j, k;
    const char *r;

    j = 0;
    for(i = 0; i < p->n; i++) {
        switch(kind) {
        case IPv6_ADDRESS:
            r = inet_ntop(AF_INET6, p->l[i].p, buf + j, 120 - j);
            if(r == NULL) return NULL;
            j += strlen(r);
            break;
        case IPv4_ADDRESS:
            r = inet_ntop(AF_INET, p->l[i].p + 12, buf + j, 120 - j);
            if(r == NULL) return NULL;
            j += strlen(r);
            break;
        case ADDRESS:
            if(memcmp(p->l[i].p, v4prefix, 12) == 0)
                r = inet_ntop(AF_INET, p->l[i].p + 12, buf + j, 120 - j);
            else
                r = inet_ntop(AF_INET6, p->l[i].p, buf + j, 120 - j);
            if(r == NULL) return NULL;
            j += strlen(r);
            break;
        case IPv6_PREFIX:
            r = inet_ntop(AF_INET6, p->l[i].p, buf + j, 120 - j);
            if(r == NULL) return NULL;
            j += strlen(r);
            k = snprintf(buf + j, 120 - j, "/%u", p->l[i].plen);
            if(k < 0 || k >= 120 - j) return NULL;
            j += k;
            break;
        case IPv4_PREFIX:
            r = inet_ntop(AF_INET, p->l[i].p + 12, buf + j, 120 - j);
            if(r == NULL) return NULL;
            j += strlen(r);
            k = snprintf(buf + j, 120 - j, "/%u", p->l[i].plen - 96);
            if(k < 0 || k >= 120 - j) return NULL;
            j += k;
            break;
        default: abort();
        }
        if(j >= 119) return NULL;
        if(i + 1 < p->n)
            buf[j++] = ' ';
    }
    if(j >= 120)
        return NULL;
    buf[j++] = '\0';
    return buf;
}

struct prefix_list *
parse_prefix(char *address, int kind)
{
    struct prefix_list *list;
    unsigned char ipv6[16], ipv4[4];
    int plen, rc;

    plen = 0xFF;

    switch(kind) {
    case IPv6_ADDRESS:
        rc = inet_pton(AF_INET6, address, ipv6);
        if(rc > 0)
            goto return_ipv6;
        return NULL;
    case IPv4_ADDRESS:
        rc = inet_pton(AF_INET, address, ipv4);
        if(rc > 0)
            goto return_ipv4;
    case ADDRESS:
        rc = inet_pton(AF_INET, address, ipv4);
        if(rc > 0)
            goto return_ipv4;
        rc = inet_pton(AF_INET6, address, ipv6);
        if(rc > 0)
            goto return_ipv6;
        return NULL;
    case PREFIX: {
        char buf[30];
        char *p;

        p = strchr(address, '/');
        if(p == NULL || p - address >= 30)
            return NULL;
        plen = atoi(p + 1);
        if(plen <= 0 || plen > 128)
            return NULL;
        memcpy(buf, address, p - address);
        buf[p - address] = '\0';
        rc = inet_pton(AF_INET, buf, ipv4);
        if(rc > 0) {
            if(plen > 32)
                return NULL;
            goto return_ipv4;
        }
        rc = inet_pton(AF_INET6, buf, ipv6);
        if(rc > 0)
            goto return_ipv6;
        return NULL;
    }
    default:
        abort();
    }

 return_ipv6:
    list = calloc(1, sizeof(struct prefix_list));
    if(list == NULL)
        return NULL;
    list->n = 1;
    memcpy(list->l[0].p, ipv6, 16);
    list->l[0].plen = plen;
    return list;

 return_ipv4:
    list = calloc(1, sizeof(struct prefix_list));
    if(list == NULL)
        return NULL;
    list->n = 1;
    memcpy(list->l[0].p, v4prefix, 12);
    memcpy(list->l[0].p + 12, ipv4, 4);
    list->l[0].plen = plen == 0xFF ? 0xFF : plen + 96;
    return list;
}

struct prefix_list *
cat_prefix_list(struct prefix_list *p1, struct prefix_list *p2)
{
    int i;

    if(p1 == NULL)
        return p2;

    if(p2 == NULL)
        return p1;

    if(p1->n + p2->n > MAX_PREFIX)
        return NULL;

    for(i = 0; i < p2->n; i++)
        p1->l[p1->n + i] = p2->l[i];

    p1->n += p2->n;

    free(p2);
    return p1;
}
