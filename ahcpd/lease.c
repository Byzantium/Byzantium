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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ahcpd.h"
#include "monotonic.h"
#include "lease.h"

#ifdef NO_SERVER

int
lease_init(const char *dir,
           const unsigned char *first, const unsigned char *last, int debug)
{
    return -1;
}

int
take_lease(const unsigned char *client_id, int client_len,
           const unsigned char *suggested_ipv4,
           unsigned char *ipv4_return, unsigned *lease_time, int commit)
{
    return -1;
}

int
release_lease(const unsigned char *client_id, int client_len,
              const unsigned char *ipv4)
{
    return -1;
}

#else

#define LEASE_GRACE_TIME 666
#define LEASE_PURGE_TIME (16 * 24 * 3600 + 666)

static unsigned int first_address = 0, last_address = 0;
const char *lease_directory = NULL;

/* A table mapping known IPs to leases.  If an entry is missing, everything
   is still safe, although we might be unable to give out leases in some
   cases; however, if it is incorrect, then we might incorrectly expire
   relative leases. */

#define MAX_LEASE_ENTRIES 16384

struct lease_entry {
    unsigned char *id;
    int id_len;
    unsigned address;
    unsigned lease_orig;        /* real time, 0 if unknown */
    unsigned lease_time;
    time_t lease_end_m;         /* monotonic time, may be negative if expired */
};

static struct lease_entry *entries = NULL;

static int numentries = 0;
static int maxentries = 0;

static unsigned char *
address_ipv4(unsigned a, unsigned char *ipv4)
{
    a = htonl(a);
    memcpy(ipv4, &a, 4);
    return ipv4;
}

static unsigned
ipv4_address(const unsigned char *ipv4)
{
    unsigned a;
    memcpy(&a, ipv4, 4);
    return ntohl(a);
}

static int
entry_match(struct lease_entry *entry, const unsigned char *id, int id_len)
{
    if(entry->id == NULL)
        return 0;

    return (entry->id_len == id_len && memcmp(entry->id, id, id_len) == 0);
}

static struct lease_entry *
find_entry(unsigned address)
{
    int i;
    for(i = 0; i < numentries; i++) {
        if(entries[i].address == address)
            return &entries[i];
    }
    return NULL;
}

static struct lease_entry *
find_entry_by_id(const unsigned char *id, int id_len)
{
    int i;
    for(i = 0; i < numentries; i++) {
        if(entry_match(&entries[i], id, id_len))
            return &entries[i];
    }
    return NULL;
}

static unsigned int
find_entryless(unsigned first, unsigned last)
{
    unsigned a;
    int i;

    for(a = first; a <= last; a++) {
        for(i = 0; i < numentries; i++) {
            if(entries[i].address == a)
                break;
        }
        if(i >= numentries)
            return a;
    }
    return 0;
}

struct lease_entry *
find_oldest_entry()
{
    int i, j = -1;
    unsigned age = 0, a;
    struct timeval now;

    gettime(&now, NULL);

    for(i = 0; i < numentries; i++) {
        if(entries[i].id == NULL)
            continue;
        a = now.tv_sec - entries[i].lease_end_m;
        if(a > age) {
            age = a;
            j = i;
        }
    }
    return j >= 0 ? &entries[j] : NULL;
}

static struct lease_entry *
add_entry(const unsigned char *id, int id_len, unsigned int address,
          unsigned lease_orig, unsigned lease_time, time_t lease_end_m)
{
    struct lease_entry *entry;
    int i;

    for(i = 0; i < numentries; i++) {
        if(entries[i].address == address) {
            if(!entry_match(&entries[i], id, id_len))
                return NULL;
            entry = &entries[i];
            goto done;
        }
    }

    entry = NULL;
    for(i = 0; i < numentries; i++) {
        if(entries[i].id == NULL) {
            entry = &entries[i];
            break;
        }
    }

    if(entry == NULL) {
        if(numentries >= maxentries) {
            if(maxentries < MAX_LEASE_ENTRIES) {
                int n = MIN(maxentries * 2, MAX_LEASE_ENTRIES);
                struct lease_entry *new =
                    realloc(entries, n * sizeof(struct lease_entry));
                if(new) {
                    entries = new;
                    maxentries = n;
                }
            }
        }
                
        if(numentries < maxentries)
            entry = &entries[numentries++];
    }

    if(entry == NULL) {
        entry = find_oldest_entry();
        if(entry == NULL)
            return NULL;

        free(entry->id);
        entry->id = NULL;
        entry->id_len = 0;
        entry->address = 0;
        entry->lease_orig = 0;
        entry->lease_time = 0;
        entry->lease_end_m = 0;
    }

    entry->id = malloc(id_len);
    if(entry->id == NULL)
        return NULL;
    memcpy(entry->id, id, id_len);
    entry->id_len = id_len;
    entry->address = address;

 done:
    entry->lease_orig = lease_orig;
    entry->lease_time = lease_time;
    entry->lease_end_m = lease_end_m;
    return entry;
}

