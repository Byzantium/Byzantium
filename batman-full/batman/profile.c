/* 
 * Copyright (C) 2006-2009 B.A.T.M.A.N. contributors:
 *
 * Simon Wunderlich, Marek Lindner
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



#include "os.h"
#include "batman.h"



#if defined PROFILE_DATA


static struct prof_container prof_container[PROF_COUNT];



void prof_init(int32_t index, char *name) {

	prof_container[index].total_time = 0;
	prof_container[index].calls = 0;
	prof_container[index].name = name;

}



void prof_start(int32_t index) {

	prof_container[index].start_time = clock();

}



void prof_stop(int32_t index) {

	prof_container[index].calls++;
	prof_container[index].total_time += clock() - prof_container[index].start_time;

}


void prof_print(void) {

	int32_t index;

	debug_output( 5, " \nProfile data:\n" );

	for ( index = 0; index < PROF_COUNT; index++ ) {

		debug_output( 5, "   %''30s: cpu time = %10.3f, calls = %''10i, avg time per call = %4.10f \n", prof_container[index].name, (float)prof_container[index].total_time/CLOCKS_PER_SEC, prof_container[index].calls, ( prof_container[index].calls == 0 ? 0.0 : ( ( (float)prof_container[index].total_time/CLOCKS_PER_SEC ) / (float)prof_container[index].calls ) ) );

	}

}


#else


void prof_init( int32_t index, char *name ) {

}



void prof_start( int32_t index ) {

}



void prof_stop( int32_t index ) {

}


void prof_print(void) {

}


#endif
