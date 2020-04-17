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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "booktool.h"
#include "util.h"

/*2nd pass: reads an input line and adds it to the position list. if a move
is marked with a following '?' or 'x', it will not be added to the active
play, but will be passively there if the opponent plays it.*/
static void Conv_Read_Input_Book_Line(const char *line, int32_t line_number, BOOK_POS *book_pos_ptr, int32_t *pos_cnt)
{
    int32_t line_len, line_index = 0, from, to;
    int32_t epsquare;
    BOARD_POS bpos;
    char move[5];
    
    /*the input format should be plain ASCII. Or UTF-8 without BOM, which is the same
    for the legal characters. But it's also easy to accomodate to UTF-8 with BOM:
    Just check the first three characters of the first line. If these are
    0xef 0xbb 0xbf, then this is the UTF-8 with BOM starter. Just jump over that,
    and we'll be fine.
    
    OK, UTF-8 may insert multibyte characters in the comments, but these are ignored
    anyway.
    
    Since the line buffer is initialised to all 0 before reading the first line,
    there is no need to check the line length for avoiding uninitialised access.*/
    
    if (line_number == 1)
    {
        uint8_t file_start[3];
        /*depending on the target platform, char might or might not be signed. Just
        interpret it as uint8_t, but avoid potential issues with pointer aliasing.
        using memcpy is one of the clean ways to do that.*/
        memcpy(file_start, line, 3);
        
        /*if we got the UTF-8 BOM, just skip that.*/
        if ((file_start[0] == 0xEFU) && (file_start[1] == 0xBBU) && (file_start[2] == 0xBFU))
            line += 3;
    }
    
    /*set initial board position at the start of the input line.*/
    Util_Set_Start_Pos(&bpos, &epsquare);
    
    /*skip over initial white spaces*/
    while (Util_Is_Whitespace(line[line_index]))
        line_index++;

    line_len = strlen(line);
    
    /*line without moves*/
    if ((line_len < line_index + 4) || (Util_Is_Commentline(line[line_index])) || (Util_Is_Line_End(line[line_index])))
        return;
    
    /*give a zero termination to the move buffer*/
    move[4] = '\0';

    while (line_index < line_len)
    {
        /*get the current move. the zero termination has already happened
          before entering this loop.*/
        move[0] = line[line_index++];
        move[1] = line[line_index++];
        move[2] = line[line_index++];
        move[3] = line[line_index++];
        
        /*get the indices format*/
        Util_Move_Conv(move, &from, &to);

        if (!Util_Is_Passivemarker(line[line_index]))
        /*add that move actively.*/
        {
            uint64_t crc40;
            crc40 = Util_Crc32(&(bpos.board), sizeof(bpos.board));
            crc40 <<= 8;
            crc40 |= Util_Crc8(&(bpos.board), sizeof(bpos.board));
            book_pos_ptr[*pos_cnt].crc40 = crc40;
            book_pos_ptr[*pos_cnt].move.mv.from = from;
            book_pos_ptr[*pos_cnt].move.mv.to   = to;
            (*pos_cnt)++;
        } else
            line_index++;

        /*play the move on the board*/
        Util_Move_Do(&bpos, &epsquare, from, to);

        /*skip over whitespaces*/
        while (Util_Is_Whitespace(line[line_index]))
            line_index++;

        /*the end of the line might be reached*/
        if (Util_Is_Line_End(line[line_index]))
             return;
    }
    return;
}

/*2nd pass: that's used for retrieving a CRC from the opening book.*/
static uint32_t Conv_Hex_Long_To_Int(const uint8_t *buffer)
{
    uint32_t ret;

    ret = *buffer++;
    ret = (ret << 8) | *buffer++;
    ret = (ret << 8) | *buffer++;
    ret = (ret << 8) | *buffer;

    return(ret);
}

/*2nd pass: finds out where the CRC32 ranges of the book start in order to
speed up the searching in the firmware. the point here is that the opening
book is already sorted by ascending CRC.
note that the CRC8 doesn't play a role here.
the output data are put into book_crc_index_cache[].*/
static void Conv_Book_Get_Opening_Ranges(uint8_t *bin_book_buffer, uint32_t length, uint32_t *book_crc_index_cache)
{
    uint32_t scan_crc, scan_crc_diff, line_crc, fileend, i;

    /*at least another crc (4 bytes) and the length field (1 byte) must be ahead,
    or else the file end has been reached.*/
    fileend = length - (sizeof(uint32_t) + sizeof(uint8_t));
    scan_crc_diff = (1UL << (32 - BOOK_INDEX_CACHE_BITS));
    for (i = 0, scan_crc = 0; i < BOOK_INDEX_CACHE_SIZE; i++, scan_crc += scan_crc_diff)
    {
        uint32_t fileindex = 0, lastindex = 0;
        do
        {
            /*start of the line: get the CRC32 out*/
            line_crc = Conv_Hex_Long_To_Int(bin_book_buffer+fileindex);

            if (line_crc >= scan_crc) /*we found the desired CRC32!*/
            {
                book_crc_index_cache[i] = fileindex;
                break;
            }

            lastindex = fileindex;
            
            fileindex += sizeof(uint32_t); /*past the crc*/
            
            /*so many moves follow in this line, plus the length byte itself.
            the length information is only in the low-nibble of the length byte.*/
            fileindex += (bin_book_buffer[fileindex] & 0x0FU) * 2U + sizeof(uint8_t);
        } while ((line_crc < scan_crc) && (fileindex < fileend));
        
        if (fileindex >= fileend) /*CRC not found at the end*/
            book_crc_index_cache[i] = lastindex;
    }
}

