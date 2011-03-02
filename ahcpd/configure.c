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
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#ifdef __linux__
#include <netinet/ether.h>
#endif

#include "ahcpd.h"
#include "monotonic.h"
#include "prefix.h"
#include "config.h"
#include "protocol.h"
#include "configure.h"

struct config_data *config_data = NULL;
const unsigned char v4prefix[16] = 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0 };

static int
interface_ipv4(const char *ifname, unsigned char *addr_r)
{
    struct ifreq req;
    int s, rc;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if(s < 0)
        return -1;

    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, ifname, sizeof(req.ifr_name));
    req.ifr_addr.sa_family = AF_INET;
    rc = ioctl(s, SIOCGIFADDR, &req);
    if(rc < 0) {
        close(s);
        return -1;
    }

    if(req.ifr_addr.sa_family != AF_INET)
        return -1;

    memcpy(addr_r, &((struct sockaddr_in*)&req.ifr_addr)->sin_addr, 4);
    close(s);
    return 1;
}

static struct prefix_list *
my_ipv4(char **interfaces)
{
    struct prefix_list *l;
    int i, rc;

    l = calloc(1, sizeof(struct prefix_list));
    if(l == NULL)
        return NULL;

    i = 0;
    while(l->n < MAX_PREFIX && interfaces[i]) {
        l->l[l->n].plen = 0xFF;
        memcpy(l->l[l->n].p, v4prefix, 12);
        rc = interface_ipv4(interfaces[i], l->l[l->n].p + 12);
        if(rc > 0)
            l->n++;
        i++;
    }
    if(l->n == 0) {
        free(l);
        return NULL;
    }

    return l;
}
    
static int
run_script(const char *action, struct config_data *config, char **interfaces)
{
    pid_t pid;
    pid = fork();
    if(pid < 0) {
        perror("fork");
        return -1;
    } else if(pid == 0) {
        char buf[200];
        int i;
        snprintf(buf, 50, "%lu", (unsigned long)getppid());
        setenv("AHCP_DAEMON_PID", buf, 1);
        buf[0] = '\0';
        i = 0;
        while(interfaces[i]) {
            if(i > 0)
                strncat(buf, " ", 200);
            strncat(buf, interfaces[i], 200);
            i++;
        }
        setenv("AHCP_INTERFACES", buf, 1);
        snprintf(buf, 50, "%d", debug);
        setenv("AHCP_DEBUG_LEVEL", buf, 1);
        if(config->our_ipv6_address && (af & 2))
            setenv("AHCP_IPv6_ADDRESS",
                   format_prefix_list(config->our_ipv6_address, IPv6_ADDRESS),
                   1);
        if(config->ipv4_address && (af & 1))
            setenv("AHCP_IPv4_ADDRESS",
                   format_prefix_list(config->ipv4_address, IPv4_ADDRESS), 1);

        if(config->ipv6_prefix_delegation && (af & 2))
            setenv("AHCP_IPv6_PREFIX_DELEGATION",
                   format_prefix_list(config->ipv6_prefix_delegation,
                                      IPv6_PREFIX),
                   1);
        if(config->ipv4_prefix_delegation && (af & 2))
            setenv("AHCP_IPv4_PREFIX_DELEGATION",
                   format_prefix_list(config->ipv4_prefix_delegation,
                                      IPv4_PREFIX),
                   1);

        if(config->name_server && !nodns)
            setenv("AHCP_NAMESERVER",
                   format_prefix_list(config->name_server, ADDRESS), 1);

        if(config->ntp_server)
            setenv("AHCP_NTP_SERVER",
                   format_prefix_list(config->ntp_server, ADDRESS), 1);

        debugf(1, "Running ``%s %s''\n", config_script, action);

        execl(config_script, config_script, action, NULL);
        perror("exec failed");
        exit(42);
    } else {
        int status;
    again:
        pid = waitpid(pid, &status, 0);
        if(pid < 0) {
            if(errno == EINTR)
                goto again;
            perror("wait");
            return -1;
        } else if(!WIFEXITED(status)) {
            fprintf(stderr, "Child died violently (%d)\n", status);
            return -1;
        } else if(WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child returned error status %d\n",
                    WEXITSTATUS(status));
            return -1;
        }
    }
    return 1;
}

unsigned int
config_renew_time(void)
{
    return config_data->origin_m + config_data->expires * 5 / 6;
}

