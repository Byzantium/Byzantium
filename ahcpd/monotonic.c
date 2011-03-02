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

#define _GNU_SOURCE 1
#define _POSIX_C_SOURCE 200112L

#if defined(__linux__)
#define HAVE_ADJTIMEX
#endif

#if defined(__NetBSD__) || defined(__FreeBSD__)
#define HAVE_NTP_GETTIME
#endif

#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

#include "monotonic.h"

#if defined(__GNUC__) && (__GNUC__ >= 3)
#define UNLIKELY(_x) __builtin_expect(!!(_x), 0)
#else
#define UNLIKELY(_x) !!(_x)
#endif

/* Whether we use CLOCK_MONOTONIC */
static int have_posix_clocks = -1;

/* The clock status, broken, untrusted, or trusted. */
static int clock_status = CLOCK_BROKEN;

/* The (monotonic) time since when monotonic time has been stable.  If we
   have TIME_MONOTONIC, this is the time at which we inited. */
time_t clock_stable_time;

/* The last (monotonic) time we checked for time sync. */
time_t ntp_check_time = 0;

/* These variables are used for simulating monotonic time when we don't
   have CLOCK_MONOTONIC.  Offset is the offset to add to real time to get
   monotonic time; previous is the previous real time. */
static time_t offset, previous;

#if defined(HAVE_ADJTIMEX)

#include <sys/timex.h>

static int
ntp_sync(void)
{
    int rc;
    struct timex timex;

    timex.modes = 0;

    rc = adjtimex(&timex);
    return (rc >= 0 && rc != TIME_ERROR);
}

#elif defined(HAVE_NTP_GETTIME)

#include <sys/timex.h>

static int
ntp_sync(void)
{
    int rc;
    struct ntptimeval ntptv;

    rc = ntp_gettime(&ntptv);
    return (rc >= 0 && ntptv.time_state != TIME_ERROR);
}

#else

static int
ntp_sync(void)
{
    return 0;
}

#endif

static void
fix_clock(struct timeval *real)
{
    if(!have_posix_clocks) {
        if(previous > real->tv_sec) {
            offset += previous - real->tv_sec;
        } else if(previous + MAX_SLEEP < real->tv_sec) {
            offset += previous + MAX_SLEEP - real->tv_sec;
        }
    }

    if(real->tv_sec < 1200000000)
        clock_status = CLOCK_BROKEN;
    else if(ntp_sync())
        clock_status = CLOCK_TRUSTED;
    else
        clock_status = CLOCK_UNTRUSTED;
}

void
time_init()
{
    struct timeval now, real;
#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0 && defined(CLOCK_MONOTONIC)
    int rc;
    struct timespec ts;
    rc = clock_gettime(CLOCK_MONOTONIC, &ts);
    have_posix_clocks = (rc >= 0);
#else
    have_posix_clocks = 0;
#endif

    gettimeofday(&real, NULL);
    offset = 0;
    previous = real.tv_sec;
    fix_clock(&real);
    
    gettime(&now, NULL);
    clock_stable_time = now.tv_sec;
}

/* -1: no confirmation, just check NTP status.
    0: confirm that our time is broken.
    1: confirm that our time seems reasonable. */

void
time_confirm(int confirm)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    if(tv.tv_sec < 1200000000 || confirm == 0)
        clock_status = CLOCK_BROKEN;
    else if(ntp_sync())
        clock_status = CLOCK_TRUSTED;
    else if(confirm > 0)
        clock_status = CLOCK_UNTRUSTED;
}

int
get_real_time(struct timeval *tv, int *status_return)
{
    int rc;
    rc = gettimeofday(tv, NULL);
    if(rc < 0)
        return rc;

    if(UNLIKELY(tv->tv_sec < previous || tv->tv_sec > previous + MAX_SLEEP)) {
        fix_clock(tv);
    }
    previous = tv->tv_sec;

    if(status_return)
        *status_return = clock_status;
    return rc;
}

int
gettime(struct timeval *tv, time_t *stable)
{
    int rc;

#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0 && defined(CLOCK_MONOTONIC)
    if(have_posix_clocks) {
        struct timespec ts;

        rc = clock_gettime(CLOCK_MONOTONIC, &ts);
        if(rc < 0)
            return rc;
        tv->tv_sec = ts.tv_sec;
        tv->tv_usec = ts.tv_nsec / 1000;

        if(stable)
            *stable = tv->tv_sec - clock_stable_time;
    } else
#else
#warning No CLOCK_MONOTONIC on this platform
#endif
    {
        rc = get_real_time(tv, NULL);
        if(rc < 0)
            return rc;
        tv->tv_sec += offset;
    }

    if(stable)
        *stable = tv->tv_sec - clock_stable_time;

    if(UNLIKELY(tv->tv_sec - ntp_check_time > 3600)) {
        time_confirm(-1);
        ntp_check_time = tv->tv_sec;
    }

    return rc;
}
