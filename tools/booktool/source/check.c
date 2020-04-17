/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2016-2019, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of CT800 (opening book tool
 *                              1st pass, error checking).
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
#include "checkerr.h"

/*1st pass: checks whether a move consists of legal characters.*/
static int32_t Check_Move_Notation(const char *move)
{
    if ((((move[0] >='a') && (move[0] <='h')) || ((move[0] >='A') && (move[0] <='H'))) &&
        (move[1] >='1') && (move[1] <='8') &&
        (((move[2] >='a') && (move[2] <='h')) || ((move[2] >='A') && (move[2] <='H'))) &&
        (move[3] >='1') && (move[3] <='8'))
        return(1);
    else
        return(0);
}

/*1st pass: checks whether this rook move is legal*/
static int32_t Check_Rook_Move(const BOARD_POS *bpos, int32_t from, int32_t to)
{
    int32_t i;

    if (from > to)
        /*always assume the same direction. what we are looking for is
        whether the squares between from and to are free, so it doesn't matter
        in which direction the move is.*/
    {
        i = from;
        from = to;
        to = i;
    }

    if (RANK(from) == RANK(to)) /*horizontal move*/
    {
        for (i = from + FILE_DIFF; i < to; i+= FILE_DIFF)
            if (bpos->board[i] != NO_PIECE) /*rooks cannot jump over pieces*/
                return(ERR_ROOK_MOVE_ILLEGAL);
    } else if (FILE(from) == FILE(to)) /*vertical move*/
    {
        for (i = from + RANK_DIFF; i < to; i += RANK_DIFF)
            if (bpos->board[i] != NO_PIECE) /*rooks cannot jump over pieces*/
                return(ERR_ROOK_MOVE_ILLEGAL);
    } else /*neither horizontal nor vertical move*/
        return(ERR_ROOK_MOVE_ILLEGAL);

    /*all tests passed: the move is OK.*/
    return(ERR_NO_ERROR);
}

/*1st pass: checks whether this bishop move is legal*/
static int32_t Check_Bishop_Move(const BOARD_POS *bpos, int32_t from, int32_t to)
{
    int32_t i, diff, start_file, end_file;

    if (from > to)
        /*always assume the same direction. what we are looking for is
        whether the squares between from and to are free, so it doesn't matter
        in which direction the move is.*/
    {
        i = from;
        from = to;
        to = i;
    }

    diff = to-from;
    start_file = FILE(from);
    end_file = FILE(to);

    if ((diff % UP_RIGHT) == 0) /*move from down-left to up-right*/
    {
        if (start_file >= end_file) /*moving to the right would be legal*/
            return(ERR_BISHOP_MOVE_ILLEGAL);

        for (i = from + UP_RIGHT; i < to; i += UP_RIGHT)
            if (bpos->board[i] != NO_PIECE) /*bishops cannot jump over pieces*/
                return(ERR_BISHOP_MOVE_ILLEGAL);
    } else if ((diff % UP_LEFT) == 0) /*move from down-right to up-left*/
    {
        if (start_file <= end_file) /*moving to the left would be legal*/
            return(ERR_BISHOP_MOVE_ILLEGAL);

        for (i = from + UP_LEFT; i < to; i += UP_LEFT)
            if (bpos->board[i] != NO_PIECE) /*bishops cannot jump over pieces*/
                return(ERR_BISHOP_MOVE_ILLEGAL);
    } else /*no diagonal move*/
         return(ERR_BISHOP_MOVE_ILLEGAL);

    /*all tests passed: the move is OK.*/
    return(ERR_NO_ERROR);
}