void
free_config_data(struct config_data *config)
{
    free_prefix_list(config->server_ipv6);
    free_prefix_list(config->server_ipv4);
    free_prefix_list(config->ipv6_prefix);
    free_prefix_list(config->ipv4_prefix);
    free_prefix_list(config->ipv6_address);
    free_prefix_list(config->ipv4_address);
    free_prefix_list(config->ipv6_prefix_delegation);
    free_prefix_list(config->ipv4_prefix_delegation);
    free_prefix_list(config->name_server);
    free_prefix_list(config->ntp_server);
    free_prefix_list(config->our_ipv6_address);
    free(config);
}

struct config_data *
copy_config_data(struct config_data *config)
{
    struct config_data *c = malloc(sizeof(struct config_data));
    if(c == NULL)
        return NULL;

    *c = *config;

#define COPY(field) \
    do { if(c->field) c->field = copy_prefix_list(c->field); } while(0)
    COPY(server_ipv6);
    COPY(server_ipv4);
    COPY(ipv6_prefix);
    COPY(ipv4_prefix);
    COPY(ipv6_address);
    COPY(ipv4_address);
    COPY(ipv6_prefix_delegation);
    COPY(ipv4_prefix_delegation);
    COPY(name_server);
    COPY(ntp_server);
    COPY(our_ipv6_address);
#undef COPY

    return c;
}

int
config_data_compatible(struct config_data *config1, struct config_data *config2)
{

    if(!!config1->ipv4_address != !!config2->ipv4_address ||
       !!config1->ipv6_address != !!config2->ipv6_address ||
       !!config1->ipv6_prefix != !!config2->ipv6_prefix ||
       !!config1->ipv4_prefix_delegation != !!config2->ipv4_prefix_delegation ||
       !!config1->ipv6_prefix_delegation != !!config2->ipv6_prefix_delegation)
        return 0;

    if(config1->ipv4_address) {
        if(!prefix_list_eq(config1->ipv4_address, config2->ipv4_address) != 0)
            return 0;
    }

    if(config1->ipv6_address) {
        if(!prefix_list_eq(config1->ipv6_address, config2->ipv6_address) != 0)
            return 0;
    }

    if(config1->ipv6_prefix) {
        if(!prefix_list_eq(config1->ipv6_prefix, config2->ipv6_prefix) != 0)
            return 0;
    }

    if(config1->ipv4_prefix_delegation) {
        if(!prefix_list_eq(config1->ipv4_prefix_delegation,
                           config2->ipv4_prefix_delegation) != 0)
            return 0;
    }

    if(config1->ipv6_prefix_delegation) {
        if(!prefix_list_eq(config1->ipv6_prefix_delegation,
                           config2->ipv6_prefix_delegation) != 0)
            return 0;
    }

    return 1;
}

struct config_data *
make_config_data(int expires,
                 unsigned char *ipv4, struct server_config *server_config,
                 char **interfaces)
{
    struct config_data *config;
    struct timeval now, real;
    int clock_status;
    struct prefix_list *my_address;

    gettime(&now, NULL);
    get_real_time(&real, &clock_status);

    config = calloc(1, sizeof(struct config_data));
    if(config == NULL)
        return NULL;

    config->expires = expires;
    config->origin_m = now.tv_sec;
    if(clock_status != CLOCK_BROKEN)
        config->origin = real.tv_sec;

    config->expires_m = config->origin_m + config->expires;

    if(ipv4)
        config->ipv4_address = raw_prefix_list(ipv4, 4, IPv4_ADDRESS);

    if(server_config->ipv6_prefix)
        config->ipv6_prefix = copy_prefix_list(server_config->ipv6_prefix);

    if(server_config->name_server)
        config->name_server = copy_prefix_list(server_config->name_server);

    if(server_config->ntp_server)
        config->ntp_server = copy_prefix_list(server_config->ntp_server);

    my_address = my_ipv4(interfaces);
    if(my_address)
        config->server_ipv4 = my_address;

    return config;
}

/* Parse a message.  Configure is 1 to configure, 0 to pretend, and -1 to
   omit a number of sanity checks when parsing client messages. */
