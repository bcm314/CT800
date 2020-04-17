/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2016-2020, Rasmus Althoff <althoff@ct800.net>
 *  Copyright (C) 2015, Marcel van Kervinck
 *
 *  This file is part of the CT800 (endgame table).
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
 *  Note: the original KPK generator by Marcel van Kervinck was not
 *  released under GPLv3 or later, see the original conditions below.
 *  This derivative work, however, is under GPLv3 or later.
 *
 *
 *
 *  Changes by Rasmus Althoff:
 *  - Modified the endgame table to be a constant compiled into the
 *    binary in form of a header file.
 *  - Added a reverse lookup function for KKP with black pawn.
 *  - Modified the access to the data for fast lookup despite ROM with
 *    waitstates.
 *  - Stripped down the code to what's really needed for the application.
 *  - Reduced lookup table from 32k to 24k.
 */

/*----------------------------------------------------------------------+
 |                                                                      |
 |      kpk.c -- pretty fast KPK endgame table generator                |
 |                                                                      |
 +----------------------------------------------------------------------*/
/*
 *  Copyright (C) 2015, Marcel van Kervinck
 *  All rights reserved
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/*----------------------------------------------------------------------+
 |      Includes                                                        |
 +----------------------------------------------------------------------*/

#include <stdint.h>
#include <string.h>
#include "ctdefs.h"
#include "kpk.h"

/*include the binary file itself*/
#include "kpk_table.c"

/*----------------------------------------------------------------------+
 |      Definitions                                                     |
 +----------------------------------------------------------------------*/

/*
*  Board geometry
*/

#define file(square)    ((square) & 7u)
#define rank(square)    ((square) >> 3u)
#define square(file, rank) (((rank) << 3u) + (file))

/*horizontally mirrored board, for looking up white king vs. black king and black pawn.*/
static const unsigned int mirror_board[] =
{
    BP_A8, BP_B8, BP_C8, BP_D8, BP_E8, BP_F8, BP_G8, BP_H8,
    BP_A7, BP_B7, BP_C7, BP_D7, BP_E7, BP_F7, BP_G7, BP_H7,
    BP_A6, BP_B6, BP_C6, BP_D6, BP_E6, BP_F6, BP_G6, BP_H6,
    BP_A5, BP_B5, BP_C5, BP_D5, BP_E5, BP_F5, BP_G5, BP_H5,
    BP_A4, BP_B4, BP_C4, BP_D4, BP_E4, BP_F4, BP_G4, BP_H4,
    BP_A3, BP_B3, BP_C3, BP_D3, BP_E3, BP_F3, BP_G3, BP_H3,
    BP_A2, BP_B2, BP_C2, BP_D2, BP_E2, BP_F2, BP_G2, BP_H2,
    BP_A1, BP_B1, BP_C1, BP_D1, BP_E1, BP_F1, BP_G1, BP_H1
};

#define kpIndex(wKing,wPawn) ((rank(wPawn) << 8) + (file(wPawn) << 6) + (wKing))


/*----------------------------------------------------------------------+
 |      Data                                                            |
 +----------------------------------------------------------------------*/

/* that's how the table was - left there for reference how to index it.
static uint64_t kpkTable[2][24*64];*/

/*gets a uint32 from the embedded table file.*/
static uint32_t Kpk_Access(unsigned int side, unsigned int index, unsigned int low_dword)
{
    const uint8_t *buffer;
    uint32_t ret;
    unsigned int idx;
    ENTRY_32 entry;

    idx = (side) ? (24U * 64U) : 0;
    idx += index;
    idx <<= 3; /* *8 because one uint64_t element has 8 bytes*/

    /*low_dword is 0 or 4*/
    buffer  = kpk_dat + idx + low_dword;

    /*actually, that is a bytewise access here to avoid endianess issues.
      it's just that four bytes at a time are fetched to minimise scattered access
      to the slow flash. Besides, ARM makes a 32bit fetch anyway.

      It's just absolutely necessary to have the kpk_dat table (in kpk_table.c) aligned on
      a 4 byte boundary in the ROM, or else this will give unaligned access with a performance penalty.*/
    memcpy(&entry.u, buffer, sizeof(uint32_t));

    /*the table has been saved bytewise.*/
    ret = entry.bytes.byte_0;
    ret <<= 8;
    ret |= entry.bytes.byte_1;
    ret <<= 8;
    ret |= entry.bytes.byte_2;
    ret <<= 8;
    ret |= entry.bytes.byte_3;

    return(ret);
}

/*----------------------------------------------------------------------+
 |      Functions                                                       |
 +----------------------------------------------------------------------*/

unsigned int Kpk_Probe(unsigned int side, unsigned int w_king, unsigned int w_pawn, unsigned int b_king)
/*white to move: side = 0, black to move: side = 1*/
{
    unsigned int low_dword;

    if (file(w_pawn) >= 4U) {
        w_pawn ^= square(7U, 0);
        w_king ^= square(7U, 0);
        b_king ^= square(7U, 0);
    }

    if (b_king >= 32U)
    {
        b_king -= 32U;
        low_dword = 0;
    } else
        low_dword = sizeof(uint32_t); /*offset 4 bytes*/

    /*the lowest 4*64 positions would be for the pawn on the first rank,
      which has been left out from the table.*/
    unsigned int ix = kpIndex(w_king, w_pawn) - 4U*64U;
    unsigned int bit = (Kpk_Access(side, ix, low_dword) >> b_king) & 1U;
    return (bit);
}

unsigned int Kpk_Probe_Reverse(unsigned int side, unsigned int w_king, unsigned int bPawn, unsigned int b_king)
/*mirror the position horizontally.
  ATTENTION! side to move is effectively reversed. white to move = 1*/
{
    unsigned int m_w_king, m_w_pawn, m_b_king;

    m_b_king = mirror_board[w_king];
    m_w_king = mirror_board[b_king];
    m_w_pawn = mirror_board[bPawn];

    return(Kpk_Probe(side, m_w_king, m_w_pawn, m_b_king));
}

/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/
