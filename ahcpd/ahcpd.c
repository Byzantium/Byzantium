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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "ahcpd.h"
#include "monotonic.h"
#include "protocol.h"
#include "transport.h"
#include "prefix.h"
#include "config.h"
#include "configure.h"
#include "lease.h"

#define BUFFER_SIZE 2048

struct timeval now;
const struct timeval zero = {0, 0};

static volatile sig_atomic_t exiting = 0, dumping = 0, changed = 0;
struct in6_addr protocol_group;
unsigned int protocol_port = 5359;
int protocol_socket = -1;
unsigned char myid[8];
char *unique_id_file = "/var/lib/ahcpd-unique-id";
int nodns = 0, af = 3, request_prefix_delegation = 0;
char *config_script = "/etc/ahcp/ahcp-config.sh";
int debug = 1;
int do_daemonise = 0;
char *logfile = NULL, *pidfile = "/var/run/ahcpd.pid";

#define CHECK_NETWORKS 1
#define MESSAGE 2

struct network networks[MAXNETWORKS];
int numnetworks;
char *interfaces[MAXNETWORKS + 1];
struct timeval check_networks_time = {0, 0};

struct timeval message_time = {0, 0};

const unsigned char zeroes[16] = {0};
const unsigned char ones[16] =
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };

static void init_signals(void);
static int check_network(struct network *net);
int ahcp_socket(int port);
int ahcp_recv(int s, void *buf, int buflen, struct sockaddr *sin, int slen);
static int send_unicast_packet(unsigned char *server_id,
                               struct config_data *config, int index,
                               void *buf, int buflen);
int timeval_compare(const struct timeval *s1, const struct timeval *s2);
static int reopen_logfile(void);
static int daemonise(void);
static void set_timeout(int which, int msecs, int override);
unsigned roughly(unsigned value);

/* Client states */

enum state {
    STATE_IDLE,                 /* we're not a client */
    STATE_INIT,                 /* searching for server */
    STATE_REQUESTING,           /* found a server */
    STATE_RENEWING_UNICAST,     /* configured and starting to get nervous */
    STATE_RENEWING,             /* configured but panicking */
    STATE_BOUND                 /* configured and cool */
};