struct config_data *
parse_message(int configure, const unsigned char *data, int len,
              char **interfaces)
{
    int bodylen, i, opt, olen, rc;
    int mandatory = 0;
    const unsigned char *body;
    struct config_data *config;
    unsigned origin = 0, expires = 0;
    struct timeval now, real;
    int clock_status;

#define CAT(dst, src)                           \
    do {                                        \
        struct prefix_list *pcat;               \
        pcat = cat_prefix_list(dst, src);       \
        if(pcat == NULL)                        \
            goto fail;                          \
        dst = pcat;                             \
    } while(0)

    if(len < 4)
        return NULL;

    gettime(&now, NULL);
    get_real_time(&real, &clock_status);

    body = data + 4;

    bodylen = (data[2] << 8) | data[3];
    if(bodylen > len - 4)
        return NULL;

    config = calloc(1, sizeof(struct config_data));
    if(config == NULL)
        return NULL;

    i = 0;
    while(i < bodylen) {
        opt = body[i];
        if(opt == OPT_PAD) {
            mandatory = 0;
            i++;
            continue;
        } else if(opt == OPT_MANDATORY) {
            mandatory = 1;
            i++;
            continue;
        }

        olen = body[i + 1];
        if(olen + 2 + i > bodylen) {
            fprintf(stderr, "Truncated message.\n");
            goto fail;
        }

        if(opt == OPT_ORIGIN_TIME) {
            unsigned int when;

            if(olen != 4)
                goto fail;

            memcpy(&when, body + i + 2, 4);
            when = ntohl(when);
            if(origin <= 0)
                origin = when;
            else
                origin = MIN(origin, when);
        } else if(opt == OPT_EXPIRES) {
            unsigned int secs;

            if(olen != 4)
                goto fail;

            memcpy(&secs, body + i + 2, 4);
            secs = ntohl(secs);
            if(expires <= 0)
                expires = secs;
            else
                expires = MIN(expires, secs);
        } else if(opt == OPT_IPv6_PREFIX || opt == OPT_IPv6_PREFIX_DELEGATION) {
            struct prefix_list *value;
            if(olen % 17 != 0) {
                fprintf(stderr, "Unexpected length for prefix.\n");
                goto fail;
            }

            value = raw_prefix_list(body + i + 2, olen, IPv6_PREFIX);
            if(opt == OPT_IPv6_PREFIX) {
                CAT(config->ipv6_prefix, value);
                if(mandatory)
                    config->ipv6_mandatory = 1;
            } else {
                CAT(config->ipv6_prefix_delegation, value);
                if(mandatory)
                    config->ipv6_delegation_mandatory = 1;
            }
        } else if(opt == OPT_IPv4_PREFIX_DELEGATION) {
            struct prefix_list *value;
            if(olen % 5 != 0) {
                fprintf(stderr, "Unexpected length for prefix.\n");
                goto fail;
            }
            value = raw_prefix_list(body + i + 2, olen, IPv4_PREFIX);
            CAT(config->ipv4_prefix_delegation, value);
            if(mandatory)
                config->ipv4_delegation_mandatory = 1;
        } else if(opt == OPT_MY_IPv6_ADDRESS || opt == OPT_IPv6_ADDRESS ||
                  opt == OPT_NAME_SERVER || opt == OPT_NTP_SERVER) {
            struct prefix_list *value;
            if(olen % 16 != 0) {
                fprintf(stderr, "Unexpected length for %s.\n",
                        opt == OPT_IPv6_ADDRESS ? "address" : "server");
                goto fail;
            }

            value = raw_prefix_list(body + i + 2, olen, IPv6_ADDRESS);
            if(opt == OPT_MY_IPv6_ADDRESS) {
                CAT(config->server_ipv6, value);
            } else if(opt == OPT_IPv6_ADDRESS) {
                CAT(config->ipv6_address, value);
                if(mandatory)
                    config->ipv6_mandatory = 1;
            } else if(opt == OPT_NAME_SERVER) {
                CAT(config->name_server, value);
            } else if(opt == OPT_NTP_SERVER) {
                CAT(config->ntp_server, value);
            } else {
                abort();
            }
        } else if(opt == OPT_MY_IPv4_ADDRESS || opt == OPT_IPv4_ADDRESS) {
            struct prefix_list *value;
            value = raw_prefix_list(body + i + 2, olen, IPv4_ADDRESS);
            if(opt == OPT_MY_IPv4_ADDRESS) {
                CAT(config->server_ipv4, value);
            } else if(opt == OPT_IPv4_ADDRESS) {
                CAT(config->ipv4_address, value);
                if(mandatory)
                    config->ipv4_mandatory = 1;
            } else {
                abort();
            }
        } else {
            if(mandatory)
                debugf(1, "Unsupported option %d\n", opt);
            if(mandatory && configure >= 0)
                goto fail;
        }
        mandatory = 0;
        i += olen + 2;
    }

    if(configure >= 0 && expires <= 0)
        goto fail;

    config->origin_m = now.tv_sec;
    config->expires = MIN(25 * 3600, expires);

    if(origin >= 0) {
        config->origin = origin;
        if(configure >= 0) {
            if(origin >= real.tv_sec - 300 && origin <= real.tv_sec + 300) {
                time_confirm(1);
            } else {
                fprintf(stderr,
                        "Detected clock skew (client=%ld, server=%ld).\n",
                        (long)now.tv_sec, (long)config->origin);
                time_confirm(0);
            }
        }
    }

    if(configure >= 0) {
        config->expires_m = config->origin_m + config->expires;

        if(clock_status != CLOCK_BROKEN && config->origin > 0) {
            config->expires_m =
                MIN(config->expires_m,
                    config->origin_m + (real.tv_sec - config->origin) +
                    config->expires);
        }

        if(config->ipv6_address) {
            config->our_ipv6_address = copy_prefix_list(config->ipv6_address);
        } else if(config->ipv6_prefix && config->ipv6_prefix->n > 0 &&
                  config->ipv6_prefix->l[0].plen <= 64) {
            unsigned char address[16];
            int have_address = 0;

            memcpy(address, config->ipv6_prefix->l[0].p, 16);

            i = 0;
            while(interfaces[i]) {
                rc = if_eui64(interfaces[i], address + 8);
                if(rc >= 0)
                    have_address = 1;
                i++;
            }

            if(!have_address) {
                rc = random_eui64(address + 8);
                if(rc >= 0)
                    have_address = 1;
            }
            
            if(have_address)
                config->our_ipv6_address =
                    raw_prefix_list(address, 16, IPv6_ADDRESS);
            if(!config->our_ipv6_address)
                fprintf(stderr, "Couldn't generate IPv6 address.\n");
        }
    }

    if(configure > 0 && config_script[0] != '\0' &&
       config->expires_m > now.tv_sec + 5) {
        if(config_data) {
            if(!config_data_compatible(config_data, config))
                unconfigure(interfaces);
        }
        if(!config_data) {
            rc = run_script("start", config, interfaces);
            if(rc > 0)
                config_data = copy_config_data(config);
        } else {
            config_data->origin = config->origin;
            config_data->origin_m = config->origin_m;
            config_data->expires = config->expires;
        }
    }

    return config;

 fail:
    free_config_data(config);
    return NULL;

#undef CAT
}

