/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2016-2019, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of CT800 (opening book tool main file).
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

/* this tool checks a line based ASCII opening book for legal moves
and generates a position hash (CRC32) based opening text file.
Besides ASCII, also UTF-8 with and without BOM are supported for
the input file.

some remarks:

required input file format is ASCII text consisting of N1 lines, each line
like this:

e2e4 e7e5 g1f3 b8c6 f1b5 (Spanish Opening)

En passant or queening promotion are recognised automatically, i.e. special
move markup is neither necessary nor allowed. Underpromotions are not
possible.

you can mark a move with a following '?' so that the program passively knows
how to continue if the opponent plays that move, but it will never play that
move by itself. another marker with the same consequence is 'x' instead
of '?', which is being used to mark moves that actually are playable, but
avoided because the line results in positions that don't suit the software.

e.g. if you don't want the program to play King's Gambit, but tell it how to
react in case the opponent plays it:

e2e4 e7e5 f2f4? ...

comments at the line end MUST start with '('.
if a line contains only a comment, then the first character must be '#'.

resulting output file format is BINARY consisting of N2 lines like this:

0x758f3477NaAbBcC

the first four bytes are the CRC, most significant byte first. the CRC is
written byte-wise so that endianess doesn't play a role.

N is a composed number. the upper four bits are additional CRC bits while
the lower four bits contain the number of moves to follow.

xX are the possible moves, the first byte (x) designating the "from" field,
the second (X) the to field. these are unsigned chars.
the first two move bytes contain each additional two bits of the CRC.
combined with the bits embedded into the length byte, this is the additional
CRC-8. the advantage is that the CRC-8 does not occupy additional space.

If you give the inputfile "example.txt", the program will generate
"example_crc.dat".

the input file is checked for being in line with the chess rules, that is
the main part of this program code.

this is a 2-pass compiler: the first pass validates the input file, and if
no errors are present, the second pass actually converts the line based
format into a position based one.

each function is marked whether it is part of the 1st, the 2nd or of both
passes.

you will notice that unlike the CT800 firmware, this booktool is using the
C standard library functions a lot, including calloc and printf. that's
because this tool is meant to be running on a PC and not embedded. so there
is no reason to avoid them, and using them saves a lot of pointless effort.

only the sorting avoids the library qsort; this way, the binary output will
always be the same, no matter what compiler or platform. shellsort is still
good enough at a 100,000 list elements.*/

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "booktool.h"
#include "check.h"
#include "convert.h"
#include "util.h"