int
main(int argc, char **argv)
{
    char *multicast = "ff02::cca6:c0f9:e182:5359";
    char *config_file = NULL;
    struct sockaddr_in6 sin6;
    int opt, fd, rc, i, net;
    unsigned int seed;
    enum state state = STATE_IDLE;
    int count = 0;
    int server_hopcount = 0;
    unsigned char selected_server[8];
    unsigned char last_ipv4[4] = {0};
    unsigned lease_time = 3666;

    
    while(1) {
        opt = getopt(argc, argv, "m:p:nN46s:d:i:t:P:c:C:DL:I:");
        if(opt < 0)
            break;

        switch(opt) {
        case 'm':
            multicast = optarg;
            break;
        case 'p':
            protocol_port = atoi(optarg);
            if(protocol_port <= 0 || protocol_port > 0xFFFF)
                goto usage;
            break;
        case 'n':
            client_config = 0;
            break;
        case 'N':
            nodns = 1;
            break;
        case '4':
            af = 1;
            break;
        case '6':
            af = 2;
            break;
        case 's':
            config_script = optarg;
            break;
        case 'd':
            debug = atoi(optarg);
            break;
        case 'i':
            unique_id_file = optarg;
            break;
        case 't':
            lease_time = atoi(optarg);
            if(lease_time < 300 || lease_time > 365 * 24 * 3600)
                goto usage;
            break;
        case 'P':
            request_prefix_delegation = 1;
            break;
        case 'c':
            config_file = optarg;
            break;
        case 'C':
            rc = parse_config_from_string(optarg);
            if(rc < 0) {
                fprintf(stderr,
                        "Couldn't parse configuration from command line.\n");
                exit(1);
            }
            break;
        case 'D':
            do_daemonise = 1;
            break;
        case 'L':
            logfile = optarg;
            break;
        case 'I':
            pidfile = optarg;
            break;
        default:
            goto usage;
        }
    }

    if(optind >= argc)
        goto usage;

    if(config_file) {
        rc = parse_config_from_file(config_file);
        if(rc < 0) {
            fprintf(stderr,
                    "Couldn't parse configuration from file %s.\n",
                    config_file);
            exit(1);
        }
    }

    for(i = optind; i < argc; i++) {
        if(i - optind >= MAXNETWORKS) {
            fprintf(stderr, "Too many interfaces.\n");
            exit(1);
        }
        interfaces[i - optind] = argv[i];
    }
    numnetworks = i - optind;
    interfaces[i - optind] = NULL;

    rc = inet_pton(AF_INET6, multicast, &protocol_group);
    if(rc <= 0)
        goto usage;

    time_init();

    if(do_daemonise) {
        if(logfile == NULL)
            logfile = "/var/log/ahcpd.log";
    }

    rc = reopen_logfile();
    if(rc < 0) {
        perror("reopen_logfile()");
        exit(1);
    }

    fd = open("/dev/null", O_RDONLY);
    if(fd < 0) {
        perror("open(null)");
        exit(1);
    }

    rc = dup2(fd, 0);
    if(rc < 0) {
        perror("dup2(null, 0)");
        exit(1);
    }

    close(fd);

    if(do_daemonise) {
        rc = daemonise();
        if(rc < 0) {
            perror("daemonise");
            exit(1);
        }
    }

    if(pidfile) {
        int pfd, len;
        char buf[100];

        len = snprintf(buf, 100, "%lu", (unsigned long)getpid());
        if(len < 0 || len >= 100) {
            perror("snprintf(getpid)");
            exit(1);
        }

        pfd = open(pidfile, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if(pfd < 0) {
            perror("creat(pidfile)");
            exit(1);
        }

        rc = write(pfd, buf, len);
        if(rc < len) {
            perror("write(pidfile)");
            goto fail;
        }

        close(pfd);
    }
    
    gettime(&now, NULL);

    fd = open("/dev/urandom", O_RDONLY);
    if(fd < 0) {
        struct timeval tv;
        perror("open(random)");
        get_real_time(&tv, NULL);
        seed = tv.tv_sec ^ tv.tv_usec;
    } else {
        rc = read(fd, &seed, sizeof(unsigned int));
        if(rc < sizeof(unsigned int)) {
            perror("read(random)");
            goto fail;
        }
        close(fd);
    }
    srandom(seed);

    myseqno = random();

    if(unique_id_file && unique_id_file[0] != '\0') {
        fd = open(unique_id_file, O_RDONLY);
        if(fd >= 0) {
            rc = read(fd, myid, 8);
            if(rc == 8) {
                close(fd);
                goto unique_id_done;
            }
            close(fd);
        }
    }

    for(i = 0; i < numnetworks; i++) {
        rc = if_eui64(interfaces[i], myid);
        if(rc >= 0)
            goto write_unique_id;
    }

    rc = random_eui64(myid);
    if(rc < 0) {
        fprintf(stderr, "Couldn't generate unique id.\n");
        exit(1);
    }

 write_unique_id:
    if(unique_id_file && unique_id_file[0] != '\0') {
        fd = open(unique_id_file, O_WRONLY | O_TRUNC | O_CREAT, 0644);
        if(fd < 0) {
            perror("creat(unique_id)");
        } else {
            rc = write(fd, myid, 8);
            if(rc != 8) {
                perror("write(unique_id)");
                unlink(unique_id_file);
            }
            close(fd);
        }
    }

 unique_id_done:

    if(server_config) {
#ifndef NO_SERVER
        if(server_config->lease_first[0] != 0) {
            if(server_config->lease_dir == NULL) {
                fprintf(stderr, "No lease directory configured!\n");
                goto fail;
            }
            rc = lease_init(server_config->lease_dir,
                            server_config->lease_first,
                            server_config->lease_last,
                            debug >= 2);
            if(rc < 0) {
                fprintf(stderr, "Couldn't initialise lease database.\n");
                goto fail;
            }
        }
#else
        abort();
#endif
    }

    protocol_socket = ahcp_socket(protocol_port);
    if(protocol_socket < 0) {
        perror("ahcp_socket");
        goto fail;
    }

    for(i = 0; i < numnetworks; i++) {
        networks[i].ifname = interfaces[i];
        check_network(&networks[i]);
        if(networks[i].ifindex <= 0) {
            fprintf(stderr, "Warning: unknown interface %s.\n",
                    networks[i].ifname);
            continue;
        }
    }

    init_signals();
    set_timeout(CHECK_NETWORKS, 30000, 1);

    /* The client state machine. */

#define SWITCH(new)                                                     \
    do {                                                                \
        assert(!!config_data == (new == STATE_BOUND ||                  \
                                 new == STATE_RENEWING_UNICAST ||       \
                                 new == STATE_RENEWING));               \
        debugf(2, "Switching to state %d.\n", new);                     \
        state = new;                                                    \
        if(state == STATE_IDLE || state == STATE_BOUND)                 \
            set_timeout(MESSAGE, 0, 1);                                 \
        else                                                            \
            set_timeout(MESSAGE, 300, 1);                               \
        count = 0;                                                      \
    } while(0)

    if(client_config) {
        server_hopcount = 0;
        SWITCH(STATE_INIT);
    }

    debugf(2, "Entering main loop.\n");

    while(1) {
        fd_set readfds;
        struct timeval tv;

        assert((config_data != NULL) == (state == STATE_BOUND ||
                                         state == STATE_RENEWING_UNICAST ||
                                         state == STATE_RENEWING));

        FD_ZERO(&readfds);

        tv = check_networks_time;
        timeval_min(&tv, &message_time);
        if(config_data) {
            timeval_min_sec(&tv, config_data->expires_m);
            if(state == STATE_BOUND)
                timeval_min_sec(&tv, config_renew_time());
        }

        gettime(&now, NULL);

        if(timeval_compare(&tv, &now) > 0) {
            timeval_minus(&tv, &tv, &now);

            FD_SET(protocol_socket, &readfds);
            debugf(3, "Sleeping for %d.%03ds, state=%d.\n",
                   (int)tv.tv_sec, (int)(tv.tv_usec / 1000), (int)state);
            rc = select(protocol_socket + 1, &readfds, NULL, NULL, &tv);
            if(rc < 0 && errno != EINTR) {
                perror("select");
                sleep(5);
                continue;
            }
        }

        gettime(&now, NULL);

        if(exiting)
            break;

        if(dumping) {
            int clock_status;
            time_t stable;
            struct timeval tv, real;
            dumping = 0;
            get_real_time(&real, &clock_status);
            gettime(&tv, &stable);
            printf("Clock status %d, stable for at least %ld seconds.\n",
                   (int)clock_status, (long)stable);
            printf("Forwarder forwarding.\n");
            if(server_config)
                printf("Server serving.\n");
            if(client_config) {
                printf("Client in state %d, ", (int)state);
                if(memcmp(selected_server, zeroes, 8) != 0)
                    printf("server selected, ");
                if(config_data) {
                    printf("configuration valid for "
                           "%lds (originally %lds since %lds).",
                           (long)((long)config_data->expires_m - now.tv_sec),
                           (long)config_data->expires,
                           (long)config_data->origin);
                } else {
                    printf("not configured.\n");
                }
            }
            printf("\n");
            fflush(stdout);
        }

        if(changed) {
            changed = 0;
            for(i = 0; i < numnetworks; i++)
                check_network(&networks[i]);
            set_timeout(CHECK_NETWORKS, 30000, 1);
            rc = reopen_logfile();
            if(rc < 0) {
                perror("reopen_logfile");
                goto fail;
            }
        }

        if(FD_ISSET(protocol_socket, &readfds)) {
            unsigned char buf[BUFFER_SIZE];
            int len;
            struct sockaddr *psin;
            int sinlen;

            len = ahcp_recv(protocol_socket, buf, BUFFER_SIZE,
                           (struct sockaddr*)&sin6, sizeof(sin6));
            if(len < 0) {
                if(errno != EAGAIN && errno != EINTR) {
                    perror("recv");
                    sleep(5);
                }
                continue;
            }

            psin = (struct sockaddr*)&sin6;
            sinlen = sizeof(sin6);

            if(IN6_IS_ADDR_LINKLOCAL(&sin6.sin6_addr)) {
                for(net = 0; net < numnetworks; net++) {
                    if(networks[net].ifindex <= 0)
                        continue;
                    if(networks[net].ifindex == sin6.sin6_scope_id)
                        break;
                }
                if(net >= numnetworks) {
                    fprintf(stderr, "Received packet on unknown network.\n");
                    continue;
                }
            } else {
                net = -1;
            }

            rc = handle_packet(net >= 0, buf, len);
            gettime(&now, NULL);
            if(rc == 2) {
                unsigned char *body = buf + 24;
                int bodylen = len - 24;

                if(len < 28) {
                    fprintf(stderr, "Received truncated packet (%d).\n", rc);
                    continue;
                }

                debugf(2, "Received packet %d in state %d.\n",
                       body[0], (int)state);

                switch(state) {
                case STATE_IDLE:
                    break;
                case STATE_INIT:
                    if(body[0] == AHCP_OFFER) {
                        struct config_data *config;
                        config = parse_message(0, body, bodylen, interfaces);
                        if(config &&
                           (((af & 1) && config->ipv4_address) ||
                            ((af & 2) && config->ipv6_prefix))) {
                            if((af & 1) && config->ipv4_address)
                                prefix_list_extract4(last_ipv4,
                                                     config->ipv4_address);
                            server_hopcount = buf[3];
                            memcpy(selected_server, buf + 8, 8);
                            SWITCH(STATE_REQUESTING);
                        }
                        if(config)
                            free_config_data(config);
                    }
                    break;
                case STATE_REQUESTING:
                case STATE_RENEWING:
                case STATE_RENEWING_UNICAST:
                    if(body[0] == AHCP_ACK &&
                       memcmp(buf + 8, selected_server, 8) == 0) {
                        struct config_data *config;
                        config = parse_message(1, body, bodylen, interfaces);
                        if(!config_data) {
                            /* Something went wrong, such as the config script
                               failing.  Reset everything, but set a large
                               timeout. */
                            server_hopcount = 0;
                            memset(selected_server, 0, 8);
                            SWITCH(STATE_INIT);
                            set_timeout(MESSAGE, 30000, 1);
                        } else if(config) {
                            if((af & 1) && config->ipv4_address)
                                prefix_list_extract4(last_ipv4,
                                                     config->ipv4_address);
                            server_hopcount = buf[3];
                            free_config_data(config);
                            SWITCH(STATE_BOUND);
                        } else {
                            fprintf(stderr,
                                    "Eek!  Configured, but config null.\n");
                        }
                    } else if(body[0] == AHCP_NACK &&
                              memcmp(buf + 8, selected_server, 8) == 0) {
                        if(config_data)
                            unconfigure(interfaces);
                        server_hopcount = 0;
                        memset(selected_server, 0, 8);
                        SWITCH(STATE_INIT);
                    }
                    break;
                case STATE_BOUND:
                    break;
                default:
                    abort();
                }

#ifndef NO_SERVER
                if(server_config) {
                    if(body[0] == AHCP_DISCOVER ||
                       body[0] == AHCP_REQUEST ||
                       body[0] == AHCP_RELEASE) {
                        struct config_data *config;
                        unsigned client_lease_time;
                        unsigned char reply[BUFFER_SIZE];
                        int hopcount;
                        unsigned char ipv4[4] = {0};

                        config = parse_message(-1, body, bodylen, interfaces);
                        if(!config) {
                            fprintf(stderr, "Unparseable client message.\n");
                            continue;
                        }

                        hopcount = buf[3];
                        if(config->ipv4_address)
                            prefix_list_extract4(ipv4, config->ipv4_address);
                        client_lease_time =
                            config->expires ?
                            config->expires + roughly(120) :
                            4 * 3600 + roughly(120);

                        free_config_data(config);

                        if(body[0] == AHCP_RELEASE) {
                            if(memcmp(ipv4, zeroes, 4) != 0) {
                                rc = release_lease(buf + 8, 8, ipv4);
                                if(rc < 0) {
                                    char a[INET_ADDRSTRLEN];
                                    inet_ntop(AF_INET, ipv4, a,
                                              INET_ADDRSTRLEN);
                                    fprintf(stderr,
                                            "Couldn't release lease for %s.\n",
                                            a);
                                }
                            }
                            continue;
                        }

                        if((config->ipv4_mandatory &&
                            !server_config->lease_first[0]) ||
                           (config->ipv6_mandatory &&
                            !server_config->ipv6_prefix) ||
                           config->ipv4_delegation_mandatory ||
                           config->ipv6_delegation_mandatory) {
                            /* We won't be able to satisfy the client's
                               mandatory constraints. */
                                rc = -1;
                        } else if(server_config->lease_first[0] &&
                                  config->ipv4_address) {
                            rc = take_lease(buf + 8, 8,
                                            memcmp(ipv4, zeroes, 4) == 0 ?
                                            ipv4 : NULL,
                                            ipv4, &client_lease_time,
                                            body[0] == AHCP_REQUEST);
                        } else {
                            rc = 0;
                        }

                        /* If the client is in the initial state, there's
                           no point in notifying it about failures -- it
                           will time out and fall back to another server */
                        if(rc < 0 && body[0] == AHCP_DISCOVER)
                                continue;

                        config =
                            make_config_data(client_lease_time,
                                             ipv4, server_config,
                                             interfaces);
                        if(config == NULL) {
                            fprintf(stderr, "Couldn't build config data.\n");
                            continue;
                        }

                        rc = server_body(rc < 0 ? AHCP_NACK :
                                         body[0] == AHCP_DISCOVER ?
                                         AHCP_OFFER : AHCP_ACK,
                                         config, reply, BUFFER_SIZE);
                        if(rc < 0) {
                            fprintf(stderr, "Couldn't build reply.\n");
                        } else {
                            debugf(2, "Sending %d (%d bytes, %d hops).\n",
                                   reply[0], rc, hopcount);
                            usleep(roughly(50000));
                            send_packet(psin, sinlen, buf + 8, hopcount,
                                        reply, rc);
                            gettime(&now, NULL);
                        }
                        
                        free_config_data(config);

                    }
                }
#endif
            }
        }

        if(config_data) {
            if(now.tv_sec >= config_data->expires_m) {
                unconfigure(interfaces);
                server_hopcount = 0;
                SWITCH(STATE_INIT);
            }
        }

        if(state == STATE_BOUND) {
            if(now.tv_sec >= config_renew_time())
                SWITCH(STATE_RENEWING_UNICAST);
        }

        if(message_time.tv_sec > 0 && now.tv_sec >= message_time.tv_sec) {
            unsigned char buf[BUFFER_SIZE];
            int i;
            switch(state) {
            case STATE_IDLE:
                fprintf(stderr, "Attempted to send message in IDLE state.\n");
                set_timeout(MESSAGE, 0, 1);
                break;
            case STATE_INIT:
                server_hopcount++;
                i = query_body(AHCP_DISCOVER, lease_time + roughly(120),
                               memcmp(last_ipv4, zeroes, 4) ? NULL : last_ipv4,
                               buf, BUFFER_SIZE);
                if(i >= 0) {
                    debugf(2, "Sending %d (%d bytes, %d hops).\n",
                           buf[0], i, server_hopcount);
                    send_packet(NULL, 0, NULL, server_hopcount, buf, i);
                } else {
                    fprintf(stderr, "Couldn't build body.\n");
                }
                count++;
                set_timeout(MESSAGE, 1000 * count, 1);
                break;
            case STATE_RENEWING:
                server_hopcount++;
                /* Fall through */
            case STATE_REQUESTING:
            case STATE_RENEWING_UNICAST:
                i = query_body(AHCP_REQUEST, lease_time + roughly(120),
                               memcmp(last_ipv4, zeroes, 4) ? NULL : last_ipv4,
                               buf, BUFFER_SIZE);
                if(i < 0) {
                    fprintf(stderr, "Couldn't build body.\n");
                    break;
                }
                if(state == STATE_REQUESTING && count > 7) {
                    debugf(2, "Giving up on request.\n");
                    server_hopcount = 0;
                    memset(selected_server, 0, 8);
                    SWITCH(STATE_INIT);
                } else if(state == STATE_RENEWING_UNICAST) {
                    debugf(2, "Sending %d (%d bytes, unicast)\n",
                           buf[0], i);
                    rc = send_unicast_packet(selected_server, config_data,
                                             count / 3,
                                             buf, i);
                    if(rc == 0) {
                        SWITCH(STATE_RENEWING);
                    } else {
                        /* A failure probably indicates we couldn't route
                           to this address, no point in delaying. */
                        count++;
                        set_timeout(MESSAGE, rc < 0 ? 300 : 10000, 1);
                    }
                } else {
                    debugf(2, "Sending %d (%d bytes, %d hops).\n",
                           buf[0], i, server_hopcount);
                    send_packet(NULL, 0, selected_server, server_hopcount,
                                buf, i);
                    count++;
                    /* At this point we have a fair idea of the server
                       hopcount, so we should get a reply in one, at most
                       two requests.  Use generous timeouts. */
                    set_timeout(MESSAGE,
                                state == STATE_REQUESTING ?
                                2000 * count : 10000 * count, 1);
                }
                break;
            case STATE_BOUND:
                fprintf(stderr, "Attempted to send message in BOUND state.\n");
                set_timeout(MESSAGE, 0, 1);
                break;
            default: abort();
            }
        }

        if(check_networks_time.tv_sec > 0 &&
           timeval_compare(&check_networks_time, &now) <= 0) {
            for(i = 0; i < numnetworks; i++)
                check_network(&networks[i]);
            set_timeout(CHECK_NETWORKS, 30000, 1);
        }
    }

    /* Clean up */

    if(config_data) {
        unsigned char buf[BUFFER_SIZE];
        int len;
        len = query_body(AHCP_RELEASE, 0,
                         memcmp(last_ipv4, zeroes, 4) ? NULL : last_ipv4,
                         buf, BUFFER_SIZE);
        if(len >= 0) {
            int index = 0;
            int success = 0;
            while(1) {
                debugf(2, "Sending %d (%d bytes, unicast).\n",  buf[0], i);
                rc = send_unicast_packet(selected_server, config_data, index,
                                         buf, len);
                if(rc > 0) {
                    success = 1;
                    break;
                } else if(rc == 0) {
                    break;
                }
                index++;
            }

            if(!success) {
                debugf(2, "Sending %d (%d bytes, %d hops).\n",
                       buf[0], i, server_hopcount);
                send_packet(NULL, 0, NULL, server_hopcount + 2, buf, len);
            }
        }
        unconfigure(interfaces);
        SWITCH(STATE_INIT);
    }

    if(pidfile)
        unlink(pidfile);
    return 0;

 usage:
    fprintf(stderr,
            "Syntax: ahcpd "
            "[-m group] [-p port] [-n] [-4] [-6] [-N]\n"
            "              "
            "[-i file] [-s script] [-D] [-I pidfile] [-L logfile]\n"
            "              "
            "[-C statement] [-c filename]"
            "interface...\n");
    exit(1);

 fail:
    if(pidfile)
        unlink(pidfile);
    exit(1);
}

unsigned
roughly(unsigned value)
{
    return value * 3 / 4 + random() % (value / 4);
}

static void
set_timeout(int which, int msecs, int override)
{
    struct timeval *tv;
    int ms = msecs == 0 ? 0 : roughly(msecs);
    switch(which) {
    case MESSAGE: tv = &message_time; break;
    case CHECK_NETWORKS: tv = &check_networks_time; break;
    default: abort();
    }

    /* (0, 0) represents never */
    if(override || tv->tv_sec == 0 || tv->tv_sec > now.tv_sec + ms / 1000) {
        if(msecs <= 0) {
            tv->tv_usec = 0;
            tv->tv_sec = 0;
        } else {
            tv->tv_usec =
                (now.tv_usec + ms * 1000) % 1000000 + random() % 1000;
            tv->tv_sec = now.tv_sec + (now.tv_usec / 1000 + ms) / 1000;
        }
    }
}

static int
check_network(struct network *net)
{
    int ifindex, rc;
    struct ipv6_mreq mreq;

    ifindex = if_nametoindex(net->ifname);
    if(ifindex != net->ifindex) {
        net->ifindex = ifindex;
        if(net->ifindex > 0) {
            memset(&mreq, 0, sizeof(mreq));
            memcpy(&mreq.ipv6mr_multiaddr, &protocol_group, 16);
            mreq.ipv6mr_interface = net->ifindex;
            rc = setsockopt(protocol_socket, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                            (char*)&mreq, sizeof(mreq));
            if(rc < 0) {
                perror("setsockopt(IPV6_JOIN_GROUP)");
                net->ifindex = 0;
                goto fail;
            }
            return 1;
        }
    }
 fail:
    return 0;
}

static void
sigexit(int signo)
{
    exiting = 1;
}

static void
sigdump(int signo)
{
    dumping = 1;
}

static void
sigchanged(int signo)
{
    changed = 1;
}

static void
init_signals(void)
{
    struct sigaction sa;
    sigset_t ss;

    sigemptyset(&ss);
    sa.sa_handler = sigexit;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);

    sigemptyset(&ss);
    sa.sa_handler = sigexit;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGHUP, &sa, NULL);

    sigemptyset(&ss);
    sa.sa_handler = sigexit;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    sigemptyset(&ss);
    sa.sa_handler = sigdump;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);

    sigemptyset(&ss);
    sa.sa_handler = sigchanged;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);