int
unconfigure(char **interfaces)
{
    int rc;
    rc = run_script("stop", config_data, interfaces);
    free_config_data(config_data);
    config_data = NULL;
    return rc;
}

int
query_body(unsigned char opcode, int time, const unsigned char *ipv4,
           unsigned char *buf, int buflen)
{
    int i, j;
    unsigned nowsecs, expires;
    struct timeval real;
    int clock_status;

    get_real_time(&real, &clock_status);

    /* Skip header */
    i = 4; if(i >= buflen) goto fail;

    if(clock_status != CLOCK_BROKEN) {
        nowsecs = htonl(real.tv_sec);
        buf[i++] = OPT_ORIGIN_TIME; if(i >= buflen) goto fail;
        buf[i++] = 4; if(i >= buflen - 4) goto fail;
        memcpy(buf + i, &nowsecs, 4);
        i += 4;
    }

    if(time > 0) {
        expires = htonl(time);
        buf[i++] = OPT_EXPIRES; if(i >= buflen) goto fail;
        buf[i++] = 4; if(i >= buflen - 4) goto fail;
        memcpy(buf + i, &expires, 4);
        i += 4;
    }

    if((af & 1)) {
        buf[i++] = OPT_IPv4_ADDRESS; if(i >= buflen) goto fail;
        buf[i++] = ipv4 ? 4 : 0; if(i >= buflen) goto fail;
        if(ipv4) {
            if(i >= buflen - 4) goto fail;
            memcpy(buf + i, ipv4, 4);
            i += 4;
        }
        if(request_prefix_delegation) {
            buf[i++] = OPT_IPv4_PREFIX_DELEGATION; if(i >= buflen) goto fail;
            buf[i++] = 0; if(i >= buflen) goto fail;
        }
    }

    if((af & 2)) {
        buf[i++] = OPT_IPv6_PREFIX; if(i >= buflen) goto fail;
        buf[i++] = 0; if(i >= buflen) goto fail;
        if(request_prefix_delegation) {
            buf[i++] = OPT_IPv6_PREFIX_DELEGATION; if(i >= buflen) goto fail;
            buf[i++] = 0; if(i >= buflen) goto fail;
        }
    }

    if(opcode != AHCP_RELEASE) {
        if(!nodns) {
            buf[i++] = OPT_NAME_SERVER; if(i >= buflen) goto fail;
            buf[i++] = 0; if(i >= buflen) goto fail;
        }
        buf[i++] = OPT_NTP_SERVER; if(i >= buflen) goto fail;
        buf[i++] = 0; if(i >= buflen) goto fail;
    }

    /* Set up header */
    j = i - 4;
    buf[0] = opcode;
    buf[1] = 0;
    buf[2] = (j >> 8) & 0xFF;
    buf[3] = j & 0xFF;

    return i;

 fail:
    return -1;
}