/*1st pass: checks whether this knight move is legal. the knight moves are
expressed as compass directions.
Eg, Ng1-f3 is a move to north-north-west: KNIGHT_NNW.*/
static int32_t Check_Knight_Move(int32_t from, int32_t to)
{
    int32_t diff;
    int from_file, from_rank;

    diff = to-from;
    
    /*basic sanity check*/
    if ((diff != KNIGHT_NNW) && (diff != KNIGHT_WNW) && (diff != KNIGHT_WSW) && (diff != KNIGHT_SSW) &&
        (diff != KNIGHT_SSE) && (diff != KNIGHT_ESE) && (diff != KNIGHT_ENE) && (diff != KNIGHT_NNE))
        return(ERR_KNIGHT_MOVE_ILLEGAL);
    
    /*now the boarders. first the border ranks.*/
    from_rank = RANK(from);
    if (from_rank == RANK_1) /*first rank*/
        if ((diff != KNIGHT_NNW) && (diff != KNIGHT_WNW) && 
            (diff != KNIGHT_ENE) && (diff != KNIGHT_NNE))
            return(ERR_KNIGHT_MOVE_ILLEGAL);
    
    if (from_rank == RANK_2) /*second rank*/
        if ((diff != KNIGHT_WSW) && (diff != KNIGHT_WNW) && (diff != KNIGHT_NNW) &&
            (diff != KNIGHT_NNE) && (diff != KNIGHT_ENE) && (diff != KNIGHT_ESE))
            return(ERR_KNIGHT_MOVE_ILLEGAL);
    
    if (from_rank == RANK_7) /*7th rank*/
        if ((diff != KNIGHT_WNW) && (diff != KNIGHT_WSW) && (diff != KNIGHT_SSW) &&
            (diff != KNIGHT_SSE) && (diff != KNIGHT_ESE) && (diff != KNIGHT_ENE))
            return(ERR_KNIGHT_MOVE_ILLEGAL);

    if (from_rank == RANK_8) /*8th rank*/
        if ((diff != KNIGHT_WSW) && (diff != KNIGHT_SSW) &&
            (diff != KNIGHT_SSE) && (diff != KNIGHT_ESE))
            return(ERR_KNIGHT_MOVE_ILLEGAL);

    /*now the border files.*/
    from_file = FILE(from);
    if (from_file == FILE_A) /*a file*/
        if ((diff != KNIGHT_SSE) && (diff != KNIGHT_ESE) &&
            (diff != KNIGHT_ENE) && (diff != KNIGHT_NNE))
            return(ERR_KNIGHT_MOVE_ILLEGAL);

    if (from_file == FILE_B) /*b file*/

        if ((diff != KNIGHT_NNW) && (diff != KNIGHT_SSW) && (diff != KNIGHT_SSE) &&
            (diff != KNIGHT_ESE) && (diff != KNIGHT_ENE) && (diff != KNIGHT_NNE))
            return(ERR_KNIGHT_MOVE_ILLEGAL);

    if (from_file == FILE_G) /*g file*/
        if ((diff != KNIGHT_NNW) && (diff != KNIGHT_WNW) && (diff != KNIGHT_WSW) &&
            (diff != KNIGHT_SSW) && (diff != KNIGHT_SSE) && (diff != KNIGHT_NNE))
            return(ERR_KNIGHT_MOVE_ILLEGAL);

    if (from_file == FILE_H) /*h file*/
        if ((diff != KNIGHT_NNW) && (diff != KNIGHT_WNW) &&
            (diff != KNIGHT_WSW) && (diff != KNIGHT_SSW))
            return(ERR_KNIGHT_MOVE_ILLEGAL);

    /*all tests passed: the move is OK.*/
    return(ERR_NO_ERROR);
}

/*1st pass: checks whether this pawn move is legal.*/
static int32_t Check_Pawn_Move(const BOARD_POS *bpos, int32_t epsquare, int32_t from, int32_t to)
{
    uint8_t moving_piece;
    moving_piece = bpos->board[from];
    
    switch (moving_piece)
    {
        case WPAWN:
            if (bpos->board[to] == NO_PIECE) /*either no capture or en passant*/
            {
                int32_t diff = to - from;
                
                if ((diff == UP_LEFT) || (diff == UP_RIGHT)) /*en passant*/
                {
                    if (to != epsquare) /*but illegal square*/
                        return(ERR_PAWN_EP_ILLEGAL);
                } else if (!( (diff == RANK_DIFF) ||
                            ((diff == 2*RANK_DIFF) && (from >= A2) &&
                             (from <= H2) && (bpos->board[from + RANK_DIFF] == NO_PIECE)) ))
                    return(ERR_PAWN_MOVE_ILLEGAL);
            } else /*regular capture*/
            {
                int32_t diff = to - from;
                
                if ((diff != UP_LEFT) && (diff != UP_RIGHT))
                    return(ERR_PAWN_CAPT_ILLEGAL);
                
                if ((FILE(from) == FILE_A) && (diff == UP_LEFT)) /*hitting to the left from the a file is not possible.*/
                    return(ERR_PAWN_CAPT_ILLEGAL);
                    
                if ((FILE(from) == FILE_H) && (diff == UP_RIGHT)) /*hitting to the right from the h file is not possible.*/
                    return(ERR_PAWN_CAPT_ILLEGAL);
            }
            break;
        
        case BPAWN:
            if (bpos->board[to] == NO_PIECE) /*either no capture or en passant*/
            {
                int32_t diff = from - to;
                
                if ((diff == UP_LEFT) || (diff == UP_RIGHT)) /*en passant*/
                {
                    if (to != epsquare) /*but illegal square*/
                        return(ERR_PAWN_EP_ILLEGAL);
                } else if (!( (diff == RANK_DIFF) ||
                              ((diff == 2*RANK_DIFF) && (from >= A7) &&
                               (from <= H7) && (bpos->board[from - RANK_DIFF] == NO_PIECE)) ))
                    return(ERR_PAWN_MOVE_ILLEGAL);
            } else /*regular capture*/
            {
                int32_t diff = from - to;
                
                if ((diff != UP_LEFT) && (diff != UP_RIGHT))
                    return(ERR_PAWN_CAPT_ILLEGAL);

                if ((FILE(from) == FILE_A) && (diff == UP_RIGHT)) /*hitting to the left from the a file is not possible.*/
                    return(ERR_PAWN_CAPT_ILLEGAL);

                if ((FILE(from) == FILE_H) && (diff == UP_LEFT)) /*hitting to the right from the h file is not possible.*/
                    return(ERR_PAWN_CAPT_ILLEGAL);
            }
            break;
        
        default:
            /*should not happen because the moving piece has already been checked*/
            return(ERR_UNKNOWN);
            break;
    }
    
    /*all tests passed: the move is OK.*/
    return(ERR_NO_ERROR);
}

