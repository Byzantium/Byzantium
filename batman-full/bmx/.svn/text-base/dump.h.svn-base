/*
 * Copyright (c) 2010  Axel Neumann
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
 */



#define DUMP_ERROR_STR "DUMP_ERROR"
#define DUMP_MAX_STR_SIZE (MAX_DBG_STR_SIZE - strlen(DUMP_ERROR_STR))
#define DUMP_MAX_MSG_SIZE 200


#define DUMP_DIRECTION_OUT 0
#define DUMP_DIRECTION_IN 1
#define DUMP_DIRECTION_ARRSZ 2

#define DUMP_STATISTIC_PERIOD 1000

#define ARG_DUMP  "traffic"
#define ARG_DUMP_ALL     "all"
#define ARG_DUMP_DEV     "dev"
#define ARG_DUMP_SUMMARY "summary"


#define DEF_DUMP_REGRESSION_EXP 4
#define MIN_DUMP_REGRESSION_EXP 0
#define MAX_DUMP_REGRESSION_EXP 20
#define ARG_DUMP_REGRESSION_EXP "traffic_regression_exponent"


#define DUMP_TYPE_UDP_PAYLOAD   0
#define DUMP_TYPE_PACKET_HEADER 1
#define DUMP_TYPE_FRAME_HEADER  2
#define DUMP_TYPE_ARRSZ         3



struct dump_data {
	uint32_t tmp_frame[DUMP_DIRECTION_ARRSZ][FRAME_TYPE_ARRSZ];
	uint32_t pre_frame[DUMP_DIRECTION_ARRSZ][FRAME_TYPE_ARRSZ];
	uint32_t avg_frame[DUMP_DIRECTION_ARRSZ][FRAME_TYPE_ARRSZ];

	uint32_t tmp_all[DUMP_DIRECTION_ARRSZ][DUMP_TYPE_ARRSZ];
	uint32_t pre_all[DUMP_DIRECTION_ARRSZ][DUMP_TYPE_ARRSZ];
	uint32_t avg_all[DUMP_DIRECTION_ARRSZ][DUMP_TYPE_ARRSZ];
};

