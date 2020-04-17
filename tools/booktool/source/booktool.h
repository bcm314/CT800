/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2016-2019, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of CT800 (opening book tool definitions
 *                              and function prototypes).
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

#define BOOKTOOL_VERSION "V1.22"

/*a demo for enhanced robustness with manual resource management in C.*/
#define USE_RESOURCE_WRAPPERS

#ifdef USE_RESOURCE_WRAPPERS
    /*safe code: use resource wrappers to detect leakage.*/
    #define alloc_safe(nitems, size, buffer, verbosity)    alloc_safe_exec(nitems, size, (void *)(buffer), __FILE__, __LINE__, verbosity)
    #define free_safe(buffer, verbosity)                   free_safe_exec(((void **)(buffer)), __FILE__, __LINE__, verbosity)
    #define fopen_safe(filename, mode, fileptr, verbosity) fopen_safe_exec(filename, mode, fileptr, __FILE__, __LINE__, verbosity)
    #define fclose_safe(fileptr, verbosity)                fclose_safe_exec(fileptr, __FILE__, __LINE__, verbosity)
    #define leak_safe(buffer, type, verbosity)             leak_safe_exec((void **)(buffer), type, __FILE__, __LINE__, verbosity)
#else
    /*fast code: remap the wrappers to the standard library routines.*/
    #define alloc_safe(nitems, size, buffer, verbosity)    calloc(nitems, size)
    #define free_safe(buffer, verbosity)                   free(*(buffer))
    #define fopen_safe(filename, mode, fileptr, verbosity) fopen(filename, mode)
    #define fclose_safe(fileptr, verbosity)                fclose(*(fileptr))
    #define leak_safe(buffer, type, verbosity)
#endif 

/*for the file output routines*/
enum E_FILE_OP {FILE_OP_OK, FILE_OP_ERROR};

/*the maximum moves in a given position that this tool can handle.
currently, the maximum this opening book is using per position is 8 moves.
must not be greater than 15 because additional 4 CRC bits end up in
the high nibble of the length byte.*/
#define MOVES_PER_POS 15

#if (MOVES_PER_POS < 1)
    #error "MOVES_PER_POS must be at least 1."
#elif (MOVES_PER_POS > 15)
    #error "MOVES_PER_POS must not be greater than 15."
#endif

/*maximum characters per line in the book text file*/
#define BOOK_LINE_LEN 511

/*cache table size. minimum is 1, corresponding to 2^1, and maximum
is 16, corresponding to 2^16. the cache table must always be sized
at a power of two. 8U means 2^8=256 as table size.*/
#define BOOK_INDEX_CACHE_BITS 8U

#if (BOOK_INDEX_CACHE_BITS < 1UL)
    #error "BOOK_INDEX_CACHE_BITS must be at least 1."
#elif (BOOK_INDEX_CACHE_BITS > 16UL)
    #error "BOOK_INDEX_CACHE_BITS must not be greater than 16."
#endif

#define BOOK_INDEX_CACHE_SIZE (1UL << BOOK_INDEX_CACHE_BITS)

#define FORMAT_ID_LEN	16U

#define Abs(a)            (((a) > 0) ? (a) : -(a))

enum {NO_PIECE,WPAWN=2,WKNIGHT,WBISHOP,WROOK,WQUEEN,WKING,BPAWN=12,BKNIGHT,BBISHOP,BROOK,BQUEEN,BKING,PIECEMAX};

/*the enums map squares on the board to the board index format.*/
enum {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8
};

#define FILE_DIFF    1
#define RANK_DIFF    8
#define UP_RIGHT     (RANK_DIFF + FILE_DIFF)
#define UP_LEFT      (RANK_DIFF - FILE_DIFF)
#define FILE(square) (square % RANK_DIFF)
#define RANK(square) (square / RANK_DIFF)


enum {RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8};
enum {FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H};

#define STATUS_FLAGS 64
#define NOSQUARE     64

#define WKMOVED		1U
#define WRA1MOVED	2U
#define WRH1MOVED	4U
#define BKMOVED		8U
#define BRA8MOVED	16U
#define BRH8MOVED	32U
#define BLACK_MV	64U
#define FLAGS_RESET 0

/*eight compass directions for knight moves.
 e.g. NNW is north-north-west, like Ng1-f3.*/
#define KNIGHT_NNW ( 2*RANK_DIFF - FILE_DIFF)
#define KNIGHT_WNW ( RANK_DIFF - 2*FILE_DIFF)
#define KNIGHT_WSW (-RANK_DIFF - 2*FILE_DIFF)
#define KNIGHT_SSW (-2*RANK_DIFF - FILE_DIFF)
#define KNIGHT_SSE (-2*RANK_DIFF + FILE_DIFF)
#define KNIGHT_ESE (-RANK_DIFF + 2*FILE_DIFF)
#define KNIGHT_ENE ( RANK_DIFF + 2*FILE_DIFF)
#define KNIGHT_NNE (2*RANK_DIFF +  FILE_DIFF)


/*for the resource leakage type*/
enum E_LEAKAGE {TYPE_MEM, TYPE_FILE};

/* hold the board position. indexing of the sqares is sequentially from 0 to 63,
with A1 = 0 and H8=63. see the enums above.*/
typedef struct t_pos {
    uint8_t board[65]; /*holds the piece standing on a square or 0. a1 = index 0, h1 = index 7, h8 = index 63.
                                index 64 is the flags for castling rights.*/
} BOARD_POS;

/* bit 0  wkmoved    sample code:  if (xy1==E1) gflags |= 1;
   bit 1  wra1moved     >>         if (xy1==A1) gflags |= 2;
   bit 2  wrh1moved     >>         if (xy1==H1) gflags |= 4;
   bit 3  bkmoved       >>         if (xy1==E8) gflags |= 8;
   bit 4  bra8moved     >>         if (xy1==A8) gflags |= 16;
   bit 5  brh8moved     >>         if (xy1==H8) gflags |= 32;
   bit 6  Side to move, black has "to move bit" set   --> gflags |= 64;
                        white has "to move bit" reset --> gflags &= ~64;
*/

/*a single move for the raw move list*/
struct mvdata {
    uint8_t from;
    uint8_t to;
};

/* for this union, u must have the same size as the struct*/
typedef union {
  struct mvdata mv;
  uint16_t mv_blob;
} MOVE;

/*the struct for the book position. this is not to be confused with BOARD_POS:
BOARD_POS represents a full blown position on a chessboard, with all pieces.
BOOK_POS is an entry in the raw move list: if the CRC32 of the associated
board position (type BOARD_POS) is "crc", then the move "move" is possible.

it's a bit ugly because the struct gets 48 pad bytes. the move data could be
put into the upper 16 bits of the CRC, with some bit masking.
or there could be two separate lists CRCs and moves.
however, this tool runs on the host, not on the target, and the code would
become more convoluted than what it's worth.
besides, the "real" opening book takes 1828 kb (less than 2 MEGAbyte!) of
temporary RAM, and getting that down to 1142 kb isn't going to make a
difference on a PC.*/
typedef struct t_book_pos
{
    uint64_t crc40;
    MOVE move;
} BOOK_POS;
