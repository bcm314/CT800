/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2015-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of CT800/NGPlay (hash tables).
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

void        Search_Make_Move(MOVE amove);
void        Search_Retract_Last_Move(void);
void        Search_Try_Move(MOVE amove);
MOVE        Search_Get_Current_Root_Move(void);
int NEVER_INLINE Search_Get_Root_Move_List(MOVE *restrict movelist, int *restrict move_cnt, enum E_COLOUR colour, int mate_check);
void NEVER_INLINE Search_Mate_Solver(int black_started_game, enum E_COLOUR colour);

enum E_COMP_RESULT NEVER_INLINE
Search_Get_Best_Move(MOVE *restrict answer_move, MOVE player_move, int32_t full_move_time, enum E_COLOUR colour);

/*the following two functions are implemented as macro since the calling overhead
isn't worthwhile, but they are needed in different files.*/

/*upper 7 bits for the en passant square, which is from 0-120
lower 9 bits for the gflags*/
#define Search_Push_Status() {                  \
  cst_p++;                                      \
  cstack[cst_p] = (en_passant_sq << 9) | gflags;\
}

#define Search_Pop_Status() do {        \
  unsigned int cst_tmp = cstack[cst_p]; \
  gflags = (cst_tmp) & ALLFLAGS;        \
  en_passant_sq = cst_tmp >> 9;         \
  cst_p--;                              \
} while (0)
