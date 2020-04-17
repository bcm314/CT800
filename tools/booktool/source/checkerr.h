/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2016-2019, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of CT800 (opening book tool
 *                               error definitions).
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

/*book file line parsing error numbers*/
enum
{
    ERR_NO_ERROR = 0,

    ERR_FROM_TO_SQ_SAME,
    ERR_FROM_SQ_EMPTY,
    ERR_WH_MOVE_BL_PIECE,
    ERR_WH_CAPT_WH_PIECE,
    ERR_BL_MOVE_WH_PIECE,
    ERR_BL_CAPT_BL_PIECE,

    ERR_PAWN_EP_ILLEGAL,
    ERR_PAWN_MOVE_ILLEGAL,
    ERR_PAWN_CAPT_ILLEGAL,

    ERR_CST_KING_ROOK_MOVED,
    ERR_CST_SQ_BLOCKED,
    ERR_CST_IN_CHECK,
    ERR_CST_SQ_THREAT,

    ERR_KING_MOVE_ILLEGAL,
    ERR_KNIGHT_MOVE_ILLEGAL,
    ERR_ROOK_MOVE_ILLEGAL,
    ERR_BISHOP_MOVE_ILLEGAL,
    ERR_QUEEN_MOVE_ILLEGAL,

    ERR_UNKNOWN
};

/*book file line parsing error messages.
  must match with the error enums above.*/
static const char *parsing_errors[] =
{
    "no error",
    "from and to square identical",
    "move from empty square",
    "white moving black piece",
    "white capturing white piece",
    "black moving white piece",
    "black capturing black piece",

    "en passant capture illegal",
    "pawn move illegal",
    "pawn capture illegal",

    "castling illegal (king/rook moved before)",
    "castling illegal (square blocked)",
    "castling illegal (king in check)",
    "castling illegal (square under threat)",

    "king move illegal",
    "knight move illegal",
    "rook move illegal",
    "bishop move illegal",
    "queen move illegal",
    
    "unknown error"
};

/*the results of a full line parsing.
OK means the line is correct,
ERROR means an ordinary error,
ABORT means unsupported encoding (UTF-16 or UTF-32).*/
enum
{
    LINE_OK,
    LINE_ERROR,
    LINE_ABORT
};
