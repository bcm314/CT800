/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2016-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of the CT800 (utility functions).
 *
 *  CT800/NGPlay is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  CT800/NGPlay is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CT800/NGPlay. If not, see <http://www.gnu.org/licenses/>.
 *
 */

void        Util_Strcpy(char *dest, const char *src);
void        Util_Strcat(char *dest, const char *src);
void        Util_Strins(char *dest, const char *src);
int         Util_Strcmp(const char *str1, const char *str2);
size_t      Util_Strlen(const char *str);
void        Util_Time_To_String(char *buffer, int32_t intime, enum E_UT_LEAD_ZEROS leading_zeros, enum E_UT_ROUND rounding_time);
void        Util_Long_Time_To_String(char *buffer, int32_t intime, enum E_UT_ROUND rounding_time);
void        Util_Depth_To_String(char *buffer, int depth);
void        Util_Centipawns_To_String(char *buffer, int centipawns);
void        Util_Pawns_To_String(char *buffer, int material);
void        Util_Itoa(char *buffer, int number);
int         Util_Convert_Moves(MOVE m, char *buffer);
void        Util_Memzero(void *ptr, size_t n_bytes);
void        Util_Memcpy(void *dest, const void *src, size_t n_bytes);
void        Util_Movelinecpy(CMOVE *dest_ptr, const CMOVE *src_ptr, size_t n_moves);
char        Util_Key_To_Char(enum E_KEY key);
char        Util_Key_To_Digit(enum E_KEY key);
char        Util_Key_To_Prom(enum E_KEY key);
uint32_t    Util_Crc32(const void *buffer, size_t len);
uint8_t     Util_Crc8(const void *buffer, size_t len);
uint32_t    Util_Hex_Long_To_Int(const uint8_t *buffer);
void        Util_Long_Int_To_Hex(uint32_t value, char *buffer);