/*1st pass: checks whether a square is threatened by black*/
static int32_t Check_White_Square_Threatened(const BOARD_POS *bpos, int32_t sq)
{
    int32_t i, start_file, end_square;
    
    /*threatened by knight?*/
    for (i = A1; i <= H8; i++)
    {
        /*check for all squares: is there an enemy knight?
        and if so, is this square in knight distance from
        the square under test?*/
        if (bpos->board[i] == BKNIGHT)
            if (Check_Knight_Move(sq, i) == ERR_NO_ERROR)
                return(1);
    }
 
    /*threatened by rook/queen, horizontally from the left?*/
    end_square = RANK(sq) * RANK_DIFF;
    for (i = sq - FILE_DIFF; i >= end_square; i-= FILE_DIFF)
    {
        if ((bpos->board[i] == BROOK) || (bpos->board[i] == BQUEEN))
            return(1);
        else if ((bpos->board[i] >= WPAWN) && (bpos->board[i] <= WKING)) /*white piece blocking*/
            break;
        else if ((bpos->board[i] == BBISHOP) || (bpos->board[i] == BKNIGHT)) /*black piece blocking*/
            break;
    }
    
    /*threatened by rook/queen, horizontally from the right?*/
    end_square += 7 * FILE_DIFF;
    for (i = sq+FILE_DIFF; i <= end_square; i+= FILE_DIFF)
    {
        if ((bpos->board[i] == BROOK) || (bpos->board[i] == BQUEEN))
            return(1);
        else if ((bpos->board[i] >= WPAWN) && (bpos->board[i] <= WKING)) /*white piece blocking*/
            break;
        else if ((bpos->board[i] == BBISHOP) || (bpos->board[i] == BKNIGHT)) /*black piece blocking*/
            break;
    }
    
    /*threatened by rook/queen, vertically from downwards?*/
    end_square = FILE(sq);
    for (i = sq - RANK_DIFF; i >= end_square; i -= RANK_DIFF)
    {
        if ((bpos->board[i] == BROOK) || (bpos->board[i] == BQUEEN))
            return(1);
        else if ((bpos->board[i] >= WPAWN) && (bpos->board[i] <= WKING)) /*white piece blocking*/
            break;
        else if ((bpos->board[i] == BPAWN) || (bpos->board[i] == BBISHOP) || (bpos->board[i] == BKNIGHT)) /*black piece blocking*/
            break;
    }
    
    /*threatened by rook/queen, vertically from upwards?*/
    end_square += 7 * RANK_DIFF;
    for (i = sq + RANK_DIFF; i <= end_square; i += RANK_DIFF)
    {
        if ((bpos->board[i] == BROOK) || (bpos->board[i] == BQUEEN))
            return(1);
        else if ((bpos->board[i] >= WPAWN) && (bpos->board[i] <= WKING)) /*white piece blocking*/
            break;
        else if ((bpos->board[i] == BPAWN) || (bpos->board[i] == BBISHOP) || (bpos->board[i] == BKNIGHT)) /*black piece blocking*/
            break;
    }
    
    /*threatened by bishop/queen, from the upper left?*/
    start_file = FILE(sq);
    for (i = sq + UP_LEFT; ((i <= H8) && (FILE(i) < start_file)); i += UP_LEFT)
    {
        if ((bpos->board[i] == BBISHOP) || (bpos->board[i] == BQUEEN))
            return(1);
        else if ((bpos->board[i] >= WPAWN) && (bpos->board[i] <= WKING)) /*white piece blocking*/
            break;
        else if ((bpos->board[i] == BPAWN) || (bpos->board[i] == BROOK) || (bpos->board[i] == BKNIGHT)) /*black piece blocking*/
            break;
    }

    /*threatened by bishop/queen, from the upper right?*/
    for (i = sq + UP_RIGHT; ((i <= H8) && (FILE(i) > start_file)); i += UP_RIGHT)
    {
        if ((bpos->board[i] == BBISHOP) || (bpos->board[i] == BQUEEN))
            return(1);
        else if ((bpos->board[i] >= WPAWN) && (bpos->board[i] <= WKING)) /*white piece blocking*/
            break;
        else if ((bpos->board[i] == BPAWN) || (bpos->board[i] == BROOK) || (bpos->board[i] == BKNIGHT)) /*black piece blocking*/
            break;
    }

    /*threatened by bishop/queen, from the lower right?*/
    for (i = sq - UP_LEFT; ((i >= A1) && (FILE(i) > start_file)); i -= UP_LEFT)
    {
        if ((bpos->board[i] == BBISHOP) || (bpos->board[i] == BQUEEN))
            return(1);
        else if ((bpos->board[i] >= WPAWN) && (bpos->board[i] <= WKING)) /*white piece blocking*/
            break;
        else if ((bpos->board[i] == BPAWN) || (bpos->board[i] == BROOK) || (bpos->board[i] == BKNIGHT)) /*black piece blocking*/
            break;
    }

    /*threatened by bishop/queen, from the lower left?*/
    for (i = sq - UP_RIGHT; ((i >= A1) && (FILE(i) < start_file)); i -= UP_RIGHT)
    {
        if ((bpos->board[i] == BBISHOP) || (bpos->board[i] == BQUEEN))
            return(1);
        else if ((bpos->board[i] >= WPAWN) && (bpos->board[i] <= WKING)) /*white piece blocking*/
            break;
        else if ((bpos->board[i] == BPAWN) || (bpos->board[i] == BROOK) || (bpos->board[i] == BKNIGHT)) /*black piece blocking*/
            break;
    }

    /*threatened by pawn from the upper left?*/
    if (start_file > FILE_A)
        if (bpos->board[sq + UP_LEFT] == BPAWN)
            return(1);

    /*threatened by pawn from the upper right?*/
    if (start_file < FILE_H)
        if (bpos->board[sq + UP_RIGHT] == BPAWN)
            return(1);

    /*the square under test is not threatened.*/
    return(0);
}