#ifdef SIGINFO
    sigemptyset(&ss);
    sa.sa_handler = sigdump;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    sigaction(SIGINFO, &sa, NULL);
#endif
}

int
ahcp_socket(int port)
{
    struct sockaddr_in6 sin6;
    int s, rc;
    int saved_errno;
    int one = 1, zero = 0;

    s = socket(PF_INET6, SOCK_DGRAM, 0);
    if(s < 0)
        return -1;

    rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if(rc < 0)
        perror("setsockopt(SO_REUSEADDR)");

    rc = setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
                    &zero, sizeof(zero));
    if(rc < 0)
        perror("setsockopt(IPV6_MULTICAST_LOOP)");

    rc = setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
                    &one, sizeof(one));
    if(rc < 0)
        perror("setsockopt(IPV6_MULTICAST_HOPS)");

#ifdef IPV6_V6ONLY
    rc = setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY,
                    &zero, sizeof(zero));
    if(rc < 0)
        perror("setsockopt(IPV6_V6ONLY)");
#endif

    rc = fcntl(s, F_GETFD, 0);
    if(rc < 0)
        goto fail;

    rc = fcntl(s, F_SETFD, rc | FD_CLOEXEC);
    if(rc < 0)
        goto fail;

    rc = fcntl(s, F_GETFL, 0);
    if(rc < 0)
        goto fail;

    rc = fcntl(s, F_SETFL, (rc | O_NONBLOCK));
    if(rc < 0)
        goto fail;

    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    sin6.sin6_port = htons(port);
    rc = bind(s, (struct sockaddr*)&sin6, sizeof(sin6));
    if(rc < 0)
        goto fail;

    return s;

 fail:
    saved_errno = errno;
    close(s);
    errno = saved_errno;
    return -1;
}