static char *
lease_file(const unsigned char *ipv4, char *buf, int bufsize)
{
    const char *p;
    int n;

    n = strlen(lease_directory);
    if(n >= bufsize - 2)
        return NULL;

    memcpy(buf, lease_directory, n);
    buf[n++] = '/';

    p = inet_ntop(AF_INET, ipv4, buf + n, bufsize - n);
    if(p == NULL)
        return NULL;

    return buf;
}

static int
close_lease_file(int fd, int modified)
{
    int rc;

 again:
    if(modified) {
        rc = fsync(fd);
        if(rc < 0) {
            int save;
            if(errno == EINTR) goto again;
            save = errno;
            close(fd);
            errno = save;
            return -1;
        }
    }

    rc = close(fd);
    if(rc < 1 && errno == EINTR)
        goto again;
    return rc;
}

static int
read_lease_file(int fd, const unsigned char *ipv4,
                unsigned *lease_orig_return, unsigned *lease_time_return,
                unsigned char *ipv4_return,
                unsigned char *client_buf, int client_len)
{
    int rc, i;
    struct iovec iov[2];
    char head[20], name[INET_ADDRSTRLEN];
    unsigned lease_orig, lease_time;

    i = 0;
    iov[i].iov_base = head;
    iov[i++].iov_len = 20;
    if(client_len > 0) {
        iov[i].iov_base = client_buf;
        iov[i++].iov_len = client_len;
    }

    rc = readv(fd, iov, i);
    if(rc < 0) {
        perror("read(lease_file)");
        return -1;
    }

    if(rc < 20 || rc > 20 + client_len) {
        fprintf(stderr, "Truncated lease file for %s.\n", name);
        return -1;
    }

    if(memcmp("AHCP", head, 4) != 0) {
        fprintf(stderr, "Corrupted lease file for %s.\n", name);
        return -1;
    }

    if(memcmp("\1\0\0\0", head + 4, 4) != 0) {
        fprintf(stderr, "Lease file %s has wrong version.\n", name);
        return -1;
    }

    if(ipv4 && memcmp(ipv4, head + 8, 4) != 0) {
        fprintf(stderr, "Mismatched lease file for %s.\n", name);
        return -1;
    }

    memcpy(&lease_orig, head + 12, 4);
    lease_orig = ntohl(lease_orig);

    memcpy(&lease_time, head + 16, 4);
    lease_time = ntohl(lease_time);

    if(lease_orig_return)
        *lease_orig_return = lease_orig;
    if(lease_time_return)
        *lease_time_return = lease_time;
    if(ipv4_return)
        memcpy(ipv4_return, head + 8, 4);

    return rc - 20;
}

static int
write_lease_file(int fd, const unsigned char *ipv4,
                 unsigned lease_orig, unsigned lease_time,
                 const unsigned char *client_id, int client_len)
{
    struct iovec iov[5];
    int i;
    int rc;

    if(client_len > 650)
        return -1;

    lease_orig = htonl(lease_orig);
    lease_time = htonl(lease_time);

    i = 0;
    iov[i].iov_base = "AHCP\1\0\0\0";
    iov[i++].iov_len = 8;
    iov[i].iov_base = (void*)ipv4;
    iov[i++].iov_len = 4;
    iov[i].iov_base = &lease_orig;
    iov[i++].iov_len = 4;
    iov[i].iov_base = &lease_time;
    iov[i++].iov_len = 4;
    iov[i].iov_base = (void*)client_id;
    iov[i++].iov_len = client_len;

    rc = writev(fd, iov, i);
    if(rc < 20 + client_len) {
        perror("write(lease_file)");
        return -1;
    }

    return 1;
}

static int
update_lease_file(int fd, unsigned lease_orig, unsigned lease_time)
{
    off_t lrc;
    int rc, i;
    struct iovec iov[2];

    lease_orig = htonl(lease_orig);
    lease_time = htonl(lease_time);

    lrc = lseek(fd, 12, SEEK_SET);
    if(lrc < 0) {
        perror("lseek(lease_file)");
        return -1;
    }

    i = 0;
    iov[i].iov_base = &lease_orig;
    iov[i++].iov_len = 4;
    iov[i].iov_base = &lease_time;
    iov[i++].iov_len = 4;

    rc = writev(fd, iov, i);
    if(rc < 8) {
        perror("write(lease_file)");
        return -1;
    }

    return 1;
}

