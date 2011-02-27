/*
 * Copyright (C) 2006, 2007 BATMAN contributors:
 * Stefan Sperling <stsp@stsp.name>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */

/* This file contains functions that are used by batman but are not
 * present in BSD libc. */

#warning BSD support is known broken - if you compile this on BSD you are expected to fix it :-P

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

/* Adapted from busybox */
int vdprintf(int d, const char *format, va_list ap)
{
	char buf[1024];
	int len;

	len = vsnprintf(buf, sizeof(buf), format, ap);
	return write(d, buf, len);
}

/* From glibc */
int dprintf(int d, const char *format, ...)
{
  va_list arg;
  int done;

  va_start (arg, format);
  done = vdprintf (d, format, arg);
  va_end (arg);

  return done;
}