int
ahcp_recv(int s, void *buf, int buflen, struct sockaddr *sin, int slen)
{
    struct iovec iovec;
    struct msghdr msg;
    int rc;

    memset(&msg, 0, sizeof(msg));
    iovec.iov_base = buf;
    iovec.iov_len = buflen;
    msg.msg_name = sin;
    msg.msg_namelen = slen;
    msg.msg_iov = &iovec;
    msg.msg_iovlen = 1;

    rc = recvmsg(s, &msg, 0);
    return rc;
}

/* Attempts to contact the server over unicast, on its indexth address.
   Returns 1 if a packet was successfully sent, 0 if index was beyond the
   number of addresses, -1 in case of failure to send the packet. */

static int
send_unicast_packet(unsigned char *server_id, struct config_data *config,
                    int index, void *buf, int buflen)
{
    int rc;
    unsigned char *address;
    struct sockaddr_in6 sin;
    int num6 = config->server_ipv6 ? config->server_ipv6->n : 0;
    int num4 = config->server_ipv4 ? config->server_ipv4->n : 0;

    if(!config_data)
        return 0;

    if(index < num6)
        address = config->server_ipv6->l[index].p;
    else if(index < num6 + num4)
        address = config->server_ipv4->l[index - num6].p;
    else
        return 0;

    memset(&sin, 0, sizeof(sin));
    sin.sin6_family = AF_INET6;
    memcpy(&sin.sin6_addr, address, 16);
    sin.sin6_port = htons(protocol_port);
    rc = send_packet((struct sockaddr*)&sin, sizeof(sin), server_id, 1,
                     buf, buflen);
    return (rc >= 0) ? 1 : -1;
}

