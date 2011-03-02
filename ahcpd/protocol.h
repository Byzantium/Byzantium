/*
Copyright (c) 2007-2009 by Juliusz Chroboczek

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

/* Opcodes */

#define AHCP_DISCOVER 0
#define AHCP_OFFER 1
#define AHCP_REQUEST 2
#define AHCP_ACK 3
#define AHCP_NACK 4
#define AHCP_RELEASE 5

/* Options */

#define OPT_PAD 0
#define OPT_MANDATORY 1
#define OPT_ORIGIN_TIME 2
#define OPT_EXPIRES 3
#define OPT_MY_IPv6_ADDRESS 4
#define OPT_MY_IPv4_ADDRESS 5
#define OPT_IPv6_PREFIX 6
#define OPT_IPv4_PREFIX 7
#define OPT_IPv6_ADDRESS 8
#define OPT_IPv4_ADDRESS 9
#define OPT_IPv6_PREFIX_DELEGATION 10
#define OPT_IPv4_PREFIX_DELEGATION 11
#define OPT_NAME_SERVER 12
#define OPT_NTP_SERVER 13