/*2nd pass: reads the input file and evaluates every line for the position
format.*/
void Conv_Read_Input_Book_File(BOOK_POS *book_pos_ptr, int32_t *pos_cnt, FILE *book_file)
{
    int32_t line_number = 1;
    char book_line[BOOK_LINE_LEN + 1]; /*+1 for 0 termination*/
    
    /*be sure to have a proper initialisation for the first line - used for
    checking the UTF-8 encoding.*/
    (void) memset(book_line, 0, sizeof(book_line));
    
    while (fgets(book_line, BOOK_LINE_LEN, book_file))
    {
        book_line[BOOK_LINE_LEN] = '\0'; /*be sure to have 0 termination*/
        Conv_Read_Input_Book_Line(book_line, line_number, book_pos_ptr, pos_cnt);
        line_number++;
    }
}

static enum E_FILE_OP 
Conv_Flush_Bin_Line(FILE *out_file, uint8_t *line, uint8_t moves_per_pos, uint8_t crc8)
{
    if (moves_per_pos > 0)
    {
        uint8_t length_byte;
        size_t num_bytes;

        if (moves_per_pos > MOVES_PER_POS)
        {
            /*this should not happen - just for robustness*/
            fprintf(stderr, "WARNING: more than %d moves per position. line shortened.\r\n", MOVES_PER_POS);
            moves_per_pos = MOVES_PER_POS;
        }
        
        /*fiddle in the CRC8 - that's a bit of a hack!
        CRC8 bits:
        7 6 5 4 => high nibble of the length byte
        3 2     => highest 2 bits of the first move byte
        1 0     => highest 2 bits of the second move byte*/
        length_byte = moves_per_pos | (crc8 & 0xF0U);
        line[sizeof(uint32_t)] = length_byte;   
        line[sizeof(uint32_t) + 1] |= (crc8 << 4) & 0xC0u;
        line[sizeof(uint32_t) + 2] |= (crc8 << 6) & 0xC0u;
        
        num_bytes = moves_per_pos * 2;
        num_bytes += sizeof(uint32_t) + sizeof (uint8_t); /*plus CRC and length byte*/
        
        /*write the moves*/
        if (num_bytes != fwrite(line, sizeof(uint8_t), num_bytes, out_file))
            return(FILE_OP_ERROR); /*write failed*/
    } else /*this should not happen - just for robustness*/
        fprintf(stderr, "WARNING: discarded empty move line.\r\n");
    return(FILE_OP_OK);
}

/*checks whether a list contains a certain move.*/
static int32_t Conv_Move_In_List(const MOVE *list_ptr, const MOVE *end_ptr, MOVE checkmove)
{
    while (list_ptr < end_ptr)
    {
        if (list_ptr->mv_blob == checkmove.mv_blob)
            return(1);
        list_ptr++;
    }
    return(0);
}