/*1st pass: checks whether square is threatened by white*/
static int32_t Check_Black_Square_Threatened(const BOARD_POS *bpos, int32_t sq)
{
    int32_t i, start_file, end_square;
    
    /*threatened by knight?
      check for all squares: is there an enemy knight?
      and if so, is this square in knight distance from the square under test?*/
    for (i = A1; i <= H8; i++)
        if (bpos->board[i] == WKNIGHT)
            if (Check_Knight_Move(sq, i) == ERR_NO_ERROR)
                return(1);
    
    /*threatened by rook/queen, horizontally from the left?*/
    end_square = RANK(sq) * RANK_DIFF;
    for (i = sq-FILE_DIFF; i >= end_square; i -= FILE_DIFF)
    {
        if ((bpos->board[i] == WROOK) || (bpos->board[i] == WQUEEN))
            return(1);
        else if ((bpos->board[i] >= BPAWN) && (bpos->board[i] <= BKING)) /*black piece blocking*/
            break;
        else if ((bpos->board[i] == WBISHOP) || (bpos->board[i] == WKNIGHT)) /*white piece blocking*/
            break;
    }
    
    /*threatened by rook/queen, horizontally from the right?*/
    end_square += 7 * FILE_DIFF;
    for (i = sq + FILE_DIFF; i <= end_square; i += FILE_DIFF)
    {
        if ((bpos->board[i] == WROOK) || (bpos->board[i] == WQUEEN))
            return(1);
        else if ((bpos->board[i] >= BPAWN) && (bpos->board[i] <= BKING)) /*black piece blocking*/
            break;
        else if ((bpos->board[i] == WBISHOP) || (bpos->board[i] == WKNIGHT)) /*white piece blocking*/
            break;
    }
    
    /*threatened by rook/queen, vertically from downwards?*/
    end_square = FILE(sq);
    for (i = sq - RANK_DIFF; i >= end_square; i -= RANK_DIFF)
    {
        if ((bpos->board[i] == WROOK) || (bpos->board[i] == WQUEEN))
            return(1);
        else if ((bpos->board[i] >= BPAWN) && (bpos->board[i] <= BKING)) /*black piece blocking*/
            break;
        else if ((bpos->board[i] == WPAWN) || (bpos->board[i] == WBISHOP) || (bpos->board[i] == WKNIGHT)) /*white piece blocking*/
            break;
    }
    
    /*threatened by rook/queen, vertically from upwards?*/
    end_square += 7 * RANK_DIFF;
    for (i = sq + RANK_DIFF; i <= end_square; i += RANK_DIFF)
    {
        if ((bpos->board[i] == WROOK) || (bpos->board[i] == WQUEEN))
            return(1);
        else if ((bpos->board[i] >= BPAWN) && (bpos->board[i] <= BKING)) /*black piece blocking*/
            break;
        else if ((bpos->board[i] == WPAWN) || (bpos->board[i] == WBISHOP) || (bpos->board[i] == WKNIGHT)) /*white piece blocking*/
            break;
    }

    /*threatened by bishop/queen, from the lower left?*/
    start_file = FILE(sq);
    for (i = sq - UP_RIGHT; ((i >= A1) && (FILE(i) < start_file)); i -= UP_RIGHT)
    {
        if ((bpos->board[i] == WBISHOP) || (bpos->board[i] == WQUEEN))
            return(1);
         else if ((bpos->board[i] >= BPAWN) && (bpos->board[i] <= BKING)) /*black piece blocking*/
            break;
        else if ((bpos->board[i] == WPAWN) || (bpos->board[i] == WROOK) || (bpos->board[i] == WKNIGHT)) /*white piece blocking*/
            break;
    }

    /*threatened by bishop, from the lower right?*/
    for (i = sq - UP_LEFT; ((i >= A1) && (FILE(i) > start_file)); i -= UP_LEFT)
    {
        if ((bpos->board[i] == WBISHOP) || (bpos->board[i] == WQUEEN))
            return(1);
        else if ((bpos->board[i] >= BPAWN) && (bpos->board[i] <= BKING)) /*black piece blocking*/
            break;
        else if ((bpos->board[i] == WPAWN) || (bpos->board[i] == WROOK) || (bpos->board[i] == WKNIGHT)) /*white piece blocking*/
            break;
    }
    
    /*threatened by bishop/queen, from the upper right?*/
    for (i = sq + UP_RIGHT; ((i <= H8) && (FILE(i) > start_file)); i += UP_RIGHT)
    {
        if ((bpos->board[i] == WBISHOP) || (bpos->board[i] == WQUEEN))
            return(1);
        else if ((bpos->board[i] >= BPAWN) && (bpos->board[i] <= BKING)) /*black piece blocking*/
            break;
        else if ((bpos->board[i] == WPAWN) || (bpos->board[i] == WROOK) || (bpos->board[i] == WKNIGHT)) /*white piece blocking*/
            break;
    }
    
    /*threatened by bishop, from the upper left?*/
    for (i = sq + UP_LEFT; ((i <= H8) && (FILE(i) < start_file)); i += UP_LEFT)
    {
        if ((bpos->board[i] == WBISHOP) || (bpos->board[i] == WQUEEN))
            return(1);
        else if ((bpos->board[i] >= BPAWN) && (bpos->board[i] <= BKING)) /*black piece blocking*/
            break;
        else if ((bpos->board[i] == WPAWN) || (bpos->board[i] == WROOK) || (bpos->board[i] == WKNIGHT)) /*white piece blocking*/
            break;
    }
    
    /*threatened by pawn from the lower left?*/
    if (start_file > FILE_A)
        if (bpos->board[sq - UP_RIGHT] == WPAWN)
            return(1);

    /*threatened by pawn from the lower right?*/
    if (start_file < FILE_H)
        if (bpos->board[sq - UP_LEFT] == WPAWN)
            return(1);

    /*the square under test is not threatened.*/
    return(0);
}