int
timeval_compare(const struct timeval *s1, const struct timeval *s2)
{
    if(s1->tv_sec < s2->tv_sec)
        return -1;
    else if(s1->tv_sec > s2->tv_sec)
        return 1;
    else if(s1->tv_usec < s2->tv_usec)
        return -1;
    else if(s1->tv_usec > s2->tv_usec)
        return 1;
    else
        return 0;
}

/* {0, 0} represents infinity */
void
timeval_min(struct timeval *d, const struct timeval *s)
{
    if(s->tv_sec == 0)
        return;

    if(d->tv_sec == 0 || timeval_compare(d, s) > 0) {
        *d = *s;
    }
}

void
timeval_minus(struct timeval *d,
              const struct timeval *s1, const struct timeval *s2)
{
    if(s1->tv_usec >= s2->tv_usec) {
        d->tv_usec = s1->tv_usec - s2->tv_usec;
        d->tv_sec = s1->tv_sec - s2->tv_sec;
    } else {
        d->tv_usec = s1->tv_usec + 1000000 - s2->tv_usec;
        d->tv_sec = s1->tv_sec - s2->tv_sec - 1;
    }
}

void
timeval_min_sec(struct timeval *d, int secs)
{
    if(d->tv_sec == 0 || d->tv_sec > secs) {
        d->tv_sec = secs;
        d->tv_usec = random() % 1000000;
    }
}

static int
reopen_logfile()
{
    int lfd, rc;

    if(logfile == NULL)
        return 0;

    lfd = open(logfile, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if(lfd < 0)
        return -1;

    fflush(stdout);
    fflush(stderr);

    rc = dup2(lfd, 1);
    if(rc < 0)
        return -1;

    rc = dup2(lfd, 2);
    if(rc < 0)
        return -1;

    if(lfd == 0 || lfd > 2)
        close(lfd);

    return 1;
}

static int
daemonise()
{
    int rc;

    fflush(stdout);
    fflush(stderr);

    rc = fork();
    if(rc < 0)
        return -1;

    if(rc > 0)
        exit(0);

    rc = setsid();
    if(rc < 0)
        return -1;

    return 1;
}

void
do_debugf(int level, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    if(debug >= level) {
        vfprintf(stderr, format, args);
        fflush(stderr);
    }
    va_end(args);
}