int
server_body(unsigned char opcode, struct config_data *config,
            unsigned char *buf, int buflen)
{
    int i, j;
    struct timeval now, real;
    int clock_status;
    unsigned nowsecs, expires;
    
    gettime(&now, NULL);
    get_real_time(&real, &clock_status);

    /* Skip header */
    i = 4; if(i >= buflen) goto fail;

    if(now.tv_sec >= config->expires_m)
        return -1;

    expires = config->expires;

    if(clock_status != CLOCK_BROKEN) {
        nowsecs = htonl(real.tv_sec);
        buf[i++] = OPT_ORIGIN_TIME; if(i >= buflen) goto fail;
        buf[i++] = 4; if(i >= buflen - 4) goto fail;
        memcpy(buf + i, &nowsecs, 4);
        i += 4;
    }

    expires = htonl(expires);
    buf[i++] = OPT_MANDATORY; if(i >= buflen) goto fail;
    buf[i++] = OPT_EXPIRES; if(i >= buflen) goto fail;
    buf[i++] = 4; if(i >= buflen - 4) goto fail;
    memcpy(buf + i, &expires, 4);
    i += 4;

    if(config->ipv4_address) {
        int j;
        buf[i++] = OPT_IPv4_ADDRESS; if(i >= buflen) goto fail;
        buf[i++] = 4 * config->ipv4_address->n;
        if(i >= buflen) goto fail;
        for(j = 0; j < config->ipv4_address->n; j++) {
            if(i >= buflen - 4) goto fail;
            memcpy(buf + i, config->ipv4_address->l[j].p + 12, 4);
            i += 4;
        }
    }

    if(config->ipv6_prefix) {
        int j;
        buf[i++] = OPT_IPv6_PREFIX; if(i >= buflen) goto fail;
        buf[i++] = 17 * config->ipv6_prefix->n; if(i >= buflen) goto fail;
        for(j = 0; j < config->ipv6_prefix->n; j++) {
            if(i >= buflen - 17) goto fail;
            memcpy(buf + i, config->ipv6_prefix->l[j].p, 16);
            i += 16;
            buf[i++] = config->ipv6_prefix->l[j].plen;
        }
    }

    if(config->ipv6_address) {
        int j;
        buf[i++] = OPT_IPv6_ADDRESS; if(i >= buflen) goto fail;
        buf[i++] = 16 * config->ipv6_address->n;
        if(i >= buflen) goto fail;
        for(j = 0; j < config->ipv6_address->n; j++) {
            if(i >= buflen - 16) goto fail;
            memcpy(buf + i, config->ipv6_address->l[j].p, 16);
            i += 16;
        }
    }

    if(config->name_server) {
        int j;
        buf[i++] = OPT_NAME_SERVER; if(i >= buflen) goto fail;
        buf[i++] = 16 * config->name_server->n;
        if(i >= buflen) goto fail;
        for(j = 0; j < config->name_server->n; j++) {
            if(i >= buflen - 6) goto fail;
            memcpy(buf + i, config->name_server->l[j].p, 16);
            i += 16;
        }
    }

    if(config->ntp_server) {
        int j;
        buf[i++] = OPT_NTP_SERVER; if(i >= buflen) goto fail;
        buf[i++] = 16 * config->ntp_server->n;
        if(i >= buflen) goto fail;
        for(j = 0; j < config->ntp_server->n; j++) {
            if(i >= buflen - 6) goto fail;
            memcpy(buf + i, config->ntp_server->l[j].p, 16);
            i += 16;
        }
    }

    if(config->server_ipv6) {
        int j;
        buf[i++] = OPT_MY_IPv6_ADDRESS; if(i >= buflen) goto fail;
        buf[i++] = 16 * config->server_ipv6->n; if(i >= buflen) goto fail;
        for(j = 0; j < config->server_ipv6->n; j++) {
            if(i >= buflen - 16) goto fail;
            memcpy(buf + i, config->server_ipv6->l[j].p, 16);
            i += 16;
        }
    }

    if(config->server_ipv4) {
        int j;
        buf[i++] = OPT_MY_IPv4_ADDRESS; if(i >= buflen) goto fail;
        buf[i++] = 4 * config->server_ipv4->n; if(i >= buflen) goto fail;
        for(j = 0; j < config->server_ipv4->n; j++) {
            if(i >= buflen - 4) goto fail;
            memcpy(buf + i, config->server_ipv4->l[j].p + 12, 4);
            i += 4;
        }
    }

    /* Set up header */
    j = i - 4;
    buf[0] = opcode;
    buf[1] = 0;
    buf[2] = (j >> 8) & 0xFF;
    buf[3] = j & 0xFF;

    return i;

 fail:
    return -1;
}

