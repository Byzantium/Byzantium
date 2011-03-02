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

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
/* nothing */
#elif defined(__GNUC__)
#define inline __inline
#if  (__GNUC__ >= 3)
#define restrict __restrict
#else
#define restrict /**/
#endif
#else
#define inline /**/
#define restrict /**/
#endif

#if defined(__GNUC__) && (__GNUC__ >= 3)
#define ATTRIBUTE(x) __attribute__ (x)
#define LIKELY(_x) __builtin_expect(!!(_x), 1)
#define UNLIKELY(_x) __builtin_expect(!!(_x), 0)
#else
#define ATTRIBUTE(x) /**/
#define LIKELY(_x) !!(_x)
#define UNLIKELY(_x) !!(_x)
#endif

#if defined(__GNUC__) && (__GNUC__ >= 4) && (__GNUC_MINOR__ >= 3)
#define COLD __attribute__ ((cold))
#else
#define COLD /**/
#endif

#undef MAX
#undef MIN

#ifdef HAVE_VALGRIND
#include <valgrind/memcheck.h>
#else
#ifndef VALGRIND_MAKE_MEM_UNDEFINED
#define VALGRIND_MAKE_MEM_UNDEFINED(a, b) do {} while(0)
#endif
#ifndef VALGRIND_CHECK_MEM_IS_DEFINED
#define VALGRIND_CHECK_MEM_IS_DEFINED(a, b) do {} while(0)
#endif
#endif

#define MAX(x,y) ((x)<=(y)?(y):(x))
#define MIN(x,y) ((x)<=(y)?(x):(y))

extern int nodns, af, request_prefix_delegation;
extern char *config_script;
extern int debug;
extern unsigned char myid[8];
extern int numnetworks;
struct network {
    char *ifname;
    int ifindex;
};

#define MAXNETWORKS 20
extern struct network networks[MAXNETWORKS];

extern struct in6_addr protocol_group;
extern unsigned int protocol_port;
extern int protocol_socket;

extern const unsigned char zeroes[16], ones[16];

void timeval_min(struct timeval *d, const struct timeval *s);
void timeval_min_sec(struct timeval *d, int secs);
void timeval_minus(struct timeval *d,
                   const struct timeval *s1, const struct timeval *s2);
int timeval_minus_msec(const struct timeval *s1, const struct timeval *s2);
void timeval_plus_msec(struct timeval *d,
                       const struct timeval *s, int msecs);
int timeval_compare(const struct timeval *s1, const struct timeval *s2);
int clock_stepped();
void do_debugf(int level, const char *format, ...)
    ATTRIBUTE ((format (printf, 2, 3))) COLD;

#if defined NO_DEBUG

#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
#define debugf(_level, ...) do {} while(0)
#elif defined __GNUC__
#define debugf(_level, _args...) do {} while(0)
#else
static inline void debugf(_level, const char *format, ...) { return; }
#endif

#else

#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
#define debugf(_level, ...)                      \
    do { \
        if(UNLIKELY(debug >= _level)) do_debugf(_level, __VA_ARGS__);     \
    } while(0)
#elif defined __GNUC__
#define debugf(_level, _args...)                 \
    do { \
        if(UNLIKELY(debug >= _level)) do_debugf(_level, _args);   \
    } while(0)
#else
static inline void debugf(int _level, const char *format, ...) { return; }
#endif

#endif