/*1st pass: checks whether the white king is in check.*/
static int32_t Check_White_King_Threatened(const BOARD_POS *bpos)
{
    int32_t king_pos;
    
    /*first get the white king's position. start with A1 because
    the white king usually is somewhere on the 1st rank.*/
    for (king_pos = A1; king_pos <= H8; king_pos++)
        if (bpos->board[king_pos] == WKING)
            break;
    
    if (king_pos > H8) /*illegal?!*/
        return(1);
        
    if (Check_White_Square_Threatened(bpos, king_pos))
        return(1);
    
    return(0);
}

/*1st pass: checks whether the black king is in check.*/
static int32_t Check_Black_King_Threatened(const BOARD_POS *bpos)
{
    int32_t king_pos;
    
    /*first get the black king's position. start with H8 because
    the black king usually is somewhere on the 8th rank.*/
    for (king_pos = H8; king_pos >= A1; king_pos--)
        if (bpos->board[king_pos] == BKING)
            break;
    
    if (king_pos < A1) /*illegal?!*/
        return(1);

    if (Check_Black_Square_Threatened(bpos, king_pos))
        return(1);

    return(0);
}

/*1st pass: checks whether this king move is legal.*/
static int32_t Check_King_Move(const BOARD_POS *bpos, int32_t from, int32_t to)
{
    uint8_t moving_piece = bpos->board[from];
    
    switch (moving_piece)
    {
        case WKING:
            if ((from == E1) && (to == G1)) /*kingside castling */
            {
                if (bpos->board[STATUS_FLAGS] & (WKMOVED | WRH1MOVED))
                    return(ERR_CST_KING_ROOK_MOVED);

                if ((bpos->board[F1] != NO_PIECE) || (bpos->board[G1] != NO_PIECE))
                    return(ERR_CST_SQ_BLOCKED);

                if (Check_White_Square_Threatened(bpos, E1))
                    return(ERR_CST_IN_CHECK);
  
                if ((Check_White_Square_Threatened(bpos, F1)) || (Check_White_Square_Threatened(bpos, G1)))
                    return(ERR_CST_SQ_THREAT);
            } else if ((from == E1) && (to == C1)) /*queenside castling */
            {
                if (bpos->board[STATUS_FLAGS] & (WKMOVED | WRA1MOVED))
                    return(ERR_CST_KING_ROOK_MOVED);

                if ((bpos->board[D1] != NO_PIECE) || (bpos->board[C1] != NO_PIECE) || (bpos->board[B1] != NO_PIECE))
                    return(ERR_CST_SQ_BLOCKED);

                if (Check_White_Square_Threatened(bpos, E1))
                    return(ERR_CST_IN_CHECK);

                if ((Check_White_Square_Threatened(bpos, D1)) || (Check_White_Square_Threatened(bpos, B1)))
                    return(ERR_CST_SQ_THREAT);
            } else
            {
                int32_t diff = Abs(from-to);
                
                if ((diff != FILE_DIFF) && (diff != RANK_DIFF) && (diff != UP_LEFT)  && (diff != UP_RIGHT))
                    return(ERR_KING_MOVE_ILLEGAL);
            }
            break;
            
        case BKING:
            if ((from == E8) && (to == G8)) /*kingside castling*/
            {
                if (bpos->board[STATUS_FLAGS] & (BKMOVED | BRH8MOVED))
                    return(ERR_CST_KING_ROOK_MOVED);

                if ((bpos->board[F8] != NO_PIECE) || (bpos->board[G8] != NO_PIECE))
                    return(ERR_CST_SQ_BLOCKED);

                if (Check_Black_Square_Threatened(bpos, E8))
                    return(ERR_CST_IN_CHECK);

                if ((Check_Black_Square_Threatened(bpos, F8)) || (Check_Black_Square_Threatened(bpos, G8)))
                    return(ERR_CST_SQ_THREAT);
            } else if ((from == E8) && (to == C8)) /*queenside castling*/
            {
                if (bpos->board[STATUS_FLAGS] & (BKMOVED | BRA8MOVED))
                    return(ERR_CST_KING_ROOK_MOVED);

                if ((bpos->board[D8] != NO_PIECE) || (bpos->board[C8] != NO_PIECE) || (bpos->board[B8] != NO_PIECE))
                    return(ERR_CST_SQ_BLOCKED);

                if (Check_Black_Square_Threatened(bpos, E8))
                    return(ERR_CST_IN_CHECK);

                if ((Check_Black_Square_Threatened(bpos, D8)) || (Check_Black_Square_Threatened(bpos, B8)))
                    return(ERR_CST_SQ_THREAT);
            } else
            {
                int32_t diff = Abs(from-to);
                
                if ((diff != FILE_DIFF) && (diff != RANK_DIFF) && (diff != UP_LEFT)  && (diff != UP_RIGHT))
                    return(ERR_KING_MOVE_ILLEGAL);
            }
            break;
            
        default:
            /*should not happen because the moving piece has already been checked*/
            return(ERR_UNKNOWN);
            break;
    }
    
    /*all tests passed: the move is OK.*/
    return(ERR_NO_ERROR);
}

