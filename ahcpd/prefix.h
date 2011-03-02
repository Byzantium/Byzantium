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

#define IPv6_ADDRESS 0
#define IPv4_ADDRESS 1
#define ADDRESS 2
#define IPv6_PREFIX 3
#define IPv4_PREFIX 4
#define PREFIX 5

extern const unsigned char v4prefix[16];

/* This holds both IPv6 and IPv4, and both prefixes and addresses.  For
   IPv4, we use v4prefix above.  For addresses, plen is 0xFF. */

struct prefix {
    unsigned char p[16];
    unsigned char plen;
};

#define MAX_PREFIX 8

struct prefix_list {
    int n;
    struct prefix l[MAX_PREFIX];
};

void free_prefix_list(struct prefix_list *l);
struct prefix_list *copy_prefix_list(struct prefix_list *l);
int prefix_list_eq(struct prefix_list *l1, struct prefix_list *l2);
int prefix_list_v4(struct prefix_list *l);
void prefix_list_extract4(unsigned char *dest, struct prefix_list *p);
void prefix_list_extract6(unsigned char *dest, struct prefix_list *p);
struct prefix_list *raw_prefix_list(const unsigned char *data, int len,
                                    int kind);
char *format_prefix_list(struct prefix_list *p, int kind);
struct prefix_list *parse_prefix(char *address, int kind);
struct prefix_list *cat_prefix_list(struct prefix_list *p1,
                                    struct prefix_list *p2);
