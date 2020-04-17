/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2016-2019, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of CT800 (opening book tool
 *                              2nd pass, conversion).
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

void    Conv_Read_Input_Book_File(BOOK_POS *book_pos_ptr, int32_t *pos_cnt, FILE *book_file);
int32_t Conv_Write_Output_Bin_File(const BOOK_POS *book_pos_ptr, int32_t pos_cnt, FILE *out_file,
                               int32_t *unique_positions, int32_t *unique_moves, int32_t *max_moves_per_pos);
int32_t Conv_Write_Output_Include_File(FILE *include_file, uint8_t *bin_book_buffer, uint32_t length);
