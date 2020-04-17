/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2016-2019, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of CT800 (opening book tool utility functions).
 *
 *  CT800 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  CT800 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CT800. If not, see <http://www.gnu.org/licenses/>.
 *
*/

uint32_t Util_Crc32(const void *buffer, size_t len);
uint8_t  Util_Crc8(const void *buffer, size_t len);
void     Util_Sort_Moves(BOOK_POS *poslist, size_t N);
void     Util_Set_Start_Pos(BOARD_POS *bpos, int32_t *epsquare);
void     Util_Move_Conv(const char *move, int32_t *from, int32_t *to);
int32_t  Util_Is_Line_End(char line_char);
int32_t  Util_Is_Whitespace(char line_char);
int32_t  Util_Is_Passivemarker(char line_char);
int32_t  Util_Is_Commentline(char line_char);
void     Util_Move_Do(BOARD_POS *bpos, int32_t *epsquare, int32_t from, int32_t to);

/*the resource handling wrappers*/
#ifdef USE_RESOURCE_WRAPPERS
void *   alloc_safe_exec(size_t nitems, size_t size, void *buffer, char *filename, int32_t line, uint32_t verbosity);
void     free_safe_exec(void **buffer, char *filename, int32_t line, uint32_t verbosity);
FILE *   fopen_safe_exec(const char *openfilename, const char *openmode, FILE *fileptr, char *filename, int32_t line, uint32_t verbosity);
void     fclose_safe_exec(FILE **fileptr, char *filename, int32_t line, uint32_t verbosity);
void     leak_safe_exec(void **buffer, enum E_LEAKAGE type, char *filename, int32_t line, uint32_t verbosity);
#endif