/* Return 1 if the file was removed. */

static int
purge_lease_file(char *fn, unsigned char *ipv4)
{
    int fd, rc;
    unsigned lease_orig, lease_time;
    struct timeval now, real;
    time_t stable;
    int clock_status;

    gettime(&now, &stable);
    get_real_time(&real, &clock_status);

    fd = open(fn, O_RDWR);
    if(fd < 0)
        return 0;

    rc = read_lease_file(fd, ipv4, &lease_orig, &lease_time, NULL, NULL, 0);
    if(rc < 0) {
        close_lease_file(fd, 0);
        return 0;
    }

    if(clock_status == CLOCK_TRUSTED && lease_orig > 0) {
        if(lease_orig + lease_time + LEASE_PURGE_TIME < real.tv_sec) {
            rc = unlink(fn);
            if(rc < 0) perror("unlink(lease_file)");
            close_lease_file(fd, 1);
            return (rc >= 0);
        }
    }

    close_lease_file(fd, 0);
    return 0;
}

/* Make a relative lease absolute. */
static int
mutate_lease(char *fn, const unsigned char *ipv4, struct lease_entry *entry)
{
    int fd;
    unsigned lease_orig, lease_time;
    int rc;
    struct timeval now, real;
    time_t stable;
    int clock_status;
    time_t orig;
    
    gettime(&now, &stable);
    get_real_time(&real, &clock_status);

    if(clock_status != CLOCK_TRUSTED)
        return -1;

    if(entry && entry->address != ipv4_address(ipv4)) {
        fprintf(stderr, "Entry mismatch when mutating!\n");
        return -1;
    }

    fd = open(fn, O_RDWR);
    if(fd < 0)
        return 0;

    rc = read_lease_file(fd, ipv4, &lease_orig, &lease_time, NULL, NULL, 0);
    if(rc < 0)
        goto fail;

    if(lease_orig != 0)
        goto fail;

    if(entry && stable >= lease_time)
        orig = real.tv_sec - (entry->lease_end_m - now.tv_sec);
    else
        orig = real.tv_sec;

    if(orig > 1000000000 && orig <= real.tv_sec + 300)
        lease_orig = orig;
    else
        lease_orig = real.tv_sec;

    rc = update_lease_file(fd, lease_orig, lease_time);
    if(rc < 0)
        goto fail;

    if(entry)
        entry->lease_orig = lease_orig;

    close_lease_file(fd, 1);
    return 1;

 fail:
    close_lease_file(fd, 0);
    return 0;
}

static int
lease_expired(const unsigned char *ipv4,
              unsigned lease_orig, unsigned lease_time)
{
    struct timeval now, real;
    time_t stable;
    int clock_status;
    struct lease_entry *entry;

    get_real_time(&real, &clock_status);

    if(clock_status == CLOCK_TRUSTED && lease_orig > 0)
        return lease_orig + LEASE_GRACE_TIME < real.tv_sec;

    gettime(&now, &stable);

    if(stable < lease_time)
        return 0;

    if(!ipv4)
        return 0;

    entry = find_entry(ipv4_address(ipv4));
    if(!entry)
        return 0;

    return entry->lease_end_m + LEASE_GRACE_TIME > now.tv_sec;
}