int
address_conflict(struct prefix_list *a, struct prefix_list *b)
{
    int i, j;

    for(i = 0; i < a->n; i++)
        for(j = 0; j < b->n; j++)
            if(memcmp(a->l[i].p, b->l[j].p, 16) == 0)
                return 1;
    return 0;
}

/* Determine an interface's hardware address, in modified EUI-64 format */
#ifdef SIOCGIFHWADDR
int
if_eui64(char *ifname, unsigned char *eui)
{
    int s, rc;
    struct ifreq ifr;

    s = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if(s < 0) return -1;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    rc = ioctl(s, SIOCGIFHWADDR, &ifr);
    if(rc < 0) {
        int saved_errno = errno;
        close(s);
        errno = saved_errno;
        return -1;
    }
    close(s);

    switch(ifr.ifr_hwaddr.sa_family) {
    case ARPHRD_ETHER:
    case ARPHRD_FDDI:
    case ARPHRD_IEEE802_TR:
    case ARPHRD_IEEE802: {
        unsigned char *mac;
        mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
        /* Check for null address and group and global bits */
        if(memcmp(mac, zeroes, 6) == 0 ||
           (mac[0] & 1) != 0 || (mac[0] & 2) != 0) {
            errno = ENOENT;
            return -1;
        }
        memcpy(eui, mac, 3);
        eui[3] = 0xFF;
        eui[4] = 0xFE;
        memcpy(eui + 5, mac + 3, 3);
        eui[0] ^= 2;
        return 1;
    }
    case ARPHRD_EUI64:
    case ARPHRD_IEEE1394:
    case ARPHRD_INFINIBAND: {
        unsigned char *mac;
        mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
        if(memcmp(mac, zeroes, 8) == 0 ||
           (mac[0] & 1) != 0 || (mac[0] & 2) != 0) {
            errno = ENOENT;
            return -1;
        }
        memcpy(eui, mac, 64);
        eui[0] ^= 2;
        return 1;
    }
    default:
        errno = ENOENT;
        return -1;
    }
}

#else

#warning Cannot determine MAC addresses on this platform

int
if_eui64(char *ifname, unsigned char *eui)
{
    errno = ENOSYS;
    return -1;
}
#endif

#if defined(__linux)
#define RND_DEV "/dev/random"
#elif defined(__OpenBSD__)
#define RND_DEV "/dev/arandom"
#endif

int
random_eui64(unsigned char *eui)
{
#ifdef RND_DEV
    int fd, rc;
    fd = open(RND_DEV, O_RDONLY);
    if(fd < 0)
        return -1;

    rc = read(fd, eui, 8);
    if(rc < 0)
        return -1;
    close(fd);

    eui[0] &= ~3;
    return 1;
#else
    errno = ENOSYS;
    return -1;
#endif
}