/*the main function that glues everything together:
1st pass, 2nd pass and resource management.*/
int main(int argc, char* argv[])
{
    int32_t move_cnt = 0, errors = 0, pos_cnt, i, unique_positions, unique_moves, max_moves_per_pos = 0;
    int32_t book_length, bytes_read, line_number = 1, ret = 0;
    uint32_t verbose = 0;
    FILE *book_file = NULL;
    FILE *include_file = NULL;
    BOOK_POS *book_pos_ptr = NULL;
    uint8_t *buffer = NULL;
    double collision_prob;
    char filename[520];
    char include_filename[520];

    fprintf(stdout, "\r\nCT800 opening book compiler %s\r\n\r\n", BOOKTOOL_VERSION);
    
    /*check verbosity level*/
    if ((argc >= 2) && (strcmp(argv[1], "-v") == 0))
        verbose = 1;
    else if ((argc >= 3) && (strcmp(argv[2], "-v") == 0))
        verbose = 1;

    if (argc < 2) /*no input file given: change to interactive mode*/
    {
        verbose = 1; /*always verbose in interactive mode*/
        memset(filename, 0, sizeof(filename));
        fprintf(stderr, "\r\nenter the name of the book text file: ");
        fgets(filename, 510, stdin);
        filename[510] = '\0';
        i = strlen(filename);
        if (i > 0)
        {
            /*cut off a potential CR/LF at the end.*/
            if ((filename[i - 1] == '\r') || (filename[i - 1] == '\n'))
            {
                filename[i - 1] = '\0';
                i--;
                if (i > 0)
                    if ((filename[i - 1] == '\r') || (filename[i - 1] == '\n') )
                        filename[i - 1] = '\0';
            }
        }
    } else /*input file given*/
    {
        strncpy(filename, argv[argc-1], 510);
        filename[510] = '\0';
        i = strlen(filename);
        if (i > 0)
        {
            /*cut off a potential CR/LF at the end
            - that can happen if called from within a shell script.*/
            if ((filename[i - 1] == '\r') || (filename[i - 1] == '\n'))
            {
                filename[i - 1] = '\0';
                i--;
                if (i > 0)
                    if ((filename[i - 1] == '\r') || (filename[i - 1] == '\n') )
                        filename[i - 1] = '\0';
            }
        }
    }

    book_file = fopen_safe(filename, "r", book_file, verbose); /*open the input book text file*/
    if (book_file == NULL)
    {
        fprintf(stderr, "ERROR: file %s not found. no book written.\r\n", filename);
        ret = -1;
        goto end_of_func;
    }

    if (verbose) fprintf(stdout, "INFO: starting scan.\r\n");

    /*verify the input file*/
    Check_Input_Book_File(&errors, &move_cnt, &line_number, book_file);
    line_number--;

    fprintf(stdout, "INFO: finished scan: %"PRId32" lines, %"PRId32" OK moves, %"PRId32" errors.\r\n", line_number, move_cnt, errors);

    if (move_cnt == 0) /*empty input file?!*/
    {
        fprintf(stderr, "ERROR: no moves found. no book written.\r\n");
        fclose_safe(&book_file, verbose);
        ret = -2;
        goto end_of_func;
    }
    
    if (errors > 0) /*don't generate the output file if there have been errors in the input file*/
    {
        fprintf(stderr, "ERROR: errors found. no book written.\r\n");
        fclose_safe(&book_file, verbose);
        ret = -3;
        goto end_of_func;
    }
    
    /*rewind input file pointer to start.*/
    if (fseek(book_file, 0L, SEEK_SET) != 0)
    {
        fprintf(stderr, "ERROR: file %s not readable. no result file written.\r\n", filename);
        fclose_safe(&book_file, verbose);
        ret = -4;
        goto end_of_func;
    }

    /*second pass: no errors, and we know how many moves have to be processed.*/

    /*allocate memory: every raw move gets a position CRC32*/ 
    book_pos_ptr = (BOOK_POS *) alloc_safe(move_cnt, sizeof(BOOK_POS), book_pos_ptr, verbose);
    if (book_pos_ptr == NULL)
    {
        fprintf(stderr, "ERROR: memory insufficient. no book written.\r\n");
        fclose_safe(&book_file, verbose);
        ret = -5;
        goto end_of_func;
    }
    
    /*info about memory allocation*/
    if (verbose)
    {
       uint64_t num_bytes;
        
        fprintf(stdout, "INFO: temporary memory usage: ");
        
        num_bytes = ((uint64_t) move_cnt) * ((uint64_t) sizeof(BOOK_POS));
        
        if (num_bytes > 1024ULL * 10ULL) /*above 10k: convert to kb*/
        {
            num_bytes /= 1024ULL;
            if (num_bytes > 1024ULL * 10ULL) /*above 10M: convert to Mb*/
            {
                num_bytes /= 1024ULL;
                fprintf(stdout, "%"PRIu64" Mb", num_bytes);
            } else /*in kb*/
                fprintf(stdout, "%"PRIu64" kb", num_bytes);
        } else /*in bytes*/
            fprintf(stdout, "%"PRIu64" bytes", num_bytes);
        
        fprintf(stdout, "\r\n");
    }

    pos_cnt = 0;

    if (verbose) fprintf(stdout, "INFO: starting read.\r\n");

    /*read the input file and associate every move with a position CRC32*/
    Conv_Read_Input_Book_File(book_pos_ptr, &pos_cnt, book_file);

    fprintf(stdout, "INFO: finished read: %d raw positions.\r\n", pos_cnt);

    /*close the text input file, not needed anymore*/
    fclose_safe(&book_file, verbose);

    if (pos_cnt == 0) /*no positions found?!*/
    {
        fprintf(stderr, "ERROR: no positions found. no book written.\r\n");
        free_safe(&book_pos_ptr, verbose);
        ret = -6;
        goto end_of_func;
    }

    if (verbose) fprintf(stdout, "INFO: sorting...");
    fflush(stdout);

    /*sort the moves by their CRC in ascending order.
     ascending order is important as that's exploited in CT800 FW, book.c*/
    Util_Sort_Moves(book_pos_ptr, pos_cnt);

    if (verbose) fprintf(stdout, " done.\r\n");

    /*generate a filename for the output file.
    if "example.txt" has been the input file name,
    then "example_crc.dat" will be the output file name.*/

    strcpy(include_filename, filename); /*save the original filename*/
    
    i = strlen(filename);
    if (i > 4)
        if (filename[i - 4] == '.')
            filename[i - 4] = '\0';

    strcat(filename, "_crc.dat");

    /*that's the output binary file*/
    book_file = fopen_safe(filename, "wb+", book_file, verbose);
    if (book_file == NULL)
    {
        fprintf(stderr, "ERROR: file %s not writable. no book written.\r\n", filename);
        free_safe(&book_pos_ptr, verbose);
        ret = -7;
        goto end_of_func;
    }

    if (verbose) fprintf(stdout, "INFO: writing temporary file: %s\r\n", filename);

    /*write the output file to disk*/
    if (Conv_Write_Output_Bin_File(book_pos_ptr, pos_cnt, book_file,
                                   &unique_positions, &unique_moves,
                                   &max_moves_per_pos) != FILE_OP_OK)
    {
        fprintf(stderr, "ERROR: file %s not writable. no book written.\r\n", filename);
        free_safe(&book_pos_ptr, verbose);
        fclose_safe(&book_file, verbose);
        ret = -8;
        goto end_of_func;
    }            
    
    /*the raw position/move buffer is not needed anymore.*/
    free_safe(&book_pos_ptr, verbose);

    /*length of the binary temporary book file*/
    book_length = ftell(book_file);  
    if (book_length <= 0)
    {
        fprintf(stderr, "ERROR: file %s not readable. no book written.\r\n", filename);
        fclose_safe(&book_file, verbose);
        ret = -9;
        goto end_of_func;
    }
    
    /*rewind temporary binary file pointer to start.*/
    if (fseek(book_file, 0L, SEEK_SET) != 0)
    {
        fprintf(stderr, "ERROR: file %s not readable. no result file written.\r\n", filename);
        fclose_safe(&book_file, verbose);
        ret = -10;
        goto end_of_func;
    }

    if (verbose) fprintf(stdout,"INFO: max. number of moves per position: %d\r\n", max_moves_per_pos);
    fprintf(stdout, "INFO: number of unique plies / positions: %d / %d\r\n", unique_moves, unique_positions);
    
    /*calculate the probability of a hash (CRC32) collision in the opening book.
    the math here is the same as for the "birthday paradoxon".
    
    a collision isn't a (serious) problem because the opening moves are not
    taken right from the book. instead, the list of available moves is matched
    against the suggestions from the opening book. an illegal move in the book
    cannot have a match and will not be suggested. only if moves from the
    colliding position were also legal in the real position, unintended and
    probably bad moves would be suggested. then again, the redundancy of the chess
    rules makes this quite unlikely - this starts with the colour to move.
    
    0xFFFFFFFFFF because that is the maximum 40 bit number, which is exactly
    the number of different CRC32 + CRC8 hashes.*/
    if (verbose)
    {
        uint32_t cnt;
        for (cnt = 0, collision_prob = 1.0; cnt < unique_positions / 2UL; cnt++)
            collision_prob *= ((double)(0xFFFFFFFFFFULL - cnt)) / ((double ) 0xFFFFFFFFFFULL);

        fprintf(stdout, "INFO: probability for CRC-40 colour collision: %.3f%%\r\n", (1.0 - collision_prob)*100.0);
        
        for (; cnt < (uint32_t) unique_positions; cnt++)
            collision_prob *= ((double)(0xFFFFFFFFFFULL - cnt)) / ((double ) 0xFFFFFFFFFFULL);

        fprintf(stdout, "INFO: probability for CRC-40 global collision: %.3f%%\r\n", (1.0 - collision_prob)*100.0);
    }

    i = strlen(include_filename); /*that's a copy of the input filename at this point*/
    if (i > 4)
        if (include_filename[i - 4] == '.')
            include_filename[i - 4] = '\0';

    strcat(include_filename, ".c");

    /*that's the output result file*/
    include_file = fopen_safe(include_filename, "wb", include_file, verbose);
    if (include_file == NULL)
    {
        fprintf(stderr, "ERROR: file %s not writable. no result file written.\r\n", include_filename);
        fclose_safe(&book_file, verbose);
        ret = -11;
        goto end_of_func;
    }

    /*allocate memory for the result file*/
    buffer = (uint8_t *) alloc_safe(book_length, sizeof(uint8_t), buffer, verbose);

    if (buffer == NULL)
    {
        fprintf(stderr, "ERROR: memory insufficient. no result file written.\r\n");
        fclose_safe(&include_file, verbose);
        fclose_safe(&book_file, verbose);
        ret = -12;
        goto end_of_func;
    }

    bytes_read = fread(buffer, sizeof(uint8_t), book_length, book_file);
    fclose_safe(&book_file, verbose); /*temporary binary file is not needed anymore*/

    if (bytes_read != book_length)
    {
        fprintf(stderr, "ERROR: file %s not readable. no result file written.\r\n", filename);
        free_safe(&buffer, verbose);
        fclose_safe(&include_file, verbose);
        ret = -13;
        goto end_of_func;
    }
    
    if (verbose) fprintf(stdout, "INFO: deleting temporary file: %s\r\n", filename);
    /*the temporary file has been read already.*/
    
    /*it doesn't matter whether the file deletion fails,
      but give a hint to the user.*/
    if (remove(filename) != 0)
        fprintf(stderr, "INFO: cannot delete temporary file %s.\r\n", filename);

    /*convert to include file*/
    if (Conv_Write_Output_Include_File(include_file, buffer,
                                      (uint32_t) book_length) != FILE_OP_OK)
    {
        fprintf(stderr, "ERROR: file %s not writable. no result file written.\r\n", include_filename);
        free_safe(&buffer, verbose);
        fclose_safe(&include_file, verbose);
        /*try to remove the include file - it is corrupted anyway.*/
        (void) remove(include_filename);
        ret = -14;
        goto end_of_func;
    }

    /*for the statistics*/
    book_length += sizeof(uint32_t) + FORMAT_ID_LEN + BOOK_INDEX_CACHE_SIZE * sizeof(uint32_t);
    fprintf(stdout, "SUCCESS: %"PRId32" bytes written to result file: %s\r\n", book_length, include_filename);

    /*free the allocated memory and close the open output file before exiting.*/
    free_safe(&buffer, verbose);
    fclose_safe(&include_file, verbose);
    
end_of_func:
    /*make sure that every exit checks possible resource leakage. OK, the
    program is ending here anyway, but to demonstrate how to deal with
    resource leakage. if there is resource leakage, a warning will be
    displayed, and the resource will be freed.
    "goto" is perfectly good programming style if used for error handling
    with cleanup operations at the end; it avoids a lot of code duplication
    and unnecessary indentation, making the coder shorter, easier to read
    and easier to maintain.
    just make sure that it isn't used in the algorithm flow, and that the
    jumps only go forward. then "goto" isn't spaghetti code, quite the
    opposite.*/
    leak_safe(&book_pos_ptr, TYPE_MEM, verbose);
    leak_safe(&buffer, TYPE_MEM, verbose);
    leak_safe(&book_file, TYPE_FILE, verbose);
    leak_safe(&include_file, TYPE_FILE, verbose);
    
    return(ret);
}
