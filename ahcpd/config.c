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
#include <arpa/inet.h>

#include "prefix.h"
#include "config.h"

int client_config = 1;

struct server_config *server_config = NULL;

/* get_next_char callback */
typedef int (*gnc_t)(void*);

static int
skip_whitespace(int c, gnc_t gnc, void *closure)
{
    while(c == ' ' || c == '\t')
        c = gnc(closure);
    return c;
}

static int
skip_to_eol(int c, gnc_t gnc, void *closure)
{
    while(c != '\n' && c >= 0)
        c = gnc(closure);
    if(c == '\n')
        c = gnc(closure);
    return c;
}

static int
skip_eol(int c, gnc_t gnc, void *closure)
{
    c = skip_whitespace(c, gnc, closure);
    if(c == '\n' || c == '#') {
        c = skip_to_eol(c, gnc, closure);
        return c;
    } else if(c == -1) {
        return -1;
    } else {
        return -2;
    }
}

static int
getword(int c, char **token_r, gnc_t gnc, void *closure)
{
    char buf[256];
    int i = 0;

    c = skip_whitespace(c, gnc, closure);
    if(c < 0)
        return c;
    if(c == '"' || c == '\n')
        return -2;
    do {
        if(i >= 255) return -2;
        buf[i++] = c;
        c = gnc(closure);
    } while(c != ' ' && c != '\t' && c != '\n' && c >= 0);
    buf[i] = '\0';
    *token_r = strdup(buf);
    return c;
}

static int
getstring(int c, char **token_r, gnc_t gnc, void *closure)
{
    char buf[256];
    int i = 0;

    c = skip_whitespace(c, gnc, closure);
    if(c < 0)
        return c;
    if(c == '\n')
        return -2;

    /* Unquoted strings have the same syntax as words. */
    if(c != '"')
        return getword(c, token_r, gnc, closure);

    c = gnc(closure);

    while(1) {
        if(i >= 255 || c == '\n') return -2;
        if(c == '"') {
            c = gnc(closure);
            break;
        }
        if(c == '\\')
            c = gnc(closure);
        buf[i++] = c;
        c = gnc(closure);
    }

    buf[i] = '\0';
    *token_r = strdup(buf);
    return c;
}

static int
parse_config(gnc_t gnc, void *closure)
{
    int c;
    char *token;

    c = gnc(closure);
    if(c < 2)
        return -1;

    while(c >= 0) {
        c = skip_whitespace(c, gnc, closure);
        if(c == '\n' || c == '#') {
            c = skip_to_eol(c, gnc, closure);
            continue;
        }
        if(c < 0)
            break;
        c = getword(c, &token, gnc, closure);
        if(c < -1)
            return -1;

        if(strcmp(token, "mode") == 0) {
            char *mtoken;

            c = getword(c, &mtoken, gnc, closure);
            if(c < -1)
                return -1;

            if(strcmp(mtoken, "server") == 0) {
#ifndef NO_SERVER
                client_config = 0;
                if(!server_config)
                    server_config = calloc(1, sizeof(struct server_config));
                if(!server_config)
                    return -1;
#else
                return -1;
#endif
            } else if(strcmp(mtoken, "client") == 0) {
                if(server_config)
                    return -1;
                client_config = 1;
            } else if(strcmp(mtoken, "forwarder") == 0) {
                if(server_config)
                    return -1;
                client_config = 0;
            } else {
                return -1;
            }

            free(mtoken);

            c = skip_eol(c, gnc, closure);
            if(c < -1)
                return -1;
        } else if(strcmp(token, "lease-dir") == 0) {
            char *dir;

            if(!server_config)
                return -1;

            c = getstring(c, &dir, gnc, closure);
            if(c < -1)
                return -1;

            if(dir[0] != '/')
                return -1;

            server_config->lease_dir = dir;
            c = skip_eol(c, gnc, closure);
            if(c < -1)
                return -1;
        } else if(strcmp(token, "prefix") == 0) {
            char *ptoken;
            struct prefix_list *prefix;

            if(!server_config)
                return -1;

            c = getword(c, &ptoken, gnc, closure);
            if(c < -1)
                return -1;

            prefix = parse_prefix(ptoken, PREFIX);
            
            if(prefix == NULL || prefix->n != 1)
                return -1;

            if(prefix_list_v4(prefix)) {
                unsigned const char zeroes[4] = {0};
                unsigned mask, first, last;
                if(memcmp(server_config->lease_first, zeroes, 4) != 0)
                    return -1;
                mask = 0xFFFFFFFF << (128 - prefix->l[0].plen);
                first =
                    (prefix->l[0].p[12] << 24 |
                     prefix->l[0].p[13] << 16 |
                     prefix->l[0].p[14] << 8 |
                     prefix->l[0].p[15] ) & mask;
                last = first | (~ mask);
                first = htonl(first + 1);
                last = htonl(last - 1);
                memcpy(server_config->lease_first, &first, 4);
                memcpy(server_config->lease_last, &last, 4);
                free(prefix);
            } else {
                server_config->ipv6_prefix =
                    cat_prefix_list(server_config->ipv6_prefix,
                                    prefix);
            }
            free(ptoken);
            c = skip_eol(c, gnc, closure);
            if(c < -1)
                return -1;
        } else if(strcmp(token, "name-server") == 0 ||
                  strcmp(token, "ntp-server") == 0) {
            char *ptoken;
            struct prefix_list *prefix;

            if(!server_config)
                return -1;

            c = getword(c, &ptoken, gnc, closure);
            if(c < -1)
                return -1;

            prefix = parse_prefix(ptoken, ADDRESS);

            if(prefix == NULL)
                return -1;

            if(strcmp(token, "name-server") == 0)
                server_config->name_server =
                    cat_prefix_list(server_config->name_server,
                                    prefix);
            else
                server_config->ntp_server =
                    cat_prefix_list(server_config->ntp_server,
                                    prefix);
            free(ptoken);
        } else {
            return -1;
        }
        free(token);
    }
    return 1;
}

int
parse_config_from_file(char *filename)
{
    FILE *f;
    int rc;

    f = fopen(filename, "r");
    if(f == NULL)
        return -1;
    rc = parse_config((gnc_t)fgetc, f);
    fclose(f);
    return rc;
}

struct string_state {
    char *string;
    int n;
};

static int
gnc_string(struct string_state *s)
{
    if(s->string[s->n] == '\0')
        return -1;
    else
        return s->string[s->n++];
}

int
parse_config_from_string(char *string)
{
    struct string_state s = { string, 0 };
    return parse_config((gnc_t)gnc_string, &s);
}