/*2nd pass: writes the binary output file*/
int32_t Conv_Write_Output_Bin_File(const BOOK_POS *book_pos_ptr, int32_t pos_cnt, FILE *out_file,
                           int32_t *unique_positions, int32_t *unique_moves, int32_t *max_moves_per_pos)
{
    int32_t i, first_line = 1;
    uint64_t last_crc40 = 0;
    
    /*CRC32 plus length byte plus two bytes per move
      (from+to square in binary notation)*/
    uint8_t line[sizeof(uint32_t) + sizeof(uint8_t) + MOVES_PER_POS*2];
    
    /*the move line for an individual position, containing
      the unique moves for said position.*/
    MOVE pos_line[MOVES_PER_POS];
    
    /*the move from the raw move list that is being examined.*/
    MOVE move_buffer;
    
    /*how many moves have been recorded for the current position*/
    uint8_t moves_per_pos;

    /*initialise the info variables*/
    *unique_positions = 0;
    *unique_moves = 0;
    
    memset(pos_line, 0, sizeof(pos_line));

    /*for every raw position*/
    for (i = 0, moves_per_pos = 0; i < pos_cnt; i++)
    {
        uint64_t current_crc40;
        current_crc40 = book_pos_ptr[i].crc40;

        if (first_line) /*first line of the output file*/
        {
            /*get the first CRC32, bytewise.
            the lowbyte is the CRC8, saved separately in the line flush routine.*/
            line[0] = (uint8_t) (((current_crc40) >> 32) & 0xFFU);
            line[1] = (uint8_t) (((current_crc40) >> 24) & 0xFFU);
            line[2] = (uint8_t) (((current_crc40) >> 16) & 0xFFU);
            line[3] = (uint8_t) (((current_crc40) >>  8) & 0xFFU);

            last_crc40 = current_crc40;
            first_line = 0; /*first line is over*/
        }
        if (current_crc40 != last_crc40) /*new line! flush out the previous one.*/
        {
            (*unique_positions)++; /*a new position has started*/
            
            if (Conv_Flush_Bin_Line(out_file, line, moves_per_pos, (uint8_t)(last_crc40 & 0xFFU)) != FILE_OP_OK)
                return(FILE_OP_ERROR);

            moves_per_pos = 0; /*reset position move counter*/

            /*fill in the new line CRC - bytewise to avoid potential endianess issues*/
            line[0] = (uint8_t) (((current_crc40) >> 32) & 0xFFU);
            line[1] = (uint8_t) (((current_crc40) >> 24) & 0xFFU);
            line[2] = (uint8_t) (((current_crc40) >> 16) & 0xFFU);
            line[3] = (uint8_t) (((current_crc40) >>  8) & 0xFFU);
        }

        /*fetch current move from the position list*/
        move_buffer.mv_blob = book_pos_ptr[i].move.mv_blob;

        /*is the move already listed for this position CRC?*/
        if (Conv_Move_In_List(pos_line, &pos_line[moves_per_pos], move_buffer) == 0)
        {
            /*no, it isn't, so include it.*/
            if (moves_per_pos < MOVES_PER_POS) /*avoid buffer overflow*/
            {
                (*unique_moves)++; /*for the statistics*/
                
                /*add the move to the move list for this position*/
                pos_line[moves_per_pos].mv_blob = move_buffer.mv_blob;
                
                /*the move data start after the CRC32 and the length byte*/
                line[sizeof(uint32_t) + sizeof(uint8_t) + moves_per_pos*2    ] = move_buffer.mv.from;
                line[sizeof(uint32_t) + sizeof(uint8_t) + moves_per_pos*2 + 1] = move_buffer.mv.to;

                moves_per_pos++;
                if (moves_per_pos > (*max_moves_per_pos)) /*for the statistics*/
                    (*max_moves_per_pos) = moves_per_pos;
            } else
            {
                fprintf(stderr, "WARNING: moves per position dimensioning is insufficient!\r\n");
                fprintf(stderr, "WARNING: Only recorded %d moves for this position.\r\n", moves_per_pos);
            }

        }

        last_crc40 = current_crc40;
    }

    /*flush out the last line*/
    if (Conv_Flush_Bin_Line(out_file, line, moves_per_pos,(uint8_t)(last_crc40 & 0xFFU)) != FILE_OP_OK)
        return(FILE_OP_ERROR);
    
    (*unique_positions)++; /*this last position is also unique, for the statistics*/
    
    return(FILE_OP_OK);
}


/*2nd pass: make sure that the build process actually results in an opening book*/
static const uint8_t w_format_id[FORMAT_ID_LEN]= {
    0xFFU, 0x33U, 0x76U, 0x6CU,
    0x70U, 0x67U, 0x66U, 0x66U,
    0x6FU, 0x68U, 0x74U, 0x6CU,
    0x61U, 0x5FU, 0x72U, 0x00U
};