/*1st pass: checks whether the current move in the current line is legal.*/
static int32_t Check_Move(const BOARD_POS *bpos, int32_t epsquare, int32_t from, int32_t to, int32_t white_move)
{
    int32_t ret;
    uint8_t moving_piece = bpos->board[from];
    uint8_t target_piece = bpos->board[to];

    /*---------------- basic checks ----------------*/
    
    /*are the origin and destation square the same?*/
    if (from == to)
        return(ERR_FROM_TO_SQ_SAME);

    /*moving from empty square?*/
    if (moving_piece == NO_PIECE)
        return(ERR_FROM_SQ_EMPTY);
    
    if (white_move) /*white to move*/
    {
        /*move only one's own pieces*/
        if ((moving_piece >= BPAWN) && (moving_piece <= BKING))
            return(ERR_WH_MOVE_BL_PIECE);

        /*hit only opposite pieces*/
        if ((target_piece >= WPAWN) && (target_piece <= WKING))
            return(ERR_WH_CAPT_WH_PIECE);
    } else /*black to move*/
    {
        /*move only one's own pieces*/
        if ((moving_piece >= WPAWN) && (moving_piece <= WKING))
            return(ERR_BL_MOVE_WH_PIECE);

        /*hit only opposite pieces*/
        if ((target_piece >= BPAWN) && (target_piece <= BKING))
            return(ERR_BL_CAPT_BL_PIECE);
    }
    
    /*now examine the specific piece that is moving*/
    switch (moving_piece)
    {
        case WKING:
        case BKING:
            ret = Check_King_Move(bpos, from, to);
            break;
            
        case WQUEEN:
        case BQUEEN:
            /*the queen check must either be a legal rook or bishop move.*/
            ret = Check_Rook_Move(bpos, from, to); /*legal rook-like move?*/
            if (ret != ERR_NO_ERROR) /*ok, not a legal rook-like move.*/
                ret = Check_Bishop_Move(bpos, from, to); /*maybe a bishop-like move?*/

            if (ret != ERR_NO_ERROR) /*illegal queen moves have their own return code.*/
                ret = ERR_QUEEN_MOVE_ILLEGAL;
            break;
        
        case WROOK:
        case BROOK:
            ret = Check_Rook_Move(bpos, from, to);
            break;
        
        case WBISHOP:
        case BBISHOP:
            ret = Check_Bishop_Move(bpos, from, to);
            break;
        
        case WKNIGHT:
        case BKNIGHT:
            ret = Check_Knight_Move(from, to);
            break;
        
        case WPAWN:
        case BPAWN:
            ret = Check_Pawn_Move(bpos, epsquare, from, to);
            break;
        
        default:
            /*should not happen since all piece types are covered*/
            ret = ERR_UNKNOWN;
            break;
    }
    
    return(ret);
}

