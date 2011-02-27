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

#define XOR( a, b ) ( !(a) != !(b) )
#define IMPLIES( a, b ) ( !(a) || (b) )

#define LOG2(result, val_tmp, VAL_TYPE)                                         \
        do {                                                                    \
                uint8_t j = ((8 * sizeof (VAL_TYPE) ) >> 1 );                   \
                result = 0;                                                     \
                for (; val_tmp > 0x01; j >>= 1) {                               \
                        if (val_tmp >> j) {                                     \
                                result += j;                                    \
                                val_tmp >>= j;                                  \
                        }                                                       \
                }                                                               \
        } while(0)


/*
        for (i = ((8 * sizeof (UMETRIC_T)) / 2); i; i >>= 1) {

                if (tmp >> i) {
                        exp_sum += i;
                        tmp >>= i;
                }
        }
*/














char* memAsStr( const void* mem, const uint32_t len);

uint32_t rand_num(const uint32_t limit);

uint8_t bits_count(uint32_t v);

uint8_t bit_get(const uint8_t *array, const uint16_t array_bit_size, uint16_t bit);

void bit_set(uint8_t *array, uint16_t array_bit_size, uint16_t bit, IDM_T value);

void bit_clear(uint8_t *array, uint16_t array_bit_size, uint16_t begin, uint16_t end);

void byte_clear(uint8_t *array, uint16_t array_size, uint16_t begin, uint16_t range);

uint8_t is_zero(void *data, int len);


IDM_T str2netw(char* args, IPX_T *ipX, char delimiter, struct ctrl_node *cn, uint8_t *maskp, uint8_t *familyp);

int8_t wordsEqual ( char *a, char *b );
void wordCopy( char *out, char *in );
uint32_t wordlen ( char *s );
int32_t check_file( char *path, uint8_t write, uint8_t exec );
int32_t check_dir( char *path, uint8_t create, uint8_t write );



void init_tools(void);