static int
get_lease(const unsigned char *client_id, int client_len,
          const unsigned char *ipv4, unsigned lease_time,
          int commit)
{
    unsigned char buf[512];
    char fn[256], *p;
    int fd, rc;
    unsigned lease_orig, old_orig, old_time;
    time_t lease_end_m;
    int clock_status;
    struct timeval now, real;

    get_real_time(&real, &clock_status);
    gettime(&now, NULL);

    lease_orig = clock_status == CLOCK_TRUSTED ? real.tv_sec : 0;
    lease_end_m = now.tv_sec + lease_time;

    p = lease_file(ipv4, fn, 256);
    if(p == NULL)
        return -1;

    fd = open(fn, commit ? O_RDWR : O_RDONLY);
    if(fd < 0) {
        if(errno == ENOENT) {
            if(commit)
                goto create;
            else
                return 1;
        }
        perror("open(lease_file)");
        return -1;
    }

    rc = read_lease_file(fd, ipv4, &old_orig, &old_time, NULL, buf, 512);
    if(rc < 0) {
        fprintf(stderr, "Couldn't read lease file.\n");
        goto fail;
    }

    if(rc == client_len && memcmp(buf, client_id, client_len) == 0) {
        struct lease_entry *entry;
        entry = find_entry(ipv4_address(ipv4));
        if(!entry || !entry_match(entry, client_id, client_len)) {
            fprintf(stderr, "Eek!  Inconsistent lease entry!\n");
            goto fail;
        }
        /* It would be unsafe to shorten this lease's time. */
        if(clock_status == CLOCK_TRUSTED && old_orig > 0 && lease_orig > 0)
            lease_time = MAX(lease_time, old_orig + old_time - lease_orig);
        else 
            lease_time = MAX(lease_time, entry->lease_end_m - now.tv_sec);
                           
        if(commit) {
            rc = update_lease_file(fd, lease_orig, lease_time);
            if(rc < 0)
                goto fail;
            entry->lease_orig = lease_orig;
            entry->lease_time = lease_time;
            entry->lease_end_m = lease_end_m;
        }
    } else {
        if(!lease_expired(ipv4, old_time, old_orig)) {
            if(old_orig == 0 && clock_status == CLOCK_TRUSTED)
                goto mutate;
            else
                goto fail;
        }

        if(commit) {
            rc = unlink(fn);
            if(rc < 0) {
                perror("unlink(lease_file)");
                goto fail;
            }
            close_lease_file(fd, 1);
            goto create;
        }
    }

    return close_lease_file(fd, commit);

 fail:
    close_lease_file(fd, 0);
    return -1;

 mutate:
    close_lease_file(fd, 0);
    mutate_lease(fn, ipv4, find_entry(ipv4_address(ipv4)));
    return -1;

 create:
    fd = open(fn, O_RDWR | O_CREAT | O_EXCL, 0644);
    if(fd < 0) {
        perror("creat(lease_file)");
        return -1;
    }

    rc = write_lease_file(fd, ipv4, lease_orig, lease_time,
                          client_id, client_len);
    if(rc < 0)
        goto fail;

    add_entry(client_id, client_len, ipv4_address(ipv4),
              lease_orig, lease_time, lease_end_m);

    return close_lease_file(fd, 1);
}

int
release_lease(const unsigned char *client_id, int client_len,
              const unsigned char *ipv4)
{
    unsigned char buf[512];
    char fn[256], *p;
    int fd, rc, clock_status;
    unsigned orig;
    struct timeval now, real;
    struct lease_entry *entry;

    if(first_address == 0 || lease_directory == NULL)
        return -1;

    p = lease_file(ipv4, fn, 256);
    if(p == NULL)
        return -1;

    fd = open(fn, O_RDWR);
    if(fd < 0) {
        perror("open(lease_file)");
        return -1;
    }

    rc = read_lease_file(fd, ipv4, NULL, NULL, NULL, buf, 512);
    if(rc < 0)
        goto fail;

    if(client_id) {
        if(rc != client_len || memcmp(buf, client_id, rc) != 0)
            goto fail;
    }

    gettime(&now, NULL);
    get_real_time(&real, &clock_status);

    if(clock_status == CLOCK_TRUSTED)
        orig = real.tv_sec;
    else
        orig = 0;

    rc = update_lease_file(fd, orig, 0);
    if(rc < 0) {
        rc = unlink(fn);
        if(rc < 0) {
            perror("unlink(lease_file)");
            goto fail;
        }
    }
    rc = close_lease_file(fd, 1);
    if(rc < 0)
        goto fail;

    entry = find_entry(ipv4_address(ipv4));
    entry->lease_orig = orig;
    entry->lease_time = 0;
    entry->lease_end_m = now.tv_sec;

    return 1;

 fail:
    close_lease_file(fd, 0);
    return -1;
}