/*1st pass: checks the current input line for illegal moves.*/
static int32_t Check_Input_Book_Line(const char *line, int32_t line_number, int32_t *move_cnt)
{
    int32_t line_len, line_index = 0, from, to, white_move = 1;
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
        uint8_t file_start[4];
        /*depending on the target platform, char might or might not be signed. Just
        interpret it as uint8_t, but avoid potential issues with pointer aliasing.
        using memcpy is one of the clean ways to do that.*/
        memcpy(file_start, line, 4);
        
        /*if we got the UTF-8 BOM, just skip that.*/
        if ((file_start[0] == 0xEFu) && (file_start[1] == 0xBBu) && (file_start[2] == 0xBFu))
            line += 3;
        else if ( ((file_start[0] == 0xFFu) && (file_start[1] == 0xFEu) && (file_start[2] == 0x00u) && (file_start[3] == 0x00u)) ||
                  ((file_start[0] == 0x00u) && (file_start[1] == 0x00u) && (file_start[2] == 0xFEu) && (file_start[3] == 0xFFu)))
        /*we got the UTF-32 BOM, but UTF-32 is not supported.
         abort the checking because that would yield tons of errors.*/
        {
            fprintf(stdout, "ERROR in line %5ld:\r\n", (long) line_number);
            fprintf(stdout, "      UTF-32 text format not supported. Use ASCII or UTF-8.\r\n\r\n");
            return(LINE_ABORT);
        }  else if ( ((file_start[0] == 0xFFu) && (file_start[1] == 0xFEu)) ||
                     ((file_start[0] == 0xFEu) && (file_start[1] == 0xFFu)))
        /*we got the UTF-16 BOM, but UTF-16 is not supported.
         abort the checking because that would yield tons of errors.*/
        {
            fprintf(stdout, "ERROR in line %5ld:\r\n", (long) line_number);
            fprintf(stdout, "      UTF-16 text format not supported. Use ASCII or UTF-8.\r\n\r\n");
            return(LINE_ABORT);
        }
    }
    
    /*set initial board position at the start of the input line.*/
    Util_Set_Start_Pos(&bpos, &epsquare);
    
    /*skip over initial white spaces*/
    while (Util_Is_Whitespace(line[line_index]))
        line_index++;

    line_len = strlen(line);
    
    /*line without moves*/
    if ((line_len == line_index) || (Util_Is_Commentline(line[line_index])) || (Util_Is_Line_End(line[line_index])))
        return(LINE_OK);

    if (!(((line[line_index] >= 'a') && (line[line_index] <= 'h')) ||
          ((line[line_index] >= 'A') && (line[line_index] <= 'H'))))
    {
        fprintf(stdout, "ERROR in line %5ld, column %3d with '%c':\r\n", (long) line_number, line_index + 1, line[line_index]);
        fprintf(stdout, "      illegal line starting character.\r\n\r\n");
        
        return(LINE_ERROR);
    }
    
    /*give a zero termination to the move buffer*/
    move[4] = '\0';

    while (line_index < line_len)
    {
        int32_t ret;

        /*get the current move. the zero termination has already happened
          before entering this loop.*/
        move[0] = line[line_index++];
        move[1] = line[line_index++];
        move[2] = line[line_index++];
        move[3] = line[line_index++];

        /*illegal characters in the move?*/
        if (Check_Move_Notation(move) == 0)
        {
            int32_t i, move_incomplete = 0;
            
            /*recognise move strings that are too short*/
            for (i = 0; i < 4; i++)
            {
                if ((move[i] <= ' ') || (Util_Is_Line_End(move[i])))
                {
                    move[i] = '\0';
                    move_incomplete = 1;
                    break;
                }
            }
            fprintf(stdout, "ERROR in line %5ld, column %3d with %s:\r\n", (long) line_number, line_index-3, move);
            if (move_incomplete == 0)
                fprintf(stdout, "      move has illegal character.\r\n\r\n");
            else
                fprintf(stdout, "      move is too short.\r\n\r\n");
            return(LINE_ERROR);
        }

        /*convert the string format to board indices*/
        Util_Move_Conv(move, &from, &to);

        /*check the move in detail*/
        ret = Check_Move(&bpos, epsquare, from, to, white_move);
        if (ret != ERR_NO_ERROR)
        {
            if (ret > ERR_UNKNOWN) /*just to be safe in case an error code has been forgotten or so*/
                ret = ERR_UNKNOWN;
            fprintf(stdout, "ERROR in line %5ld, column %3d with %s:\r\n", (long) line_number, line_index-3, move);
            fprintf(stdout, "      %s.\r\n\r\n", parsing_errors[ret]);
            return(LINE_ERROR);
        }

        /*execute the move on the board*/
        Util_Move_Do(&bpos, &epsquare, from, to);
        
        /*now check that the side that is NOT to move is not in check*/
        if ((bpos.board[STATUS_FLAGS] & BLACK_MV) == FLAGS_RESET)
        /*white to move*/
        {
            if (Check_Black_King_Threatened(&bpos))
            {
                fprintf(stdout, "ERROR in line %5ld, column %3d with %s:\r\n", (long) line_number, line_index-3, move);
                fprintf(stdout, "      black king in check when white is to move.\r\n\r\n");
                return(LINE_ERROR);
            }
        } else
        /* black to move*/
        {
            if (Check_White_King_Threatened(&bpos))
            {
                fprintf(stdout, "ERROR in line %5ld, column %3d with %s:\r\n", (long) line_number, line_index-3, move);
                fprintf(stdout, "      white king in check when black is to move.\r\n\r\n");
                return(LINE_ERROR);
            }
        }

        /*raise the move counter*/
        (*move_cnt)++;

        /*jump over inactive move markers. The move line may be passive knowledge,
          but it still has to be legal.*/
        if (Util_Is_Passivemarker(line[line_index]))
            line_index++;
        
        if (Util_Is_Line_End(line[line_index])) /*line end is OK here*/
            return(LINE_OK);
        
        if (!Util_Is_Whitespace(line[line_index]))
        /*if we are not at the end of the line, be it by real end or by
          a beginning '(' comment, then a white space must follow.*/
        {
            fprintf(stdout, "ERROR in line %5ld, column %3d with '%c':\r\n", (long) line_number, line_index+1, line[line_index]);
            fprintf(stdout, "      white space expected.\r\n\r\n");
            return(LINE_ERROR);
        }
        
        /*before comments, there are usually several spaces in a line, so
          skip over them.*/
        while (Util_Is_Whitespace(line[line_index]))
            line_index++;
        
        /*we may have reached the end of the line or the beginning of a comment.*/
        if (Util_Is_Line_End(line[line_index]))
            return(LINE_OK);

        /*change the side to move*/
        white_move = !white_move;
    }
    return(LINE_OK);
}

/*1st pass: checks the input file for illegal moves.*/
void Check_Input_Book_File(int32_t *errors, int32_t *move_cnt, int32_t *line_number, FILE *book_file)
{
    char book_line[BOOK_LINE_LEN + 1]; /*+1 for 0 termination*/
    
    /*be sure to have a proper initialisation for the first line - used for
    checking a potentially unsupported encoding.*/
    (void) memset(book_line, 0, sizeof(book_line));
    
    while (fgets(book_line, BOOK_LINE_LEN, book_file))
    {
        int32_t ret;
        book_line[BOOK_LINE_LEN] = '\0'; /*be sure to have 0 termination*/
        /*parse the line*/
        ret = Check_Input_Book_Line(book_line, *line_number, move_cnt);

        /*if an error was found, raise the error counter*/
        if (ret != LINE_OK)
        {
            (*errors)++;
            if (ret == LINE_ABORT) /*unsupported text encoding: UTF-16, or UTF-32.*/
                return;
        }

        /*increment the line counter*/
        (*line_number)++;
    }
}