/*2nd pass: writes the actual C style include ASCII file.*/
int32_t Conv_Write_Output_Include_File(FILE *include_file, uint8_t *bin_book_buffer, uint32_t length)
{
    uint32_t book_crc_index_cache[BOOK_INDEX_CACHE_SIZE];
    char line_buffer[80];
    char conv_buffer[20];
    char cache_index_format[7];
       
    uint32_t i, j;
    int ret; /*for checking the result of the file write operations*/
    
    ret = fputs("/***************************************************\r\n", include_file);
    if (ret < 0) return(FILE_OP_ERROR);
    ret = fputs("* this is the CT800 opening book in binary format. *\r\n", include_file);
    if (ret < 0) return(FILE_OP_ERROR);
    sprintf(line_buffer, "* generated using the opening book tool %s.     *\r\n", BOOKTOOL_VERSION);
    ret = fputs(line_buffer, include_file);
    if (ret < 0) return(FILE_OP_ERROR);
    ret = fputs("***************************************************/\r\n\r\n", include_file);
    if (ret < 0) return(FILE_OP_ERROR);

    /*declaration of the book array - name is fixed*/
    ret = fputs("static FLASH_ROM const uint8_t ctbook_crc_dat[] = {", include_file);
    if (ret < 0) return(FILE_OP_ERROR);

    /*book array data*/
    for (i = 0; i < length; i++)
    {
        if ((i % 10) == 0)
        {
            ret = fputs("\r\n  ", include_file);
            if (ret < 0) return(FILE_OP_ERROR);
        }
        sprintf(conv_buffer, "0x%02XU,", bin_book_buffer[i]);
        if (((i+1) % 10) != 0) /*no trailing space at the line end*/
            strcat(conv_buffer, " ");
        ret = fputs(conv_buffer, include_file);
        if (ret < 0) return(FILE_OP_ERROR);
    }

    /*format ID*/
    for (j = 0; j < FORMAT_ID_LEN; j++, i++)
    {
        if ((i % 10) == 0) /*12 numbers per line*/
        {
            ret = fputs("\r\n  ", include_file);
            if (ret < 0) return(FILE_OP_ERROR);
        }
        sprintf(conv_buffer, "0x%02XU", w_format_id[j]);
        if (j < (FORMAT_ID_LEN - 1)) /*more data follow*/
        {
            strcat(conv_buffer, ",");
            if (((i+1) % 10) != 0) /*no trailing space at the line end*/
                strcat(conv_buffer, " ");
        }
        ret = fputs(conv_buffer, include_file);
        if (ret < 0) return(FILE_OP_ERROR);
    }

    /*end of book array*/
    ret = fputs("\r\n};\r\n\r\n", include_file);
    if (ret < 0) return(FILE_OP_ERROR);

    /*the length of the actual opening book without counting the format ID.
    the point here is that the application may also ignore the format info,
    that's why it's at the end. However, different opening book formats can
    easily be supported.*/
    sprintf(line_buffer, "static FLASH_ROM const uint32_t ctbook_crc_dat_len = %"PRIu32"UL;\r\n\r\n", length);
    ret = fputs(line_buffer, include_file);
    if (ret < 0) return(FILE_OP_ERROR);
    
    /*now for the index cache entries.*/
    
    /*the scan shift will be needed in the CT800 for looking up the
      cached indices.*/
    sprintf(line_buffer, "#define BOOK_SCAN_CRC_SHIFT %uU\r\n\r\n", 32 - BOOK_INDEX_CACHE_BITS);
    ret = fputs(line_buffer, include_file);
    if (ret < 0) return(FILE_OP_ERROR);
    
    /*and the index cache table itself.*/
    sprintf(line_buffer, "static FLASH_ROM const uint32_t book_crc_index_cache[%"PRIu32"] = {", (uint32_t) BOOK_INDEX_CACHE_SIZE);
    ret = fputs(line_buffer, include_file);
    if (ret < 0) return(FILE_OP_ERROR);
    
    memset(book_crc_index_cache, 0, sizeof(book_crc_index_cache));
    Conv_Book_Get_Opening_Ranges(bin_book_buffer, length, book_crc_index_cache);
    
    /*how much digits do the cache indices really have?*/
    strcpy(cache_index_format, "%7luUL");
    if (length < 10UL)      {cache_index_format[1] = '1';} else
    if (length < 100UL)     {cache_index_format[1] = '2';} else
    if (length < 1000UL)    {cache_index_format[1] = '3';} else
    if (length < 10000UL)   {cache_index_format[1] = '4';} else
    if (length < 100000UL)  {cache_index_format[1] = '5';} else
    if (length < 1000000UL) {cache_index_format[1] = '6';}
    
    /*index cache array data*/
    for (i = 0; i < BOOK_INDEX_CACHE_SIZE; i++)
    {
        if ((i % 8) == 0) /*8 numbers per line*/
        {
            ret = fputs("\r\n  ", include_file); /*new line*/
            if (ret < 0) return(FILE_OP_ERROR);
        }
        
        sprintf(conv_buffer, cache_index_format, (unsigned long int) book_crc_index_cache[i]);

        if (i < (BOOK_INDEX_CACHE_SIZE - 1)) /*more data follow*/
        {
            strcat(conv_buffer, ",");
            if (((i+1) % 8) != 0) /*no trailing space at the line end*/
                strcat(conv_buffer, " "); /*space separator*/
         }
        ret = fputs(conv_buffer, include_file);
        if (ret < 0) return(FILE_OP_ERROR);
    }
    
    /*end of index cache array*/
    ret = fputs("\r\n};\r\n", include_file);
    if (ret < 0) return(FILE_OP_ERROR);
    
    return(FILE_OP_OK);
}