int
lease_init(const char *dir,
           const unsigned char *first, const unsigned char *last, int debug)
{
    DIR *d;
    struct timeval now, real;
    int clock_status;
    unsigned fa, la;

    fa = ipv4_address(first);
    la = ipv4_address(last);

    if(fa <= 0x1000000 || fa >= la)
        return -1;

    entries = malloc(16 * sizeof(struct lease_entry));
    if(entries == NULL)
        return -1;
    numentries = 0;
    maxentries = 16;

    gettime(&now, NULL);
    get_real_time(&real, &clock_status);

    d = opendir(dir);
    if(d == NULL) {
        perror("open(lease_dir)");
        return -1;
    }

    while(1) {
        struct dirent *e;
        unsigned char ipv4[4], client_buf[512];
        char name[INET_ADDRSTRLEN], fn[256];
        const char *p;
        struct lease_entry *entry;
        unsigned lease_orig, lease_time;
        int fd, rc, len;

        e = readdir(d);
        if(e == NULL) break;
        if(e->d_name[0] == '.')
            continue;

        rc = snprintf(fn, 256, "%s/%s", dir, e->d_name);
        if(rc < 0 || rc >= 256) {
            fprintf(stderr, "Couldn't format filename %s/%s.\n",
                    dir, e->d_name);
            continue;
        }

        fd = open(fn, O_RDONLY);
        if(fd < 0) {
            fprintf(stderr, "Inaccessible lease file %s.\n", e->d_name);
            continue;
        }
        len = read_lease_file(fd, NULL, &lease_orig, &lease_time,
                              ipv4, client_buf, 512);
        close(fd);

        if(len < 0) {
            fprintf(stderr, "Corrupted lease file %s.\n", fn);
            continue;
        }

        p = inet_ntop(AF_INET, ipv4, name, INET_ADDRSTRLEN);
        if(p == NULL) {
            fprintf(stderr, "Couldn't format address.\n");
            continue;
        }

        if(strcmp(p, e->d_name) != 0) {
            fprintf(stderr, "Mis-named lease file %s (should be %s).\n",
                    fn, p);
            continue;
        }

        debugf(1, "Lease file %s: %u %u.\n",
               e->d_name, lease_orig, lease_time);

        if(clock_status == CLOCK_TRUSTED) {
            if(lease_expired(NULL, lease_orig, lease_time)) {
                rc = purge_lease_file(fn, ipv4);
                if(rc > 0)
                    continue;
            }
        }

        entry = add_entry(client_buf, len, ipv4_address(ipv4),
                          lease_orig, lease_time, now.tv_sec + lease_time);

        if(entry && clock_status == CLOCK_TRUSTED) {
            if(lease_orig == 0)
                mutate_lease(fn, ipv4, entry);
        }
    }
    closedir(d);

    if(numentries >= MAX_LEASE_ENTRIES) {
        fprintf(stderr, "Warning: lease index full.\n"
                "Perhaps you should recompile "
                "with a larger value for MAX_LEASE_ENTRIES?");
    }

    lease_directory = dir;
    first_address = fa;
    last_address = la;

    return 1;
}

int
take_lease(const unsigned char *client_id, int client_len,
           const unsigned char *suggested_ipv4,
           unsigned char *ipv4_return, unsigned *lease_time, int commit)
{
    unsigned int a, a0;
    unsigned time;
    struct lease_entry *entry;
    unsigned char ipv4[4];
    struct timeval now, real;
    int clock_status;
    time_t stable;

    if(first_address == 0 || lease_directory == NULL)
        return -1;

    if(client_len < 1)
        return -1;

    gettime(&now, &stable);
    get_real_time(&real, &clock_status);

    time = *lease_time;
    if(time > MAX_LEASE_TIME)
        time = MAX_LEASE_TIME;
    if(clock_status != CLOCK_TRUSTED && time > MAX_RELATIVE_LEASE_TIME)
        time = MAX_RELATIVE_LEASE_TIME;
    a0 = 0;

    /* Client suggested an IP.  If it is in range, try that. */
    if(suggested_ipv4) {
        a0 = ipv4_address(suggested_ipv4);
        entry = find_entry(a0);
        if(entry) {
            if(!entry_match(entry, client_id, client_len) &&
               !lease_expired(suggested_ipv4,
                              entry->lease_orig, entry->lease_time))
                a0 = 0;
        }
    }

    /* See if we have an old lease for this client. */
    if(a0 < first_address || a0 > last_address) {
        entry = find_entry_by_id(client_id, client_len);
        if(entry)
            a0 = entry->address;
    }

    /* Choose a free slot. */
    if(a0 < first_address || a0 > last_address)
        a0 = find_entryless(first_address, last_address);

    /* Choose the oldest slot. */
    if(a0 < first_address || a0 > last_address) {
        entry = find_oldest_entry();
        if(entry)
            a0 = entry->address;
    }

    /* Give up, take the first one. */
    if(a0 < first_address || a0 > last_address)
        a0 = first_address;

    /* Now scan all addresses in range sequentially, starting at a0. */
    a = a0;
    do {
        int rc;

        if(a > last_address)
            a = first_address;

        rc = get_lease(client_id, client_len, address_ipv4(a, ipv4),
                       time, commit);
        if(rc >= 0) {
            memcpy(ipv4_return, ipv4, 4);
            *lease_time = time;
            return 1;
        }
        a++;
    } while (a != a0);

    return -1;
}

#endif
