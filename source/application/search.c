/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2015-2020, Rasmus Althoff <althoff@ct800.net>
 *  Copyright (C) 2010-2014, George Georgopoulos
 *
 *  This file is part of CT800/NGPlay (search engine).
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

#include <stdint.h>
#include <stddef.h>
#include "ctdefs.h"
#include "confdefs.h"
#include "timekeeping.h"
#include "hmi.h"
#include "move_gen.h"
#include "hashtables.h"
#include "eval.h"
#include "book.h"
#include "util.h"
#include "hardware.h"
#include "search.h"

#ifdef PC_PRINTF
#include <stdio.h>
extern char *Play_Translate_Moves(MOVE m);
#endif

/*---------- external variables ----------*/
/*-- READ-ONLY  --*/
extern int game_started_from_0;
extern PIECE empty_p;
extern int fifty_moves;
/*these get set up in Eval_Setup_Initial_Material(), see eval.c for explanation*/
extern int start_pawns;
extern int dynamic_resign_threshold;
extern enum E_COLOUR computer_side;
extern PIECE Wpieces[16];
extern PIECE Bpieces[16];
extern int32_t eval_noise;

/*-- READ-WRITE --*/
extern PIECE *board[120];
extern int mv_stack_p;
extern MVST move_stack[MAX_STACK+1];
extern int cst_p;
extern uint16_t cstack[MAX_STACK+1];
extern int Starting_Mv;
extern int wking, bking;
extern int en_passant_sq;
extern unsigned int gflags;
extern GAME_INFO game_info;
extern LINE GlobalPV;
extern volatile enum E_TIMEOUT time_is_up;
extern TT_ST     T_T[MAX_TT+CLUSTER_SIZE];
extern TT_ST Opp_T_T[MAX_TT+CLUSTER_SIZE];
extern unsigned int hash_clear_counter;
extern uint64_t hw_config;

#ifdef DEBUG_STACK
extern size_t top_of_stack;
extern size_t max_of_stack;
#endif


/*---------- module global variables ----------*/

/*for every root move, the expected reply from the opponent is cached in this
  array during the iterative deepening to help with the move sorting. since the
  hash tables are so small, the entries quickly get overwritten during the
  middlegame, but move sorting is most important close to the root. so this
  arrays helps out.*/
static CMOVE opp_move_cache[MAXMV];

static MOVE curr_root_move;

/*use the Ciura sequence for the shell sort. more than 57 is not needed because
the rare maximum of pseudo-legal moves in real game positions is about 80 to 90.*/
DATA_SECTION static int shell_sort_gaps[] = {1, 4, 10, 23, 57 /*, 132, 301, 701*/};

DATA_SECTION static int PieceValFromType[PIECEMAX]= {0, 0, PAWN_V, KNIGHT_V, BISHOP_V, ROOK_V, QUEEN_V, INFINITY_, 0, 0,
                                                     0, 0, PAWN_V, KNIGHT_V, BISHOP_V, ROOK_V, QUEEN_V, INFINITY_
                                                    };

/*this is needed for deepening the PV against delay exchanges, which can cause
a horizon effect. only equal exchanges can do so because unequal ones either
would be a clear win or loss anyway.
note that for this purpose, minor pieces are assumed to be equal.*/
DATA_SECTION static int ExchangeValue[PIECEMAX]=    {0, 0, PAWN_V, KNIGHT_V, KNIGHT_V, ROOK_V, QUEEN_V, INFINITY_, 0, 0,
                                                     0, 0, PAWN_V, KNIGHT_V, KNIGHT_V, ROOK_V, QUEEN_V, INFINITY_
                                                    };
/*for mapping board squares to file masks*/
DATA_SECTION static uint8_t board_file_mask[120] = {
     0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
     0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
     0xFFU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U, 0xFFU,
     0xFFU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U, 0xFFU,
     0xFFU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U, 0xFFU,
     0xFFU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U, 0xFFU,
     0xFFU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U, 0xFFU,
     0xFFU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U, 0xFFU,
     0xFFU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U, 0xFFU,
     0xFFU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U, 0xFFU,
     0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
     0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU
};

/*for move masking except MVV/LVA value*/
DATA_SECTION static MOVE mv_move_mask = {{0xFFU, 0xFFU, 0xFFU, 0}};

/* --------- FUTILITY PRUNING DEFINITIONS ---------- */
#define FUTIL_DEPTH  4
DATA_SECTION static int FutilityMargins[FUTIL_DEPTH]  = {0, 240, 450, 600};

/*same margins in both directions*/
#define RVRS_FUTIL_D    FUTIL_DEPTH
#define RVRS_FutilMargs FutilityMargins

/* ------------- GLOBAL KILERS/HISTORY TABLES ----------------*/

int8_t W_history[6][ENDSQ], B_history[6][ENDSQ];
CMOVE W_Killers[2][MAX_DEPTH], B_Killers[2][MAX_DEPTH];

/* ------------- CHECK LIST BUFFER ----------------*/

/*the check list is only used as intermediate buffer back to back with
  finding evasions as move generation if being in check. works because
  of single thread and reduces stack usage.*/
static MOVE search_check_attacks_buf[CHECKLISTLEN];

/*---------- local functions ----------*/

/*that's a shell sort which
a) doesn't need recursion, unlike quicksort, and
b) is faster than quicksort on lists with less than 100 entries.*/
static void FUNC_HOT Search_Do_Sort(MOVE *restrict movelist, int N)
{
    for (int sizeIndex = sizeof(shell_sort_gaps)/sizeof(shell_sort_gaps[0]) - 1; sizeIndex >= 0; sizeIndex--)
    {
        int gap = shell_sort_gaps[sizeIndex];
        for (int i = gap; i < N; i++)
        {
            int value = movelist[i].m.mvv_lva;
            uint32_t tmp_move = movelist[i].u;
            int j;
            for (j = i - gap; j >= 0; j -= gap)
            {
                MOVE current_move;
                current_move.u = movelist[j].u;
                if (current_move.m.mvv_lva >= value)
                    break;
                movelist[j + gap].u = current_move.u;
            }
            movelist[j + gap].u = tmp_move;
        }
    }
}

/*that's a shell sort for the Search_Play_And_Sort_Moves() routine. Sorts by the position value in sortV[].*/
static void Search_Do_Sort_Value(MOVE *restrict movelist, int *restrict sortV, int N)
{
    for (int sizeIndex = sizeof(shell_sort_gaps)/sizeof(shell_sort_gaps[0]) - 1; sizeIndex >= 0; sizeIndex--)
    {
        int gap = shell_sort_gaps[sizeIndex];
        for (int i = gap; i < N; i++)
        {
            uint32_t tmp_move = movelist[i].u;
            int tmp_sortV = sortV[i];
            int j;
            for (j = i - gap; ((j >= 0) && (sortV[j] < tmp_sortV)); j -= gap)
            {
                movelist[j + gap].u = movelist[j].u;
                sortV[j + gap] = sortV[j];
            }
            movelist[j + gap].u = tmp_move;
            sortV[j + gap] = tmp_sortV;
        }
    }
}

#ifdef PC_PRINTF
void Search_Print_PV_Line(const LINE *aPVp, int Goodmove)
{
    int i;

    for (i = 0; i < aPVp->line_len; i++)
    {
        MOVE decomp_move = Mvgen_Decompress_Move(aPVp->line_cmoves[i]);
        if (i==0 && Goodmove)
        {
            printf("%s! ", Play_Translate_Moves(decomp_move));
        } else {
            printf("%s ", Play_Translate_Moves(decomp_move));
        }
    }
    printf("\n");
}
#endif

/*find the move "key_move" in the list and put it to the top,
moving the following moves down the list.
used for getting PV moves to the top of the list.*/
static void Search_Find_Put_To_Top(MOVE *restrict movelist, int len, MOVE key_move)
{
    for (int i = 0; i < len; i++)
    {
        if (((movelist[i].u ^ key_move.u) & mv_move_mask.u) == 0)
        {
            uint32_t listed_key;

            if (i == 0) /*already at the top*/
                return;

            listed_key = movelist[i].u;

            for ( ; i > 0; i--)
                movelist[i].u = movelist[i-1].u;

            movelist[0].u = listed_key;
            return;
        }
    }
}

/*find the move "key_move" in the list and put it to the top,
moving the following moves down the list, and handle the associated
answer compressed move list, too.
used for getting PV moves to the top of the list.*/
static void Search_Find_Put_To_Top_Root(MOVE *restrict movelist, CMOVE *restrict comp_answerlist, int len, MOVE key_move)
{
    for (int i = 0; i < len; i++)
    {
        if (((movelist[i].u ^ key_move.u) & mv_move_mask.u) == 0)
        {
            uint32_t listed_key = movelist[i].u;
            CMOVE listed_answer = comp_answerlist[i];

            for ( ; i > 0; i--)
            {
                movelist[i].u      = movelist[i - 1].u;
                comp_answerlist[i] = comp_answerlist[i - 1];
            }

            movelist[0].u      = listed_key;
            comp_answerlist[0] = listed_answer;

            return;
        }
    }
}

/*find the move best mvv/lva move in the list and swap it to the top.
note that list length 0 theoretically would yield a buffer overflow,
but all places where this function is called from actually have a
sufficient buffer - it could just be that there is no valid move
inside.*/
static void FUNC_HOT Search_Swap_Best_To_Top(MOVE *movelist, int len)
{
    int best_val;
    MOVE tmp_move;
    MOVE *list_ptr, *end_ptr, *best_ptr;

    tmp_move.u = movelist->u;
    best_val = tmp_move.m.mvv_lva;
    best_ptr = movelist;
    list_ptr = movelist + 1;
    end_ptr = movelist + len;

    while (list_ptr < end_ptr)
    {
        int cur_val = list_ptr->m.mvv_lva;
        if (cur_val > best_val)
        {
            best_val = cur_val;
            best_ptr = list_ptr;
        }
        list_ptr++;
    }
    movelist->u = best_ptr->u;
    best_ptr->u = tmp_move.u;
}

/*for the alternate search info display mode in the HMI*/
MOVE Search_Get_Current_Root_Move(void)
{
    return(curr_root_move);
}

/*returns whether a mate has been seen despite the noise setting.
  for each move of depth, the engine shall overlook the mate with a probability
  of "eval_noise".*/
static int NEVER_INLINE Search_Mate_Noise(int depth)
{
    int i;
    unsigned int prob, eval_real;

    prob = 100U;
    eval_real = 100U - ((unsigned) eval_noise); /*non-noise part*/
    depth /= 2; /*plies depth to moves depth*/

    for (i = 0; i < depth; i++)
    {
        prob *= eval_real;
        prob += 50U;         /*rounding*/
        prob /= 100U;        /*scale back to 0..100%*/
    }

    /*the probability of mate detection decreases with depth.*/
    if (Hw_Rand() % 101U > prob)
        return(0); /*mate overlooked*/

    return(1);
}

/*20 moves without capture or pawn move, that might tend drawish.
so start to ramp down the difference from 100% at move 20 to 10% target at move 50.
From some moves before move 50, the looming draw will also appear,
but if it has already been flattened before, that will not come as surprisingly.

flatten steadily as to encourage staying draw in worse positions, but don't
introduce a sudden drop which might cause panic in better situations.

note that this routine is ONLY called if fifty_moves >= NO_ACTION_PLIES already.*/
static int32_t NEVER_INLINE Search_Flatten_Difference(int32_t eval)
{
    int i, fifty_moves_search;

    /*basic endgames*/
    if ((Wpieces[0].next == NULL) || (Wpieces[0].next->next == NULL) ||
        (Bpieces[0].next == NULL) || (Bpieces[0].next->next == NULL))
    {
        return(eval);
    }

    for (i = Starting_Mv + 1, fifty_moves_search = fifty_moves; i <= mv_stack_p; i++, fifty_moves_search++)
    {
        MVST* p = &move_stack[i];
        if ((p->captured->type) /* capture */ || (p->move.m.flag > 1) /* pawn move */)
            return (eval); /*such moves will reset the drawish tendency*/

        /*50 moves draw detected*/
        if (fifty_moves_search >= 100)
            return(0);
    }

    /*90% discount during 60 plies (30 moves).*/
    eval *= (107 - fifty_moves_search); /*fifty_moves_search is >= NO_ACTION_PLIES*/
    eval /= (107 - NO_ACTION_PLIES);
    return(eval);
}

static int NEVER_INLINE Search_Quiescence(int alpha, int beta, enum E_COLOUR colour, int do_checks, int qs_depth)
{
    MOVE movelist[MAXCAPTMV];
    enum E_COLOUR next_colour;
    int e, score, i, move_cnt, actual_moves, t, recapt;
    int is_material_enough, n_checks, n_check_pieces;
    unsigned has_move;
#ifdef DEBUG_STACK
    if ((top_of_stack - ((size_t) &is_material_enough)) > max_of_stack)
    {
        max_of_stack = (top_of_stack - ((size_t) &is_material_enough));
    }
#endif
#ifdef G_NODES
    g_nodes++;
#endif
    if (colour==BLACK) {
        /*using has_move as dummy*/
        e = -Eval_Static_Evaluation(&is_material_enough, BLACK, &has_move, &has_move, &has_move);
        if (UNLIKELY(!is_material_enough))
            return 0;
        if (UNLIKELY(fifty_moves >= NO_ACTION_PLIES))
            e = Search_Flatten_Difference(e);
        /*try to delay bad positions and go for good positions faster,
          but don't change the eval sign*/
        if (e > 0)
        {
            e -= (mv_stack_p - Starting_Mv);
            if (e <= 0) e = 1;
        } else if (e < 0)
        {
            e += (mv_stack_p - Starting_Mv);
            if (e >= 0) e = -1;
        }
        /*prevent stack overflow*/
        if (UNLIKELY((mv_stack_p - Starting_Mv >= MAX_DEPTH+MAX_QIESC_DEPTH-1) ||
                     (time_is_up == TM_USER_CANCEL)))
        {
            return e;
        }

        /*in pre-search or after 4 plies QS, don't do check extensions.
          depth 0 cannot have checks because Negascout does not enter QS
          when in check, and the pre-search does not request QS check extension.*/
        if ((qs_depth < QS_CHECK_DEPTH) && (qs_depth > 0) && (do_checks != QS_NO_CHECKS))
            n_checks = Mvgen_Black_King_In_Check_Info(search_check_attacks_buf, &n_check_pieces);
        else
            n_checks = 0;

        if (n_checks == 0)
        {
            if (e >= beta)
                return beta;
            t = QUEEN_V + PAWN_V;
            if (move_stack[mv_stack_p].special == PROMOT)
                t += QUEEN_V - PAWN_V;
            if (e + t < alpha)
                return alpha;

            /*check for stalemate against lone king.
              needed in some endgames like these:
              8/8/1b5p/8/6P1/8/5k1K/8 w - - 0 1
              6K1/5P2/8/5q2/2k5/8/8/8 b - - 0 1*/
            if (Bpieces[0].next == NULL)
            {
                move_cnt = has_move =0;
                Mvgen_Add_Black_King_Moves(&Bpieces[0], movelist, &move_cnt);
                for (i = 0; i < move_cnt; i++)
                {
                    Search_Push_Status();
                    Search_Make_Move(movelist[i]);
                    if (!Mvgen_Black_King_In_Check())
                    {
                        /*no stalemate*/
                        Search_Retract_Last_Move();
                        Search_Pop_Status();
                        has_move = 1U;
                        break;
                    }
                    Search_Retract_Last_Move();
                    Search_Pop_Status();
                }
                if (!has_move)
                    return 0;
            }
            /*ignore underpromotion in quiescence - not worth the effort.*/
            move_cnt = Mvgen_Find_All_Black_Captures_And_Promotions(movelist, QUEENING);

            if (move_cnt == 0)
                return e;

            if (alpha < e)
                alpha = e;
        } else
        {
            /*in QS, drop underpromotion.*/
            move_cnt = Mvgen_Find_All_Black_Evasions(movelist, search_check_attacks_buf, n_checks, n_check_pieces, QUEENING);
        }
        next_colour = WHITE;
    } else {
        /*using has_move as dummy*/
        e = Eval_Static_Evaluation(&is_material_enough, WHITE, &has_move, &has_move, &has_move);
        if (UNLIKELY(!is_material_enough))
            return 0;
        if (UNLIKELY(fifty_moves >= NO_ACTION_PLIES))
            e = Search_Flatten_Difference(e);
        /*try to delay bad positions and go for good positions faster,
          but don't change the eval sign*/
        if (e > 0)
        {
            e -= (mv_stack_p - Starting_Mv);
            if (e <= 0) e = 1;
        } else if (e < 0)
        {
            e += (mv_stack_p - Starting_Mv);
            if (e >= 0) e = -1;
        }

        /*prevent stack overflow*/
        if (UNLIKELY((mv_stack_p - Starting_Mv >= MAX_DEPTH+MAX_QIESC_DEPTH-1) ||
                     (time_is_up == TM_USER_CANCEL)))
        {
            return e;
        }

        /*in pre-search or after 4 plies QS, don't do check extensions.
          depth 0 cannot have checks because Negascout does not enter QS
          when in check, and the pre-search does not request QS check extension.*/
        if ((qs_depth < QS_CHECK_DEPTH) && (qs_depth > 0) && (do_checks != QS_NO_CHECKS))
            n_checks = Mvgen_White_King_In_Check_Info(search_check_attacks_buf, &n_check_pieces);
        else
            n_checks = 0;

        if (n_checks == 0)
        {
            if (e >= beta)
                return beta;
            t = QUEEN_V + PAWN_V;
            if (move_stack[mv_stack_p].special == PROMOT)
                t += QUEEN_V - PAWN_V;
            if (e + t < alpha)
                return alpha;

            /*check for stalemate against lone king.
              needed in some endgames like these:
              8/8/1b5p/8/6P1/8/5k1K/8 w - - 0 1
              6K1/5P2/8/5q2/2k5/8/8/8 b - - 0 1*/
            if (Wpieces[0].next == NULL)
            {
                move_cnt = has_move = 0;
                Mvgen_Add_White_King_Moves(&Wpieces[0], movelist, &move_cnt);
                for (i = 0; i < move_cnt; i++)
                {
                    Search_Push_Status();
                    Search_Make_Move(movelist[i]);
                    if (!Mvgen_White_King_In_Check())
                    {
                        /*no stalemate*/
                        Search_Retract_Last_Move();
                        Search_Pop_Status();
                        has_move = 1U;
                        break;
                    }
                    Search_Retract_Last_Move();
                    Search_Pop_Status();
                }
                if (!has_move)
                    return 0;
            }
            /*ignore underpromotion in quiescence - not worth the effort.*/
            move_cnt = Mvgen_Find_All_White_Captures_And_Promotions(movelist, QUEENING);

            if (move_cnt == 0)
                return e;

            if (alpha < e)
                alpha = e;
        } else
        {
            /*in QS, drop underpromotion.*/
            move_cnt = Mvgen_Find_All_White_Evasions(movelist, search_check_attacks_buf, n_checks, n_check_pieces, QUEENING);
        }
        next_colour = BLACK;
    }

    Search_Swap_Best_To_Top(movelist, move_cnt);
    actual_moves = has_move = 0;
    recapt = (qs_depth < QS_RECAPT_DEPTH) ? 0 : move_stack[mv_stack_p].move.m.to;
    qs_depth++;

    /*continue QS despite timeout because otherwise, the pre-search
      would return nonsense instead of a fail-safe move. besides, apart from
      some pathological QS explosion positions, QS is quick anyway. Still
      check the time to trigger the watchdog and update the display.*/
    if (UNLIKELY(Time_Check(computer_side)))
        if (time_is_up == TM_NO_TIMEOUT)
            time_is_up = TM_TIMEOUT;

    for (i = 0; i < move_cnt; i++)
    {
        if (i == 1) /*first move is over, that usually cuts*/
            if (move_cnt >= 3) /*sort only if there are at least 2 more moves*/
                Search_Do_Sort(movelist + 1, move_cnt - 1);

        /*delta pruning, and recapture-only after 5 plies QS*/
        t = movelist[i].m.to;
        if (((e + PieceValFromType[board[t]->type] + DELTAMARGIN) < alpha) ||
            ((recapt != 0) && (recapt != t)))
        {
            /*if we are in check, it doesn't matter whether it's mate or
              considerable material loss, we are below alpha anyway.*/
            has_move = 1U;
            continue;
        }

        Search_Push_Status();
        Search_Make_Move(movelist[i]);

        if (Mvgen_King_In_Check(colour))
        {
            Search_Retract_Last_Move();
            Search_Pop_Status();
            continue;
        }

        actual_moves++;
        score = -Search_Quiescence(-beta, -alpha, next_colour, do_checks, qs_depth);
        Search_Retract_Last_Move();
        Search_Pop_Status();

        if (score >= beta)
            return beta;
        if (score > alpha)
            alpha = score;
    }
    /*mate found? only if no possible evasion moves have been pruned.*/
    if ((actual_moves == 0) && (n_checks != 0) && (has_move == 0) &&
        ((eval_noise <= 0) || (Search_Mate_Noise(mv_stack_p - Starting_Mv))) )
    {
        return(-INFINITY_ + (mv_stack_p - Starting_Mv));
    }

    return alpha;
}

static int NEVER_INLINE Search_Negamate(int depth, int alpha, int beta, enum E_COLOUR colour, int check_depth,
                                 LINE *restrict pline, MOVE *restrict blocked_movelist, int blocked_moves,
                                 int root_node, int in_check)
{
    MOVE movelist[MAXMV];
    MOVE dummy;
    LINE line;
    enum E_COLOUR next_colour;
    int i, a, move_cnt, actual_move_cnt, checking, tt_value;
#ifdef DEBUG_STACK
    if ((top_of_stack - ((size_t) &tt_value)) > max_of_stack)
    {
        max_of_stack = (top_of_stack - ((size_t) &tt_value));
    }
#endif

    pline->line_len = 0;

    /*prevent stack overflow.
      should never hit because the maximum mate search depth is 8 moves,
      which is mate in 15 plies plus 1 ply for scanning the final position.
      So, 16 plies in total, and 20 plies are allowed. But just in case
     something changes.*/
    if (UNLIKELY(mv_stack_p - Starting_Mv >= MAX_DEPTH-1 ))
    /* We are too deep */
        return(0);

    if (Hash_Check_For_Draw())
        return(0);
    if (colour == WHITE) /*slightly different way to use the hash tables*/
    {
        if (Hash_Check_TT(T_T, colour, alpha, beta, depth, move_stack[mv_stack_p].mv_pos_hash, &tt_value, &dummy))
            return(tt_value);
    } else /*black*/
    {
        if (Hash_Check_TT(Opp_T_T, colour, alpha, beta, depth, move_stack[mv_stack_p].mv_pos_hash, &tt_value, &dummy))
            return(tt_value);
    }

    /*first phase: get the moves, filter out the legal ones,
      prioritise check delivering moves, and if it is checkmate, return.*/

    /*the history is used differently here because the depth serves as level.
      This is possible because Negamate does not have selective deepening.*/
    if (in_check == 0)
        move_cnt = Mvgen_Find_All_Moves(movelist, depth, colour, UNDERPROM);
    else
    {
        int n_checks, n_check_pieces;
        /*in mating problems, always consider underpromotion even with evasions as
          this might be part of the puzzle.*/
        if (colour == WHITE)
        {
            n_checks = Mvgen_White_King_In_Check_Info(search_check_attacks_buf, &n_check_pieces);
            move_cnt = Mvgen_Find_All_White_Evasions(movelist, search_check_attacks_buf, n_checks, n_check_pieces, UNDERPROM);
        } else
        {
            n_checks = Mvgen_Black_King_In_Check_Info(search_check_attacks_buf, &n_check_pieces);
            move_cnt = Mvgen_Find_All_Black_Evasions(movelist, search_check_attacks_buf, n_checks, n_check_pieces, UNDERPROM);
        }
    }

    next_colour = Mvgen_Opp_Colour(colour);

    for (i = 0, actual_move_cnt = 0, checking = 0; i < move_cnt; i++)
    {
        if (blocked_moves)
        /*if we are searching for double solutions, we may have
          to filter out the blocked moves.*/
        {
            int j, move_is_blocked;
            for (j = 0, move_is_blocked = 0; j < blocked_moves; j++)
            {
                if (((movelist[i].u ^ blocked_movelist[j].u) & mv_move_mask.u) == 0)
                {
                    move_is_blocked = 1;
                    break;
                }
            }
            if (move_is_blocked)
            {
                movelist[i].m.flag = 0;
                movelist[i].m.mvv_lva = MVV_LVA_ILLEGAL;
                continue;
            }
        }

        Search_Push_Status();
        Search_Make_Move(movelist[i]);
        if (Mvgen_King_In_Check(colour))
        {
            movelist[i].m.flag = 0;
            movelist[i].m.mvv_lva = MVV_LVA_ILLEGAL;
        } else
        {
            /*if there is a legal move at depth == 0, then it isn't checkmate.*/
            if (depth == 0)
            {
                Search_Retract_Last_Move();
                Search_Pop_Status();
                return(0);
            }
            actual_move_cnt++;
            if (Mvgen_King_In_Check(next_colour))
            /*prioritise check giving moves*/
            {
                checking++;
                movelist[i].m.mvv_lva = MVV_LVA_CHECK;
            }
        }
        Search_Retract_Last_Move();
        Search_Pop_Status();
    }

    /*at depth==1, only check giving moves are tried. so at this point, there
    are no legal moves, otherwise we would have returned, and we are in check.
    Must be checkmate.*/
    if (depth == 0)
        return(-INFINITY_ + (mv_stack_p - Starting_Mv));

    if (actual_move_cnt == 0)
    {
        if (in_check)
            return(-INFINITY_ + (mv_stack_p - Starting_Mv));
        else
            return(0);
    }

    /*if no move delivers check, then it won't be checkmate.*/
    if ((depth <= check_depth) && (depth & 1))
    {
        if (checking == 0)
            return(0);
        else
        {
            /*if the depth is below the "from here on only checking moves" threshold,
            only consider the checking moves. only for odd depths because the side that
            is supposed to be mated always must have all possibilities evaluated.*/
            actual_move_cnt = checking;
        }
    }

    Search_Swap_Best_To_Top(movelist, move_cnt);

    if (UNLIKELY(Time_Check(computer_side)))
        if (time_is_up == TM_NO_TIMEOUT)
            time_is_up = TM_TIMEOUT;
    /*remember time_is_up is volatile and might also get set from the interrupt
    if the user cancels the computation.*/
    if (time_is_up != TM_NO_TIMEOUT)
        return(0);

    line.line_len = 0;
    a = alpha;
    /*second phase: iterate deeper.*/
    for (i = 0; i < actual_move_cnt; i++)
    {
        int score, giving_check;

        if (i == 1)
            Search_Do_Sort(movelist+1, move_cnt-1);

        Search_Push_Status();
        Search_Make_Move(movelist[i]);

        giving_check = (movelist[i].m.mvv_lva == MVV_LVA_CHECK) ? 1 : 0;
        /*there are no blocked moves from within this search, only from the root level.*/
        score = -Search_Negamate(depth-1, -beta, -a, next_colour, check_depth, &line,
                                 NULL /*no block list*/, 0 /*no blocked moves*/, 0 /*not root node*/,
                                 giving_check);
        Search_Retract_Last_Move();
        Search_Pop_Status();

        if (score > a)
        {
            a = score;
            /*update the PV*/
            pline->line_cmoves[0] = Mvgen_Compress_Move(movelist[i]);
            Util_Movelinecpy(pline->line_cmoves + 1, line.line_cmoves, line.line_len);
            pline->line_len = line.line_len + 1;

            if ((root_node) && (score > MATE_CUTOFF)) /*all we are looking for*/
                return(score);

            if (score >= beta)
            {
                MOVE current_move = movelist[i];

                /*the supplied move is only used for extended hash entry validation*/
                if (colour == WHITE)
                    Hash_Update_TT(    T_T, depth, score, CHECK_BETA, move_stack[mv_stack_p].mv_pos_hash, movelist[0]);
                else
                    Hash_Update_TT(Opp_T_T, depth, score, CHECK_BETA, move_stack[mv_stack_p].mv_pos_hash, movelist[0]);

                /*Update depth killers for non captures.
                  Quiet king moves generally don't work in sibling positions.*/
                if (board[current_move.m.to]->type == NO_PIECE)
                {
                    int last_moved_piece_type = board[current_move.m.from]->type;

                    if ((last_moved_piece_type != WKING) && (last_moved_piece_type != BKING))
                    {
                        CMOVE cmove = Mvgen_Compress_Move(current_move);

                        if (colour == BLACK)
                        {
                            if (B_Killers[0][depth] != cmove)
                            {
                                B_Killers[1][depth] = B_Killers[0][depth];
                                B_Killers[0][depth] = cmove;
                            }
                        } else
                        {
                            if (W_Killers[0][depth] != cmove)
                            {
                                W_Killers[1][depth] = W_Killers[0][depth];
                                W_Killers[0][depth] = cmove;
                            }
                        }
                    }
                }
                return(score);
            }
        }
    }/*for each node*/

    /*update transposition table*/
    if (a > alpha)
    {
        /*the supplied move is only used for extended hash entry validation*/
        if (colour == WHITE)
            Hash_Update_TT(    T_T, depth, a, EXACT, move_stack[mv_stack_p].mv_pos_hash, movelist[0]);
        else
            Hash_Update_TT(Opp_T_T, depth, a, EXACT, move_stack[mv_stack_p].mv_pos_hash, movelist[0]);
    } else
    {
        /*the supplied move is only used for extended hash entry validation*/
        if (colour == WHITE)
            Hash_Update_TT(    T_T, depth, a, CHECK_ALPHA, move_stack[mv_stack_p].mv_pos_hash, movelist[0]);
        else
            Hash_Update_TT(Opp_T_T, depth, a, CHECK_ALPHA, move_stack[mv_stack_p].mv_pos_hash, movelist[0]);
    }

    return(a);
}

/*only adjust stuff if there is something to adjust. moving the checks outside the for
loop increases the NPS rate by about 1%.
len is assumed to be > 0, which is always the case because there are always
pseudo-legal moves.*/
static void NEVER_INLINE Search_Adjust_Priorities(MOVE * restrict movelist, int len,
                                                  uint8_t * restrict should_iid,
                                                  MOVE pv_move, MOVE hash_move,
                                                  MOVE threat_move)
{
    #define NONE_ADJ 0U
    #define PV_ADJ   1U
    #define HS_ADJ   2U
    #define TR_ADJ   4U
    int i;
    unsigned int search_mode = NONE_ADJ, search_done;

    /*what we are looking for to adjust. only consider the hash move if it is different
      from the PV move, similarly for the threat move.*/
    if (pv_move.u != MV_NO_MOVE_MASK) search_mode = PV_ADJ;
    if ((hash_move.u != MV_NO_MOVE_MASK) && (hash_move.u != pv_move.u)) search_mode |= HS_ADJ;
    if ((threat_move.u != MV_NO_MOVE_MASK) && (threat_move.u != pv_move.u) && (threat_move.u != hash_move.u)) search_mode |= TR_ADJ;

    switch (search_mode)
    {
        case NONE_ADJ: /*0*/
            return;
        case PV_ADJ: /*1*/
            i = 0;
            do {
                if (((movelist[i].u ^ pv_move.u) & mv_move_mask.u) == 0)
                {
                    movelist[i].m.mvv_lva = MVV_LVA_PV; /* Move follows PV*/
                    *should_iid = 0;
                    return;
                }
                i++;
            } while (i < len);
            return;
        case HS_ADJ: /*2*/
            i = 0;
            do {
                if (((movelist[i].u ^ hash_move.u) & mv_move_mask.u) == 0)
                {
                    movelist[i].m.mvv_lva = MVV_LVA_HASH; /* Move from Hash table */
                    *should_iid = 0;
                    return;
                }
                i++;
            } while (i < len);
            return;
        case PV_ADJ | HS_ADJ: /*3*/
            search_done = 0;
            i = 0;
            do {
                if (((movelist[i].u ^ pv_move.u) & mv_move_mask.u) == 0)
                {
                    movelist[i].m.mvv_lva = MVV_LVA_PV; /* Move follows PV*/
                    *should_iid = 0;
                    if (search_done == HS_ADJ) /*both found*/
                        return;
                    search_done = PV_ADJ;
                } else if (((movelist[i].u ^ hash_move.u) & mv_move_mask.u) == 0)
                {
                    movelist[i].m.mvv_lva = MVV_LVA_HASH; /* Move from Hash table */
                    *should_iid = 0;
                    if (search_done == PV_ADJ) /*both found*/
                        return;
                    search_done = HS_ADJ;
                }
                i++;
            } while (i < len);
            return;
        case TR_ADJ: /*4*/
            i = 0;
            do {
                if (((movelist[i].u ^ threat_move.u) & mv_move_mask.u) == 0)
                {
                    movelist[i].m.mvv_lva = MVV_LVA_THREAT; /* Move found as threat from previous level Null Move search */
                    return;
                }
                i++;
            } while (i < len);
            return;
        case TR_ADJ | PV_ADJ: /*5*/
            search_done = 0;
            i = 0;
            do {
                if (((movelist[i].u ^ pv_move.u) & mv_move_mask.u) == 0)
                {
                    movelist[i].m.mvv_lva = MVV_LVA_PV; /* Move follows PV*/
                    *should_iid = 0;
                    if (search_done == TR_ADJ) /*both found*/
                        return;
                    search_done = PV_ADJ;
                } else if (((movelist[i].u ^ threat_move.u) & mv_move_mask.u) == 0)
                {
                    movelist[i].m.mvv_lva = MVV_LVA_THREAT; /* Move found as threat from previous level Null Move search */
                    if (search_done == PV_ADJ) /*both found*/
                        return;
                    search_done = TR_ADJ;
                }
                i++;
            } while (i < len);
            return;
        case TR_ADJ | HS_ADJ: /*6*/
            search_done = 0;
            i = 0;
            do {
                if (((movelist[i].u ^ hash_move.u) & mv_move_mask.u) == 0)
                {
                    movelist[i].m.mvv_lva = MVV_LVA_HASH; /* Move from Hash table */
                    *should_iid = 0;
                    if (search_done == TR_ADJ) /*both found*/
                        return;
                    search_done = HS_ADJ;
                } else if (((movelist[i].u ^ threat_move.u) & mv_move_mask.u) == 0)
                {
                    movelist[i].m.mvv_lva = MVV_LVA_THREAT; /* Move found as threat from previous level Null Move search */
                    if (search_done == HS_ADJ) /*both found*/
                        return;
                    search_done = TR_ADJ;
                }
                i++;
            } while (i < len);
            return;
        case TR_ADJ | HS_ADJ | PV_ADJ: /*7*/
            search_done = 0;
            i = 0;
            do {
                if (((movelist[i].u ^ pv_move.u) & mv_move_mask.u) == 0)
                {
                    movelist[i].m.mvv_lva = MVV_LVA_PV; /* Move follows PV*/
                    *should_iid = 0;
                    if (search_done == (HS_ADJ | TR_ADJ)) /*all found*/
                        return;
                    search_done |= PV_ADJ;
                } else if (((movelist[i].u ^ hash_move.u) & mv_move_mask.u) == 0)
                {
                    movelist[i].m.mvv_lva = MVV_LVA_HASH; /* Move from Hash table */
                    *should_iid = 0;
                    if (search_done == (PV_ADJ | TR_ADJ)) /*all found*/
                        return;
                    search_done |= HS_ADJ;
                } else if (((movelist[i].u ^ threat_move.u) & mv_move_mask.u) == 0)
                {
                    movelist[i].m.mvv_lva = MVV_LVA_THREAT; /* Move found as threat from previous level Null Move search */
                    if (search_done == (PV_ADJ | HS_ADJ)) /*all found*/
                        return;
                    search_done |= TR_ADJ;
                }
                i++;
            } while (i < len);
            return;
    }
}

static int NEVER_INLINE Search_Endgame_Reduct(void)
{
    /*basic endgames*/
    if ((Wpieces[0].next == NULL) || (Wpieces[0].next->next == NULL) ||
        (Bpieces[0].next == NULL) || (Bpieces[0].next->next == NULL))
    {
        return(0);
    }

    /*promotion threat*/
    for (int i = A2; i <= H2; i += FILE_DIFF)
    {
        if ((board[i]->type == BPAWN) ||
            (board[i + 5*RANK_DIFF]->type == WPAWN))
        {
            return(0);
        }
    }
    return(1);
}

/* -------------------------------- NEGA SCOUT ALGORITHM -------------------------------- */

static int NEVER_INLINE FUNC_NOGCSE
Search_Negascout(int CanNull, int level, LINE *restrict pline, MOVE *restrict mlst,
                 int n, int depth, int alpha, int beta, enum E_COLOUR colour,
                 int *restrict best_move_index, int is_pv_node, int being_in_check,
                 MOVE threat_move, int following_pv)
{
    const int mate_score = INFINITY_ - (mv_stack_p - Starting_Mv);

    pline->line_len = 0;
    *best_move_index = TERMINAL_NODE;

    /*mate distance pruning*/
    if (alpha >= mate_score) return alpha;
    if (beta <= -mate_score) return beta;

    if (depth <= 0)
    {   /*node is a terminal node  */
        if (eval_noise < HIGH_EVAL_NOISE)
            return Search_Quiescence(alpha, beta, colour, QS_CHECKS, 0);
        else
            return Search_Quiescence(alpha, beta, colour, QS_NO_CHECKS, 0);
    } else
    {
        static int root_move_index;
        MOVE x2movelst[MAXMV];
        MOVE threat_best, null_best, hash_best;
        enum E_COLOUR next_colour;
        int i, e, t, a, x2movelen, next_depth, node_moves;
        int iret, is_material_enough, n_check_pieces;
        unsigned is_endgame, w_passed_mask, b_passed_mask;
        LINE line;
        uint8_t should_iid=1, hash_move_mode, level_gt_1, node_pruned_moves;

        level_gt_1 = (level > 1) ? 1 : 0;

#ifdef DEBUG_STACK
        if ((top_of_stack - ((size_t) &node_pruned_moves)) > max_of_stack)
        {
            max_of_stack = (top_of_stack - ((size_t) &node_pruned_moves));
        }
#endif

        hash_best.u = MV_NO_MOVE_MASK;
#ifdef G_NODES
        g_nodes++;
#endif
        /*prevent stack overflow*/
        if (UNLIKELY(mv_stack_p - Starting_Mv >= MAX_DEPTH-1 )) { /* We are too deep */
            return Search_Quiescence(alpha, beta, colour, QS_CHECKS, 0);
        }

        /* Check Transposition Table for a match */
        if (!is_pv_node) {
            if (level & 1) { /* Our side to move */
                if (Hash_Check_TT(T_T, colour, alpha, beta, depth, move_stack[mv_stack_p].mv_pos_hash, &t, &hash_best)) {
                    if (hash_best.u != MV_NO_MOVE_MASK)
                    {
                        pline->line_cmoves[0] = Mvgen_Compress_Move(hash_best);
                        pline->line_len = 1;
                    }
                    return t;
                }
            } else { /* Opponent time to move */
                if (Hash_Check_TT(Opp_T_T, colour, alpha, beta, depth, move_stack[mv_stack_p].mv_pos_hash, &t, &hash_best)) {
                    if (hash_best.u != MV_NO_MOVE_MASK)
                    {
                        pline->line_cmoves[0] = Mvgen_Compress_Move(hash_best);
                        pline->line_len = 1;
                    }
                    return t;
                }
            }
        } else if (level_gt_1)
        /*for PV nodes, don't return because that causes PV truncation. Only use the
          hash best move for move ordering.*/
        {
            if (level & 1) /* Our side to move */
                Hash_Check_TT_PV(T_T, colour, depth, move_stack[mv_stack_p].mv_pos_hash, &t, &hash_best);
            else           /* Opponent time to move */
                Hash_Check_TT_PV(Opp_T_T, colour, depth, move_stack[mv_stack_p].mv_pos_hash, &t, &hash_best);
        }

        /*level 2 has a dedicated move cache.*/
        if ((level == 2) && (hash_best.u == MV_NO_MOVE_MASK))
            hash_best = Mvgen_Decompress_Move(opp_move_cache[root_move_index]);

        /*note that the contents of the passed pawn masks are only defined for is_endgame != 0.*/
        if (colour == BLACK) {
            e = -Eval_Static_Evaluation(&is_material_enough, BLACK, &is_endgame, &w_passed_mask, &b_passed_mask);
            next_colour = WHITE;
        } else {
            e = Eval_Static_Evaluation(&is_material_enough, WHITE, &is_endgame, &w_passed_mask, &b_passed_mask);
            next_colour = BLACK;
        }
        if (!is_material_enough)
        /*if this node has insufficient material, that cannot change further
          down towards the leaves of the search tree.*/
        {
          MOVE smove;
          smove.u = MV_NO_MOVE_MASK;
          if (level & 1) {
            Hash_Update_TT(T_T, depth, 0, EXACT, move_stack[mv_stack_p].mv_pos_hash, smove);
          } else {
            Hash_Update_TT(Opp_T_T, depth, 0, EXACT, move_stack[mv_stack_p].mv_pos_hash, smove);
          }
          return 0;
        }
        if (UNLIKELY(fifty_moves >= NO_ACTION_PLIES))
            e = Search_Flatten_Difference(e);

        null_best.u = MV_NO_MOVE_MASK;
        if ((!is_pv_node) && (!being_in_check))
        {
            /*Reverse Futility Pruning*/
            if ((move_stack[mv_stack_p].move.m.mvv_lva < MVV_LVA_TACTICAL) &&
                (depth < RVRS_FUTIL_D) && (e - RVRS_FutilMargs[depth] >= beta) &&
                ((is_material_enough >= EG_PIECES) || Search_Endgame_Reduct()))
            {
                return e;
            }
            /*Null search*/
            if (CanNull && (depth >= NULL_START_DEPTH) && (is_material_enough >= NULL_PIECES))
            {
                int next_depth = depth - (3 + depth / 4) - (e >= beta + PAWN_V);
                MOVE smove;
                smove.u = MV_NO_MOVE_MASK;

                /*this can fall right into QS which does not do check evasions at
                  QS level 0. But this is OK because the other side cannot be in check
                  given that it is actually our turn here, i.e. without null move.*/
                t = -Search_Negascout(0, level + 1, &line, x2movelst, 0, next_depth, -beta, -beta + 1, next_colour, &iret, is_pv_node, 0, smove, 0);
                if (t >= beta)
                    return t;
                if (iret >= 0)
                    null_best.u = x2movelst[iret].u;
            }
        }

        /*late move generation*/
        hash_move_mode = 0;
        if (n == 0)
        {
            /*defer that if we have a hash move - a beta cutoff is likely.*/
            if ((hash_best.u == MV_NO_MOVE_MASK) || (following_pv))
            {
                MOVE GPVmove;

                n = Mvgen_Find_All_Moves(mlst, level-1, colour, UNDERPROM);

                /* Adjust move priorities */
                if ((following_pv) && (GlobalPV.line_len > level-1))
                    GPVmove = Mvgen_Decompress_Move(GlobalPV.line_cmoves[level-1]);
                else
                    GPVmove.u = MV_NO_MOVE_MASK;

                Search_Adjust_Priorities(mlst, n, &should_iid, GPVmove, hash_best, threat_move);
            } else
            {
                hash_move_mode = 1;
                should_iid = 0;
            }
        }

        if (should_iid && (depth > IID_DEPTH) && (level_gt_1))
        {
            /* Internal Iterative Deepening
            level > 1: no IID in the root node because the pre-sorting in
            Search_Play_And_Sort_Moves() has already done that.
            should_iid would have been set to false if we had had a hash or PV move available.*/
            Search_Negascout(CanNull,level,&line,mlst,n, depth/3,
                             alpha,beta,colour,&iret,is_pv_node,being_in_check,threat_move,following_pv);
            if (iret >= 0)
                mlst[iret].m.mvv_lva = MVV_LVA_HASH;
        }

        a = alpha;
        node_moves = node_pruned_moves = 0;

        if (level_gt_1)
        {
            /*root move list is already sorted in the main IID loop.*/
            Search_Swap_Best_To_Top(mlst, n);
        }

        /*if we are not in a PV node and there is a hash best move, then this
          move is 90% likely to cause a cutoff here. The move list has not
          yet been generated, so n is 0, and the hash move is the only move
          for now. It has been validated for pseudo legality upon retrieval
          from the hash table. The in-check verification remains to do.*/
        if (hash_move_mode)
        {
            mlst[0].u = hash_best.u;
            n = 2;
        }

        if (UNLIKELY(Time_Check(computer_side)))
            if (time_is_up == TM_NO_TIMEOUT)
                time_is_up = TM_TIMEOUT;

        for (i = 0; i < n; i++)
        {
            /* foreach child of node */
            int LastMoveToSquare, LastMovePieceType, capture_1, capture_2,
                curr_move_follows_pv, can_reduct, n_checks;

            if (level_gt_1) /*initial move list is already sorted*/
            {
                if (i == 1)
                {
                    /*even later move generation*/
                    if (hash_move_mode)
                    {
                        MOVE GPVmove;
                        GPVmove.u = MV_NO_MOVE_MASK;

                        n = Mvgen_Find_All_Moves(mlst, level-1, colour, UNDERPROM);
                        /*if there is only one pseudo legal move, that must have been
                          the hash move which has already been tried.*/
                        if (n <= 1)
                            break;

                        /* Adjust move priorities */
                        Search_Adjust_Priorities(mlst, n, &should_iid, GPVmove, hash_best, threat_move);
                        Search_Do_Sort(mlst, n); /*hash move will be at the top*/
                    } else
                        Search_Do_Sort(mlst + 1, n - 1);
                }
            } else /*level 1 is root moves.*/
            {
                root_move_index = i;
                /*save the current root move for the live search mode in the HMI*/
                curr_root_move.u = mlst[i].u;
            }
            Search_Push_Status();
            Search_Make_Move(mlst[i]);
            if (Mvgen_King_In_Check(colour))
            {
                Search_Retract_Last_Move();
                Search_Pop_Status();
                continue;
            }
#ifdef MOVE_ANALYSIS
            DIFPrint
            printf("%s ",Play_Translate_Moves(mlst[i]));
#endif
            threat_best.u = MV_NO_MOVE_MASK;
            if (Hash_Check_For_Draw())
            {
                if ((mv_stack_p < CONTEMPT_END) && (game_started_from_0))
                {
                    if (colour == computer_side)
                        t = CONTEMPT_VAL;
                    else
                        t = -CONTEMPT_VAL;
                } else
                    t = 0;
            } else
            {
                if (colour == BLACK)
                {
                    /* if our move just played gives check, generate evasions and do not reduce depth so we can search deeper */
                    n_checks = Mvgen_White_King_In_Check_Info(search_check_attacks_buf, &n_check_pieces);
                    if (n_checks) { /* early move generation */
                        can_reduct = 0;
                        if ((depth <= 4) && (eval_noise < HIGH_EVAL_NOISE))
                            /*track checks if the search tree were to end, but don't replicate high-level trees.
                            This depth limitation was not in the original version, and that caused the CT800 to
                            reach only 4 plies depth in 20 seconds during the middlegame, sometimes.
                            The original NG-Play did not have this problem, at least not that severe, because
                            it also has megabytes of hash tables, and a fast PC processor.*/
                            next_depth = depth;
                        else
                            next_depth = depth - 1;
                        x2movelen = Mvgen_Find_All_White_Evasions(x2movelst, search_check_attacks_buf, n_checks, n_check_pieces, UNDERPROM);
                    } else {
                        can_reduct = (!being_in_check) && (mlst[i].m.mvv_lva < MVV_LVA_TACTICAL) &&
                                      ((is_material_enough >= EG_PIECES) || Search_Endgame_Reduct());
                        /*futility pruning*/
                        if ( can_reduct && (!is_pv_node) && (depth < FUTIL_DEPTH) && (e+FutilityMargins[depth] < a) ) {
                            Search_Retract_Last_Move();
                            Search_Pop_Status();
                            node_pruned_moves = 1U; /*a pruned legal move still is a legal move - for the stalemate recognition at the end of this routine.*/
                            continue;
                        }
                        x2movelen = 0;
                        next_depth = depth-1;
                        if (time_is_up == TM_NO_TIMEOUT)
                        {
                            /*special attention to mutual passed pawn races*/
                            if ((is_endgame) && (depth <= 2) && (mlst[i].m.flag == BPAWN) &&
                                (b_passed_mask & board_file_mask[mlst[i].m.to]) && (eval_noise < HIGH_EVAL_NOISE))
                            {
                                next_depth = depth;
                            } else if ((is_pv_node) && (depth <= PV_ADD_DEPTH) && (eval_noise < HIGH_EVAL_NOISE))
                            {
                                /*make sure that capture chains don't push things just out of the horizon.
                                  checks are deepened anyway.*/
                                capture_1 = move_stack[mv_stack_p].captured->type;
                                if (capture_1)
                                {
                                    capture_2 = move_stack[mv_stack_p-1].captured->type;
                                    /*there cannot be a buffer underun because the current search move
                                      has been made so that mv_stack_p is minimum 1 at this point.*/
                                    if (capture_2)
                                    {
                                        if (ExchangeValue[capture_1] == ExchangeValue[capture_2])
                                        /*unequal captures would either be a bad idea, then the quiescence show
                                          a loss anyway, or a win, which the quiescence also shows. Only equal
                                          captures could cause a horizon effect delay.*/
                                           next_depth = depth;
                                    }
                                }
                            }
                        }
                    }
                } else { /* colour == WHITE */
                    /* if our move just played gives check do not reduce depth so we can search deeper */
                    n_checks = Mvgen_Black_King_In_Check_Info(search_check_attacks_buf, &n_check_pieces);
                    if (n_checks) { /* early move generation */
                        can_reduct = 0;
                        if ((depth <= 4) && (eval_noise < HIGH_EVAL_NOISE))
                            /*track checks if the search tree were to end, but don't replicate high-level trees.
                            This depth limitation was not in the original version, and that caused the CT800 to
                            reach only 4 plies depth in 20 seconds during the middlegame, sometimes.
                            The original NG-Play did not have this problem, at least not that severe, because
                            it also has megabytes of hash tables, and a fast PC processor.*/
                            next_depth = depth;
                        else
                            next_depth = depth-1;
                        x2movelen=Mvgen_Find_All_Black_Evasions(x2movelst, search_check_attacks_buf, n_checks, n_check_pieces, UNDERPROM);
                    } else {
                        can_reduct = (!being_in_check) && (mlst[i].m.mvv_lva < MVV_LVA_TACTICAL) &&
                                      ((is_material_enough >= EG_PIECES) || Search_Endgame_Reduct());
                        /*futility pruning*/
                        if ( can_reduct && (!is_pv_node) && (depth < FUTIL_DEPTH) && (e+FutilityMargins[depth] < a) ) {
                            Search_Retract_Last_Move();
                            Search_Pop_Status();
                            node_pruned_moves = 1U; /*a pruned legal move still is a legal move - for the stalemate recognition at the end of this routine.*/
                            continue;
                        }
                        x2movelen = 0;
                        next_depth = depth-1;
                        if (time_is_up == TM_NO_TIMEOUT)
                        {
                            /*special attention to mutual passed pawn races*/
                            if ((is_endgame) && (depth <= 2) && (mlst[i].m.flag == WPAWN) &&
                                (w_passed_mask & board_file_mask[mlst[i].m.to]) && (eval_noise < HIGH_EVAL_NOISE))
                            {
                                next_depth = depth;
                            } else if ((is_pv_node) && (depth <= PV_ADD_DEPTH) && (eval_noise < HIGH_EVAL_NOISE))
                            {
                                /*make sure that capture chains don't push things just out of the horizon.
                                  checks are deepened anyway.*/
                                capture_1 = move_stack[mv_stack_p].captured->type;
                                if (capture_1)
                                {
                                    capture_2 = move_stack[mv_stack_p-1].captured->type;
                                    /*there cannot be a buffer underun because the current search move
                                      has been made so that mv_stack_p is minimum 1 at this point.*/
                                    if (capture_2)
                                    {
                                        if (ExchangeValue[capture_1] == ExchangeValue[capture_2])
                                        /*unequal captures would either be a bad idea, then the quiescence show
                                          a loss anyway, or a win, which the quiescence also shows. Only equal
                                          captures could cause a horizon effect delay.*/
                                           next_depth = depth;
                                    }
                                }
                            }
                        }
                    }
                }
                curr_move_follows_pv = 0;
                if ((following_pv) && (GlobalPV.line_len > level-1) &&
                    (Mvgen_Compress_Move(mlst[i]) == GlobalPV.line_cmoves[level-1]))
                {
                    curr_move_follows_pv = 1;
                }
                if (node_moves == 0) { /* First move to search- full window [-beta,-alpha] used */
                    t = (beta > a + 1) ? PV_NODE : CUT_NODE;
                    t = -Search_Negascout(1, level+1, &line, x2movelst, x2movelen, next_depth, -beta, -a, next_colour, &iret, t, n_checks, null_best, curr_move_follows_pv);
                } else {
                    if (can_reduct && (node_moves >= LMR_MOVES) && (depth >= LMR_DEPTH_LIMIT)) {
                        /* LMR - Search with reduced depth and scout window [-alpha-1,-alpha].
                           Don't fall straight into quiescence at depth==3 because there's no
                           check evasion detection at QS level 0.*/
                        t = ((node_moves < 2 * LMR_MOVES) || (depth <= 3)) ? depth-2 : depth-3;
                        t = -Search_Negascout(1, level+1, &line, x2movelst, x2movelen, t, -a-1, -a, next_colour, &iret, CUT_NODE, n_checks, null_best, curr_move_follows_pv);
                    } else t = a + 1;  /* Ensure that re-search is done. */

                    if (t > a) {
                        /* Search normal depth and scout window [-alpha-1,-alpha] */
                        t = -Search_Negascout(1, level+1, &line, x2movelst, x2movelen, next_depth, -a-1, -a, next_colour, &iret, CUT_NODE, n_checks, null_best, curr_move_follows_pv);
                        if ((t > a) && (t < beta)) {
                            /* re-search using full window for PV nodes */
                            t = -Search_Negascout(1, level+1, &line, x2movelst, x2movelen, next_depth, -beta, -a, next_colour, &iret, PV_NODE, n_checks, null_best, curr_move_follows_pv);
                        }
                    }
                }
                if (iret >= 0) /* Update Best defense for later use in PV line update */
                    threat_best.u = x2movelst[iret].u;
            }
            LastMoveToSquare  = mlst[i].m.to;
            LastMovePieceType = board[LastMoveToSquare]->type;
            Search_Retract_Last_Move();
            Search_Pop_Status();

            if (time_is_up != TM_NO_TIMEOUT)
                return a;

            /*if we are in level 1, then store the best answer from level 2
              for the next main depth iteration.*/
            if ((!level_gt_1) && (threat_best.u != MV_NO_MOVE_MASK))
                opp_move_cache[i] = Mvgen_Compress_Move(threat_best);

            /*the following constitutes alpha-beta pruning*/
            if (t > a) {
#ifdef MOVE_ANALYSIS
                DIFPrint
                printf("(%d),  ",t);
#endif
                a = t;
                *best_move_index = i;
                /* Update Principal Variation */
                if (threat_best.u != MV_NO_MOVE_MASK) {
                    pline->line_cmoves[0] = Mvgen_Compress_Move(threat_best);
                    Util_Movelinecpy(pline->line_cmoves + 1, line.line_cmoves, line.line_len);
                    pline->line_len = line.line_len + 1;
                } else {
                    pline->line_len = 0;
                }
                if (a >= beta) { /*-- cut-off --*/
#ifdef DBGCUTOFF
                    if (node_moves == 0) { /* First move of search */
                        cutoffs_on_1st_move++;
                    }
                    total_cutoffs++;
#endif
                    /*Update depth killers for non captures.
                      not for kings because that's usually castling, moving out of
                      check or some tempo/manoeuvring thing, i.e. different across
                      search tree branches.*/
                    if ((board[LastMoveToSquare]->type == NO_PIECE) &&
                        (LastMovePieceType != WKING) && (LastMovePieceType != BKING))
                    {
                        CMOVE cmove = Mvgen_Compress_Move(mlst[i]);

                        if (colour == BLACK)
                        {
                            if (B_Killers[0][level - 1] != cmove)
                            {
                                B_Killers[1][level - 1] = B_Killers[0][level - 1];
                                B_Killers[0][level - 1] = cmove;
                            }
                        } else
                        {
                            if (W_Killers[0][level - 1] != cmove)
                            {
                                W_Killers[1][level - 1] = W_Killers[0][level - 1];
                                W_Killers[0][level - 1] = cmove;
                            }
                        }
                    }

                    /* Update Transposition table */
                    if (level & 1) {
                        Hash_Update_TT(T_T, depth, a, CHECK_BETA, move_stack[mv_stack_p].mv_pos_hash, mlst[i]);
                    } else {
                        Hash_Update_TT(Opp_T_T, depth, a, CHECK_BETA, move_stack[mv_stack_p].mv_pos_hash, mlst[i]);
                    }
                    return a;
                }
                /* Update history values for non captures */
                if (board[LastMoveToSquare]->type == 0) {
                    /* Non capture move increased alpha - increase (piece,square) history value */
                    if (colour == BLACK) {
                        if (B_history[LastMovePieceType - BPAWN][LastMoveToSquare] == 0) {
                            B_history[LastMovePieceType - BPAWN][LastMoveToSquare] = -MAX_DEPTH;
                        }
                        B_history[LastMovePieceType - BPAWN][LastMoveToSquare] += depth;
                        if (B_history[LastMovePieceType - BPAWN][LastMoveToSquare] >= 0 )
                            B_history[LastMovePieceType - BPAWN][LastMoveToSquare] = -1;
                    } else {
                        if (W_history[LastMovePieceType - WPAWN][LastMoveToSquare] == 0) {
                            W_history[LastMovePieceType - WPAWN][LastMoveToSquare] = -MAX_DEPTH;
                        }
                        W_history[LastMovePieceType - WPAWN][LastMoveToSquare] += depth;
                        if (W_history[LastMovePieceType - WPAWN][LastMoveToSquare] >= 0 )
                            W_history[LastMovePieceType - WPAWN][LastMoveToSquare] = -1;
                    }
                }
            }
            node_moves++;
        }/* for each node */

        if (node_moves == 0) /*no useful and legal moves*/
        {
            if (!node_pruned_moves) /*there are no legal moves.*/
            {
                if (being_in_check) /*mate */
                {
                    if ((eval_noise <= 0) || (Search_Mate_Noise(mv_stack_p - Starting_Mv)))
                        a = -mate_score;
                    else
                        a = e; /*mate overlooked*/
                } else /*stalemate */
                    a = 0;
            } else
            /*there are legal moves, but they were all cut away in the
              futility pruning.*/
            {
                /*'a' just stays alpha because no move has improved alpha.*/
            }
            *best_move_index = TERMINAL_NODE;
        }
        /* Update Transposition table */
        if (a > alpha)
        {
            if ( *best_move_index != TERMINAL_NODE )
                hash_best = mlst[*best_move_index];
            else
                hash_best.u = MV_NO_MOVE_MASK;

            if (level & 1)
                Hash_Update_TT(T_T, depth, a, EXACT, move_stack[mv_stack_p].mv_pos_hash, hash_best);
            else
                Hash_Update_TT(Opp_T_T, depth, a, EXACT, move_stack[mv_stack_p].mv_pos_hash, hash_best);
        } else
        {
            hash_best.u = MV_NO_MOVE_MASK;
            if (level & 1)
                Hash_Update_TT(T_T, depth, a, CHECK_ALPHA, move_stack[mv_stack_p].mv_pos_hash, hash_best);
            else
                Hash_Update_TT(Opp_T_T, depth, a, CHECK_ALPHA, move_stack[mv_stack_p].mv_pos_hash, hash_best);
        }
        return a;
    }
}

/*perform a shallow search of 1 ply + QS at root to sort the root move list.*/
static int NEVER_INLINE Search_Play_And_Sort_Moves(MOVE *restrict movelist, int len,
                                                   enum E_COLOUR Nextcolour, int *restrict score_drop)
{
    int sortV[MAXMV], i;

#ifdef DEBUG_STACK
    if ((top_of_stack - ((size_t) &i)) > max_of_stack)
    {
        max_of_stack = (top_of_stack - ((size_t) &i));
    }
#endif

    /*this is just in case that during future works, this routine gets called with an empty move list
    (len == 0), i.e. mate or stalemate. usually, this should have been handled before calling this routine.
    but this way, the program would resign immediately, making it obvious that something is wrong.
    otherwise, if len==0, we would return an uninitialised value.*/
    sortV[0] = -INFINITY_;

    for (i = 0; i < len; i++)
    {
        int current_score;

        if (movelist[i].m.flag == 0) /*illegal move - should not happen*/
            current_score = -INFINITY_;
        else
        {
            Search_Push_Status();
            Search_Make_Move(movelist[i]);

            if (Hash_Check_For_Draw())
            {
                if ((mv_stack_p < CONTEMPT_END) && (game_started_from_0))
                    current_score = CONTEMPT_VAL;
                else
                    current_score = 0;
            } else
                current_score = -Search_Quiescence(-INFINITY_, INFINITY_, Nextcolour, QS_NO_CHECKS, 0);

            Search_Retract_Last_Move();
            Search_Pop_Status();
        }
        sortV[i] = current_score;
    }

    /*sort movelist using sortV.*/
    if (len > 1)
    {
        Search_Do_Sort_Value(movelist, sortV, len);
        *score_drop = sortV[0] - sortV[1];
    } else
        *score_drop = SORT_THRESHOLD;

    return(sortV[0]);
}

#ifdef PC_PRINTF
static void Search_Print_Move_Output(int depth, int score, int goodmove)
{
#ifdef MOVE_ANALYSIS
    printf("\n");
#endif
    printf("%3d%7.2lf%7.2lf   ",depth, 0.01*score, SECONDS_PASSED);
    Search_Print_PV_Line(&GlobalPV,goodmove);
}
#endif

/*tests whether the position is checkmate for the relevant colour.*/
static int NEVER_INLINE Search_Is_Checkmate(enum E_COLOUR colour)
{
    MOVE check_attacks[CHECKLISTLEN];
    MOVE movelist[MAXMV];
    int i, move_cnt;
    int n_checks, n_check_pieces;
    if (colour == WHITE)
    {
        if (Mvgen_White_King_In_Check())
        {
            int non_checking_move = 0;
            n_checks = Mvgen_White_King_In_Check_Info(check_attacks, &n_check_pieces);
            move_cnt = Mvgen_Find_All_White_Evasions(movelist, check_attacks, n_checks, n_check_pieces, UNDERPROM);
            /*is there a legal move?*/
            for (i = 0; i < move_cnt; i++)
            {
#ifdef G_NODES
                g_nodes++;
#endif
                Search_Push_Status();
                Search_Make_Move(movelist[i]);
                if (!Mvgen_White_King_In_Check()) non_checking_move = 1;
                Search_Retract_Last_Move();
                Search_Pop_Status();
                if (non_checking_move) return(0); /*that move gets out of check: no checkmate.*/
            }
            return(1); /*no move gets the white king out of check: checkmate.*/
        }
    } else
    {
        if (Mvgen_Black_King_In_Check())
        {
            int non_checking_move = 0;
            n_checks = Mvgen_Black_King_In_Check_Info(check_attacks, &n_check_pieces);
            move_cnt = Mvgen_Find_All_Black_Evasions(movelist, check_attacks, n_checks, n_check_pieces, UNDERPROM);
            /*is there a legal move?*/
            for (i = 0; i < move_cnt; i++)
            {
#ifdef G_NODES
                g_nodes++;
#endif
                Search_Push_Status();
                Search_Make_Move(movelist[i]);
                if (!Mvgen_Black_King_In_Check()) non_checking_move = 1;
                Search_Retract_Last_Move();
                Search_Pop_Status();
                if (non_checking_move) return(0); /*that move gets out of check: no checkmate.*/
            }
            return(1); /*no move gets the black king out of check: checkmate.*/
        }
    }
    return(0); /*no king is in check, so it can't be checkmate.*/
}

static void Search_Reset_History(void)
{
    Util_Memzero(W_history, sizeof(W_history));
    Util_Memzero(B_history, sizeof(B_history));
    Util_Memzero(W_Killers, sizeof(W_Killers));
    Util_Memzero(B_Killers, sizeof(B_Killers));
}


/* ---------- global functions ----------*/


void Search_Make_Move(MOVE amove)
{
    int xy1, xy2, flag, ptype1, xyc, xydif;
    MVST* p;

    xy1  = amove.m.from;
    xy2  = amove.m.to;
    flag = amove.m.flag;
    ptype1 = board[xy1]->type;
    mv_stack_p++;
    p = &move_stack[mv_stack_p];
    p->move.u = amove.u;
    en_passant_sq = 0;
    p->special = NORMAL;

    if (ptype1 == WPAWN) {
        xydif = xy2-xy1;
        if (((xydif == 11) || (xydif == 9)) && (board[xy2]->type == 0)) { /*En Passant*/
            xyc = xy2-10;
            p->captured = board[xyc];
            p->capt = xyc;
        } else {
            if ((flag >= WKNIGHT) && (flag < WKING)) { /* white pawn promotion. Piece in flag */
                board[xy1]->type = flag;
                p->special = PROMOT;
            }
            xyc = xy2;
            p->captured = board[xyc];
            p->capt = xyc;
            if (xydif == 20) {
                if ((board[xy2+1]->type == BPAWN) || (board[xy2-1]->type == BPAWN)) {
                    en_passant_sq = xy1+10;
                }
            }
        }
    } else if (ptype1 == BPAWN) {
        xydif = xy2-xy1;
        if (((xydif == -11) || (xydif == -9)) && (board[xy2]->type == 0)) { /*En Passant*/
            xyc = xy2+10;
            p->captured = board[xyc];
            p->capt = xyc;
        } else {
            if ((flag >= BKNIGHT) && (flag < BKING)) { /* black pawn promotes to flag */
                board[xy1]->type = flag;
                p->special = PROMOT;
            }
            xyc = xy2;
            p->captured = board[xyc];
            p->capt = xyc;
            if (xydif == -20) {
                if ((board[xy2+1]->type == WPAWN) || (board[xy2-1]->type == WPAWN)) {
                    en_passant_sq = xy1-10;
                }
            }
        }
    } else {
        xyc = xy2;
        p->captured = board[xyc];
        p->capt = xyc;
    }
    /* Captured piece struct modified */
    if (board[xyc]->type) {
        PIECE *piece_p = board[xyc];
        piece_p->xy = 0;
        piece_p->prev->next = piece_p->next;
        if (piece_p->next) {
            piece_p->next->prev = piece_p->prev;
        }
        board[xyc] = &empty_p;
    }
    board[xy1]->xy = xy2;    /* Moving piece struct modified */
    board[xy2] = board[xy1];
    board[xy1] = &empty_p;
    /* Update Flags */
    if (board[xy2]->type > BLACK) {
        gflags |= BLACK_MOVED; /*black move*/
        if (ptype1 == BROOK) {
            if (xy1 == A8) {
                gflags |= BRA8MOVED; /*  bra8moved=1;*/
                if (gflags & BRH8MOVED) /*if both rooks have moved, set the king also moved*/
                    gflags |= BKMOVED;
            } else if (xy1 == H8) {
                gflags |= BRH8MOVED; /*  brh8moved=1;*/
                if (gflags & BRA8MOVED) /*if both rooks have moved, set the king also moved*/
                    gflags |= BKMOVED;
            }
        } else if (ptype1 == BKING) {
            gflags |= (BKMOVED | BRA8MOVED | BRH8MOVED);
            bking = xy2;
            if (xy1 == E8) {
                if (xy2 == G8) { /* black short castle */
                    board[F8] = board[H8];
                    board[F8]->xy = F8;
                    board[H8] = &empty_p;
                    p->special = CASTL;
                    gflags |= BCASTLED;
                } else if (xy2 == C8) { /* black long castle */
                    board[D8] = board[A8];
                    board[D8]->xy = D8;
                    board[A8] = &empty_p;
                    p->special = CASTL;
                    gflags |= BCASTLED;
                }
            }
            if (xy2 == G8) {
                if ((board[F8]->type == BROOK) && (board[H8]->xy == 0)) {
                    gflags |= BCASTLED; /*  artificial BHasCastled=1 */
                }
            }
        }
    } else {
        gflags &= ~BLACK_MOVED; /*white move*/
        if (ptype1 == WROOK) {
            if (xy1 == A1) {
                gflags |= WRA1MOVED;
                if (gflags & WRH1MOVED) /*if both rooks have moved, set the king also moved*/
                    gflags |= WKMOVED;
            } else if (xy1 == H1) {
                gflags |= WRH1MOVED;
                if (gflags & WRA1MOVED) /*if both rooks have moved, set the king also moved*/
                    gflags |= WKMOVED;
            }
        } else if (ptype1 == WKING) {
            wking = xy2;
            if (xy1 == E1) {
                gflags |= (WKMOVED | WRA1MOVED | WRH1MOVED);
                if (xy2 == G1) { /* white short castle */
                    board[F1] = board[H1];
                    board[F1]->xy = F1;
                    board[H1] = &empty_p;
                    p->special = CASTL;
                    gflags |= WCASTLED;
                } else if (xy2 == C1) { /* white long castle */
                    board[D1] = board[A1];
                    board[D1]->xy = D1;
                    board[A1] = &empty_p;
                    p->special = CASTL;
                    gflags |= WCASTLED;
                }
            }
            if (xy2 == G1) {
                if ((board[F1]->type == WROOK) && (board[H1]->xy == 0)) {
                    gflags |= WCASTLED; /*  artificial WHasCastled=1 */
                }
            }
        }
    }
    p->mv_pos_hash = Hash_Get_Position_Value(&(p->mv_pawn_hash));
}

void Search_Retract_Last_Move(void)
{
    int xy1=move_stack[mv_stack_p].move.m.from;
    int xy2=move_stack[mv_stack_p].move.m.to;
    int cpt=move_stack[mv_stack_p].capt;
    board[xy1] = board[xy2];
    board[xy1]->xy = xy1;
    board[xy2] = &empty_p;
    board[cpt] = move_stack[mv_stack_p].captured;
    if (board[cpt] != &empty_p) {
        board[cpt]->xy = cpt;
        board[cpt]->prev->next = board[cpt];
        if (board[cpt]->next)
            board[cpt]->next->prev = board[cpt];
    }

    if (move_stack[mv_stack_p].special==PROMOT) {
        if (xy1>=A7) { /* white pawn promotion */
            board[xy1]->type  = WPAWN;
        } else  { /* black pawn promotion */
            board[xy1]->type  = BPAWN;
        }
    } else {
        if (board[xy1]->type==WKING) {
            wking=xy1;
        } else if (board[xy1]->type==BKING) {
            bking=xy1;
        }
        if (move_stack[mv_stack_p].special==CASTL) {
            if (xy1==E1) { /* white castle */
                if (xy2==G1) {
                    board[H1] = board[F1];
                    board[H1]->xy = H1;
                    board[F1] = &empty_p;
                } else if (xy2==C1) {
                    board[A1] = board[D1];
                    board[A1]->xy = A1;
                    board[D1] = &empty_p;
                } else Hmi_Reboot_Dialogue("err: castle moves,", "must reboot.");
            } else if (xy1==E8) { /* black castle */
                if (xy2==G8) {
                    board[H8] = board[F8];
                    board[H8]->xy = H8;
                    board[F8] = &empty_p;
                } else if (xy2==C8) {
                    board[A8] = board[D8];
                    board[A8]->xy = A8;
                    board[D8] = &empty_p;
                } else Hmi_Reboot_Dialogue("err: castle moves,", "must reboot.");
            } else Hmi_Reboot_Dialogue("err: castle moves,", "must reboot.");
        }
    }
    mv_stack_p--;
}

void Search_Try_Move(MOVE amove) /* No ep/castle flags checked */
{
    int xyc, xydif;
    int xy1 =amove.m.from;
    int xy2 =amove.m.to;
    int flag=amove.m.flag;
    mv_stack_p++;

    move_stack[mv_stack_p].move.u = amove.u;
    move_stack[mv_stack_p].special = NORMAL;
    if (board[xy1]->type==WPAWN) {
        xydif = xy2-xy1;
        if ((xydif==11 || xydif==9) && (board[xy2]->type==0)) { /*En Passant*/
            xyc = xy2-10;
            move_stack[mv_stack_p].captured = board[xyc];
            move_stack[mv_stack_p].capt = xyc;
        } else {
            if (flag>=WKNIGHT && flag<WKING) { /* white pawn promotion. Piece in flag */
                board[xy1]->type = flag;
                move_stack[mv_stack_p].special=PROMOT;
            }
            xyc=xy2;
            move_stack[mv_stack_p].captured = board[xyc];
            move_stack[mv_stack_p].capt = xyc;
        }
    } else if (board[xy1]->type==BPAWN) {
        xydif = xy2-xy1;
        if ((xydif==-11 || xydif==-9) && (board[xy2]->type==0)) { /*En Passant*/
            xyc = xy2+10;
            move_stack[mv_stack_p].captured = board[xyc];
            move_stack[mv_stack_p].capt = xyc;
        } else {
            if (flag>=BKNIGHT && flag<BKING) { /* black pawn promotes to flag */
                board[xy1]->type = flag;
                move_stack[mv_stack_p].special=PROMOT;
            }
            xyc=xy2;
            move_stack[mv_stack_p].captured = board[xyc];
            move_stack[mv_stack_p].capt = xyc;
        }
    } else {
        xyc=xy2;
        move_stack[mv_stack_p].captured = board[xyc];
        move_stack[mv_stack_p].capt = xyc;
    }
    board[xyc]->xy = 0;      /* Captured piece struct modified */
    board[xy1]->xy = xy2;    /* Moving piece struct modified */
    board[xyc] = &empty_p;
    board[xy2] = board[xy1];
    board[xy1] = &empty_p;
    if (board[xy2]->type==WKING) {
        wking = xy2;
        if (xy1==E1) {
            if (xy2==G1) { /* white short castle */
                board[F1] = board[H1];
                board[F1]->xy = F1;
                board[H1] = &empty_p;
                move_stack[mv_stack_p].special = CASTL;
            } else if (xy2==C1) { /* white long castle */
                board[D1] = board[A1];
                board[D1]->xy = D1;
                board[A1] = &empty_p;
                move_stack[mv_stack_p].special = CASTL;
            }
        }
    } else if (board[xy2]->type==BKING) {
        bking = xy2;
        if (xy1==E8) {
            if (xy2==G8) { /* black short castle */
                board[F8] = board[H8];
                board[F8]->xy = F8;
                board[H8] = &empty_p;
                move_stack[mv_stack_p].special = CASTL;
            } else if (xy2==C8) { /* black long castle */
                board[D8] = board[A8];
                board[D8]->xy = D8;
                board[A8] = &empty_p;
                move_stack[mv_stack_p].special = CASTL;
            }
        }
    }
}

/*50 moves rule: every move will draw except captures and pawn moves,
  and if these had been useful, they would have been chosen before.
  just don't make a move that allows the opponent to capture. that
  would still be a draw, but some GUIs might have issues in noticing
  that. besides, it would look silly.
  however, if the opponent has hung a piece and now it isn't draw, then
  we'll catch that because it's only about pre-sorting the list. on the
  other hand, if everything draws, then the list order will not be changed
  in search.*/
static void NEVER_INLINE Search_Sort_50_Moves(MOVE *restrict player_move, MOVE *restrict movelist, int move_cnt, enum E_COLOUR colour)
{
    if (fifty_moves >= 99)
    {
        MOVE opp_movelist[MAXMV];
        enum E_COLOUR next_colour = Mvgen_Opp_Colour(colour);
        int i;

        for (i = 0; i < move_cnt; i++)
        {
            int moving_piece, moving_to;

            if (movelist[i].m.mvv_lva == MVV_LVA_MATE_1) /*mate already found*/
                continue;
            moving_piece = board[movelist[i].m.from]->type;
            moving_to = movelist[i].m.to;

            if ((moving_piece != WPAWN) && (moving_piece != BPAWN) &&
                (board[movelist[i].m.to]->type == NO_PIECE))
            {
                int opp_capture_moves, is_checking, take_moving_piece = 0;

                Search_Push_Status();
                Search_Make_Move(movelist[i]);

                /*prefer check giving moves. if those lead to legal
                  captures, they will be de-prioritised anyway.*/
                if (Mvgen_King_In_Check(next_colour))
                    is_checking = 1;
                else
                    is_checking = 0;

                /*find the opponent's capture moves.*/
                opp_capture_moves = Mvgen_Find_All_Captures_And_Promotions(opp_movelist, next_colour, QUEENING);

                /*if there are any, don't count those that would put the
                  opponent in check. that increases the chance to find
                  suitable own moves.*/
                if (opp_capture_moves > 0)
                {
                    int k, legal_opp_moves = 0;
                    for (k = 0; k < opp_capture_moves; k++)
                    {
                        Search_Push_Status();
                        Search_Make_Move(opp_movelist[k]);
                        if (!Mvgen_King_In_Check(next_colour))
                        {
                            legal_opp_moves = 1;
                            /*can take the piece that we have moved?*/
                            if (opp_movelist[k].m.to == moving_to)
                                take_moving_piece = 1;
                        }
                        Search_Retract_Last_Move();
                        Search_Pop_Status();
                    }
                    if (legal_opp_moves == 0) /*no legal captures.*/
                        opp_capture_moves = 0;
                }
                if (opp_capture_moves == 0)
                {
                    /*a checking move that can't be answered by a capture is
                      bullet-proof - it might even be checkmate if the opponent
                      does not know that checkmate has priority over the 50 moves
                      draw. otherwise, at least a move where there are no captures.*/
                    movelist[i].m.mvv_lva = MVV_LVA_50_OK + is_checking;
                } else
                {
                    /*a check giving move that can be answered by a capture
                      will capture the check-giving piece!*/
                    movelist[i].m.mvv_lva = MVV_LVA_50_NOK - is_checking - take_moving_piece;
                }

                Search_Retract_Last_Move();
                Search_Pop_Status();
            } else
                movelist[i].m.mvv_lva = MVV_LVA_50_NOK;
        }
        Search_Do_Sort(movelist, move_cnt);
        player_move->u = MV_NO_MOVE_MASK; /*trash PV*/
    }
}

int NEVER_INLINE Search_Get_Root_Move_List(MOVE *restrict movelist, int *restrict move_cnt, enum E_COLOUR colour, int mate_check)
{
    enum E_COLOUR next_colour;
    int i, mv_len, actual_move_cnt, n_checks, n_check_pieces;

    if (colour == WHITE)
    {
        next_colour = BLACK;
        n_checks = Mvgen_White_King_In_Check_Info(search_check_attacks_buf, &n_check_pieces);
        if (n_checks != 0)
            mv_len = Mvgen_Find_All_White_Evasions(movelist, search_check_attacks_buf, n_checks, n_check_pieces, UNDERPROM);
        else
            mv_len = Mvgen_Find_All_White_Moves(movelist, NO_LEVEL, UNDERPROM);
    } else
    {
        next_colour = WHITE;
        n_checks = Mvgen_Black_King_In_Check_Info(search_check_attacks_buf, &n_check_pieces);
        if (n_checks != 0)
            mv_len = Mvgen_Find_All_Black_Evasions(movelist, search_check_attacks_buf, n_checks, n_check_pieces, UNDERPROM);
        else
            mv_len = Mvgen_Find_All_Black_Moves(movelist, NO_LEVEL, UNDERPROM);
    }

    for (i = 0, actual_move_cnt = 0; i < mv_len; i++)
    {
        /*filter out all moves that would put or let our king in check*/
        Search_Push_Status();
        Search_Make_Move(movelist[i]);
        if (Mvgen_King_In_Check(colour))
        {
            Search_Retract_Last_Move();
            Search_Pop_Status();
            movelist[i].m.flag = 0;
            movelist[i].m.mvv_lva = MVV_LVA_ILLEGAL;
            continue;
        }
        /*rank mate in 1 high: useful for pathological test positions like this:
          1QqQqQq1/r6Q/Q6q/q6Q/B2q4/q6Q/k6K/1qQ1QqRb w - -
          1Qq1qQrB/K6k/Q6q/b2Q4/Q6q/q6Q/R6q/1qQqQqQ1 b - -
          1qQ1QqRb/k6K/q6Q/B2q4/q6Q/Q6q/r6Q/1QqQqQq1 w - -
          1qQqQqQ1/R6q/q6Q/Q6q/b2Q4/Q6q/K6k/1Qq1qQrB b - -
          doesn't cost significant time in normal play, and the CT800 shall
          always be able to deliver mate no matter what.*/
        if (mate_check)
            if (Search_Is_Checkmate(next_colour))
                movelist[i].m.mvv_lva = MVV_LVA_MATE_1;
        Search_Retract_Last_Move();
        Search_Pop_Status();
        actual_move_cnt++;
    }

    /*the illegal moves get sorted to the end of the list.*/
    Search_Do_Sort(movelist, mv_len);
    *move_cnt = actual_move_cnt;
    return(n_checks);
}

/*set up the eval/search noise for the following search.*/
static void NEVER_INLINE Search_Setup_Noise(void)
{
    uint64_t noise_option = CFG_GET_OPT(CFG_NOISE_MODE);

    /*option is in 10% steps*/
    eval_noise = (int)(noise_option >> CFG_NOISE_OFFSET);
    eval_noise *= 10;
    if (eval_noise > 100) /*should not happen*/
        eval_noise = 100;
}

/*never inline this one since this isn't called in hot loops anway.*/
enum E_COMP_RESULT NEVER_INLINE
Search_Get_Best_Move(MOVE *restrict answer_move, MOVE player_move, int32_t full_move_time, enum E_COLOUR colour)
{
    int ret_mv_idx=0, move_cnt, is_material_enough, in_check, mate_in_1, is_analysis;
    MOVE movelist[MAXMV];
    LINE line;

    time_is_up = TM_NO_TIMEOUT;
    COMPILER_BARRIER;

#ifdef G_NODES
    g_nodes = 0;
#endif

#ifdef DBGCUTOFF
    cutoffs_on_1st_move = total_cutoffs = 0ULL;
#endif

    answer_move->u = 0;
    mate_in_1 = 0;

    is_analysis = CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_ANA);

    Search_Setup_Noise();
    Search_Reset_History();

    is_material_enough = Eval_Setup_Initial_Material();
    if (!is_material_enough) /*draw due to insufficient material*/
        return(COMP_MAT_DRAW);

    Starting_Mv = mv_stack_p;
    in_check = Search_Get_Root_Move_List(movelist, &move_cnt, colour, MATE_CHECK);

    if (move_cnt == 0)
    {
        if (in_check)
            return (COMP_MATE);
        else
            return(COMP_STALE);
    }

    if (Book_Is_Line(&ret_mv_idx, movelist, move_cnt))
    {
        game_info.valid = EVAL_BOOK;
        answer_move->u = movelist[ret_mv_idx].u;
        return(COMP_MOVE_FOUND);
    } else
    {
        int32_t reduced_move_time;
        MOVE no_threat_move, decomp_move;
        int i, d, sort_max, pv_hit = 0, score_drop, pos_score, nscore;
        CMOVE failsafe_cmove;

        curr_root_move.u = MV_NO_MOVE_MASK;
        no_threat_move.u = MV_NO_MOVE_MASK;

        /*if 50 moves draw is close, re-sort the list*/
        Search_Sort_50_Moves(&player_move, movelist, move_cnt, colour);

        if (hash_clear_counter < MAX_AGE_CNT) hash_clear_counter++;
        else hash_clear_counter = 0;

        if (full_move_time >= 500L) /*drop age clearing under extreme time pressure*/
            Hash_Cut_Tables(hash_clear_counter);

        /*if too much time has been used, don't start another iteration as it will not finish anyway.*/
        if (!is_analysis)
        {
            if (in_check) /*there are not many moves anyway*/
            {
                full_move_time /= 2;
                Time_Override_Stop_Time(full_move_time);
            }
            reduced_move_time = (full_move_time * 55L) / 100L;
        } else
            reduced_move_time = full_move_time;

        /*immediate mate found?*/
        if (movelist[0].m.mvv_lva == MVV_LVA_MATE_1)
        {
            mate_in_1 = 1;
            GlobalPV.line_cmoves[0] = Mvgen_Compress_Move(movelist[0]);
            GlobalPV.line_len = 1;
            if (fifty_moves < 100)
                game_info.eval = sort_max = pos_score = INF_MATE_1;
            else
                game_info.eval = sort_max = pos_score = 0;
            game_info.valid = EVAL_MOVE;
            game_info.depth = 1;
            player_move.u = MV_NO_MOVE_MASK; /*trash PV*/
            score_drop = 2 * EASY_THRESHOLD;
            /*save the best move from the pre-scan*/
            failsafe_cmove = GlobalPV.line_cmoves[0];
            if (!is_analysis)
            {
                answer_move->u = movelist[0].u;
                return(COMP_MOVE_FOUND);
            }
        } else
        {
            MOVE hash_best;
            int dummy;

            hash_best.u = MV_NO_MOVE_MASK;
            /*check with alpha=+INF and beta=-INF so that only the depth check
              is actually performed.*/
            if (!Hash_Check_TT(T_T, colour, INFINITY_, -INFINITY_, PRE_DEPTH, move_stack[mv_stack_p].mv_pos_hash, &dummy, &hash_best))
                 hash_best.u = MV_NO_MOVE_MASK;

             if ((player_move.u != MV_NO_MOVE_MASK) && (GlobalPV.line_len >= 3) &&
                 (GlobalPV.line_cmoves[1] == Mvgen_Compress_Move(player_move)))
            {
                /*shift down the global PV*/
                for (i = 0; i < GlobalPV.line_len - 2; i++)
                    GlobalPV.line_cmoves[i] = GlobalPV.line_cmoves[i+2];
                GlobalPV.line_len -= 2;
                if (GlobalPV.line_len > 1) /*else the pre-search is better*/
                    pv_hit = 1;
                decomp_move = Mvgen_Decompress_Move(GlobalPV.line_cmoves[0]);
                Search_Find_Put_To_Top(movelist, move_cnt, decomp_move);
            } else
            {
                /*the opponent didn't follow the PV, so trash it.*/
                GlobalPV.line_len = 0;
                GlobalPV.line_cmoves[0] = MV_NO_MOVE_CMASK;
            }

            /*do the move presorting at depth=1.*/
            sort_max = Search_Play_And_Sort_Moves(movelist, move_cnt, Mvgen_Opp_Colour(colour), &score_drop);
#ifdef PC_PRINTF
            printf("\nInitial Eval:%d\n",sort_max);
#endif
            game_info.valid = EVAL_MOVE;

            /*prepare the game info structure. if the move time is about 0, then the iterative
              deepening loop will not be finished even for depth 2.*/
            if (!pv_hit)
            {
                GlobalPV.line_cmoves[0] = Mvgen_Compress_Move(movelist[0]);
                GlobalPV.line_len = 1;
                pos_score = game_info.eval = sort_max;
                game_info.depth = PRE_DEPTH;
            } else /*PV hit*/
            {
                if (game_info.last_valid_eval != NO_RESIGN)
                {
                    /*adjust possible mate distance*/
                    if (game_info.last_valid_eval > MATE_CUTOFF)
                        game_info.eval = game_info.last_valid_eval + 2;
                    else if (game_info.last_valid_eval < -MATE_CUTOFF)
                        game_info.eval = game_info.last_valid_eval - 2;
                    else
                        game_info.eval = game_info.last_valid_eval;
                } else
                    game_info.eval = sort_max;
                pos_score = game_info.eval;
                game_info.depth = GlobalPV.line_len;

                /*if this is a forced move and we have a PV anyway, just play the move.*/
                if ((move_cnt < 2) && (!is_analysis))
                {
#ifdef PC_PRINTF
                    printf("Anticipated forced move.\r\n");
#endif
                    *answer_move = Mvgen_Decompress_Move(GlobalPV.line_cmoves[0]);
                    return(COMP_MOVE_FOUND);
                }
            }
            /*save the best move from the pre-scan*/
            failsafe_cmove = Mvgen_Compress_Move(movelist[0]);

            /*put the hash move (if available) to the top before doing so with the
              PV move - if both are equal, nothing happens, and otherwise, the hash move
              will come at place 2.*/
            if (hash_best.u != MV_NO_MOVE_MASK)
                Search_Find_Put_To_Top(movelist, move_cnt, hash_best);
        }

#ifdef PC_PRINTF
        printf("\nDepth Eval  Seconds Principal Variation  \n ---  ----  ------- -------------------\n");
#endif

        /* If opponent answered following previous PV, add the computer reply choice first in the moves list*/
        if (pv_hit)
        {
            decomp_move = Mvgen_Decompress_Move(GlobalPV.line_cmoves[0]);
            Search_Find_Put_To_Top(movelist, move_cnt, decomp_move);
#ifdef PC_PRINTF
            printf("Anticipated move.\r\n");
#endif
        }

        /*look whether the easy move detection is
          a) a PV hit
          OR
          b) mate in 1
          OR
          c) bad enough to be realistic: yes, the opponent might just hang a piece. But it could also be a trap.
          AND
          d) good enough: maybe in the deep search, we know we have a mate, then don't be content with a piece.*/
        if (!(
            ((pv_hit) && (failsafe_cmove == GlobalPV.line_cmoves[0])) ||
            (mate_in_1) ||
            ((game_info.last_valid_eval != NO_RESIGN) &&
                ((sort_max - game_info.last_valid_eval) < EASY_MARGIN_UP) &&
                ((sort_max - game_info.last_valid_eval) > EASY_MARGIN_DOWN))
           ))
        {
            score_drop = 0;
        }

        /*clear the level 2 move cache.*/
        Util_Memzero(opp_move_cache, sizeof(opp_move_cache));

        /*reduce CPU speed after the presort (if applicable).*/
        Hw_Throttle_Speed();

        nscore = pos_score;

        if (!is_analysis)
        {
            int32_t disp_time = Hw_Get_System_Time();

            curr_root_move = Mvgen_Decompress_Move(GlobalPV.line_cmoves[0]);
            Hmi_Update_Alternate_Screen(pos_score, PRE_DEPTH, &GlobalPV);
            /*a full display update takes about 6 ms, so maybe add this*/
            disp_time = Hw_Get_System_Time() - disp_time;
            if (disp_time > 1L) /*probably just timer interrupt*/
                Time_Add_Conf_Delay(disp_time + 1L);
        }

        for (d = START_DEPTH; d < MAX_DEPTH; d++)
        { /* Iterative deepening method*/
            const int alpha_full = -INFINITY_;
            const int beta_full  =  INFINITY_;
            int alpha, beta;

            /*set aspiration window.*/
            if (d >= ID_WINDOW_DEPTH)
            {
                alpha = nscore - ID_WINDOW_SIZE;
                if (alpha < alpha_full) alpha = alpha_full;
                beta  = nscore + ID_WINDOW_SIZE;
                if (beta > beta_full) beta = beta_full;
            } else
            {
                /*use full window at low depth.*/
                alpha = alpha_full;
                beta  = beta_full;
            }

            /*widen window until neither fail high nor low.*/
            for (;;)
            {
                nscore = Search_Negascout(0, 1, &line, movelist, move_cnt, d, alpha, beta, colour,
                                          &ret_mv_idx, PV_NODE, in_check, no_threat_move, 1);

                /*search with full window should not fail, but
                  just for robustness.*/
                if ((alpha == alpha_full) && (beta == beta_full))
                    break;

                COMPILER_BARRIER;
                if (time_is_up != TM_NO_TIMEOUT)
                    break;
                COMPILER_BARRIER;

                if (nscore <= alpha) /*fail low*/
                {
                    /*note that a fail low implies ret_mv_idx == -1 because
                      the Negascout initialises the return index to that value
                      and only sets it up when a move raises alpha, which no
                      move does in case of a fail low.*/
                    alpha = alpha_full;
                } else if (nscore >= beta) /*fail high*/
                {
                    beta = beta_full;
                    if (ret_mv_idx > 1)
                    {
                        /*don't accept as new PV because the re-search will be
                          with half open window, and the PV will guide the search
                          faster to useful limits. But make sure that its root move
                          is not ranked lower down the list than 2nd place.*/
                        MOVE ret_move = movelist[ret_mv_idx];
                        Search_Find_Put_To_Top_Root(movelist + 1, opp_move_cache + 1, move_cnt - 1, ret_move);
                    }
                } else
                    break;
            }

            if (ret_mv_idx >= 0)
            {
#ifdef PC_PRINTF
                int exclamation = 0;

#endif
                /*retain the PV if possible. this helps the move ordering and is
                  especially useful with PV hits.*/
                int copy_line_pv = 0;
                /*first move of PV changed, or new PV is at least as long?*/
                if ((GlobalPV.line_cmoves[0] != Mvgen_Compress_Move(movelist[ret_mv_idx])) ||
                    (GlobalPV.line_len <= line.line_len + 1))
                {
                    copy_line_pv = 1;
                } else
                {
                    /*something in the new, but shorter PV changed?*/
                    for (i = 0; i < line.line_len; i++)
                    {
                        if (GlobalPV.line_cmoves[i+1] != line.line_cmoves[i])
                        {
                            copy_line_pv = 1;
                            break;
                        }
                    }
                }

                if (copy_line_pv)
                {
                    game_info.valid = EVAL_MOVE;
                    game_info.eval = pos_score = nscore;
                    game_info.depth = d;

                    decomp_move = movelist[ret_mv_idx];
                    GlobalPV.line_cmoves[0] = Mvgen_Compress_Move(decomp_move);
                    Util_Movelinecpy(GlobalPV.line_cmoves + 1, line.line_cmoves, line.line_len);
                    GlobalPV.line_len = line.line_len + 1;
                    Search_Find_Put_To_Top_Root(movelist, opp_move_cache, move_cnt, decomp_move);
                }
#ifdef PC_PRINTF
                exclamation = (d > START_DEPTH) && ((pos_score - sort_max) > PV_CHANGE_THRESH);
                Search_Print_Move_Output(d, pos_score, exclamation);
#endif
                if (is_analysis)
                {
                    int32_t time_passed = Time_Passed();
                    Hmi_Update_Analysis_Screen(time_passed, pos_score, d, &GlobalPV);
                    if (time_passed > 5L*MILLISECONDS)
                        /*don't annoy the user with a constant BEEP during the first few plies depth*/
                        Hmi_Signal(HMI_MSG_ATT);
                    Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_ANA, HW_MSG_PARAM_BACK_CONF);
                } else
                {
                    int32_t disp_time = Hw_Get_System_Time();

                    Hmi_Update_Alternate_Screen(pos_score, d, &GlobalPV);
                    /*a full display update takes about 6 ms, so maybe add this*/
                    disp_time = Hw_Get_System_Time() - disp_time;
                    if (disp_time > 1L) /*probably just timer interrupt*/
                        Time_Add_Conf_Delay(disp_time + 1L);
                }

                if ((((pos_score > MATE_CUTOFF) || (pos_score < -MATE_CUTOFF) || (move_cnt < 2)) && (!is_analysis))
                    || (time_is_up != TM_NO_TIMEOUT))
                    break;
            }
            /*if the pre-sorting has shown an outstanding move, and this move is still at the head of the PV,
            then just do it. Saves time and prevents the opponent from having a certain ponder hit in case he
            is using pondering.
            if we have an easy move that also is a PV hit, then the PV is retained if it is longer than
            the "easy depth", which is the desired effect.*/
            if ((score_drop >= EASY_THRESHOLD) && (d >= EASY_DEPTH) &&
                (failsafe_cmove == GlobalPV.line_cmoves[0]) &&
                (!is_analysis))
            {
                break;
            }
            if (Time_Passed() > reduced_move_time) /*more than 55% already used*/
            {
                time_is_up = TM_TIMEOUT;
                break;
            }
        } /*for (d=...) end of Iterative Deepening loop*/
#ifdef DBGCUTOFF
        printf("\n Cutoff on 1st move is %4.2lf%% \n", 100.0*((double)cutoffs_on_1st_move)/((double)total_cutoffs) );
#endif

        COMPILER_BARRIER;
        if (is_analysis)
        {
            /*the question now is: why are we here?
            big question indeed, but fortunately, the chess program has only three possible answers
            at this point. yes, being a chess program makes life pretty easy sometimes.
            a) analysis mode hit the maximum searching depth, OR
            b) the 9 hours thinking time are over, OR
            c) the user pressed the GO button.
            if case c), we don't want to idle around, but in case a) and b), we want.*/
            if ((time_is_up == TM_NO_TIMEOUT) ||  /*case a)*/
                (Time_Check(computer_side))) /*case b)*/
            {
                /*redundant in case a), but in case b), we want to reset that flag
                and wait for the user to hit GO, which will drive it to 1.*/
                time_is_up = TM_NO_TIMEOUT;

                /*we're idling along, so get the system speed down - that saves energy.*/
                Hw_Set_Speed(SYSTEM_SPEED_LOW, SYSTEM_MODE_COMP, CLK_FORCE_AUTO);
                COMPILER_BARRIER;
                while ((!Time_Check(computer_side)) && (time_is_up == TM_NO_TIMEOUT))
                {
                    /*do nothing. the current time will be updated on the display once per second.*/
                    Time_Delay(10UL, SLEEP_ALLOWED);
                }
                COMPILER_BARRIER;
                while (time_is_up == TM_NO_TIMEOUT)
                {
                    /*wait for the user to hit GO*/
                    Time_Delay(10UL, SLEEP_ALLOWED);
                    Hw_Trigger_Watchdog();
                }
            }
        }

        if ((pos_score < -dynamic_resign_threshold) && (!is_analysis))
        {
            /*computer resigns, but still return the move found in case the player wants to play it out*/
            *answer_move = Mvgen_Decompress_Move(GlobalPV.line_cmoves[0]);
            return(COMP_RESIGN);
        }
    }
    *answer_move = Mvgen_Decompress_Move(GlobalPV.line_cmoves[0]);
    return(COMP_MOVE_FOUND);
}

/*the mating solver.*/
static enum E_COMP_RESULT NEVER_INLINE
Search_Get_Mate_Solution(int max_d, MOVE *blocked_movelist, int blocked_moves, LINE *pline, enum E_COLOUR colour)
{
    int res, move_cnt, actual_move_cnt, i, in_check;
    int Alpha = 0;
    int Beta  = INFINITY_;
    int check_depth;
    MOVE movelist[MAXMV];

#ifdef G_NODES
    g_nodes = 0;
#endif

    pline->line_len = 0;
    in_check = Mvgen_King_In_Check(colour);

    Starting_Mv = mv_stack_p;
    move_cnt = Mvgen_Find_All_Moves(movelist, NO_LEVEL, colour, UNDERPROM);
    actual_move_cnt = 0;

    for (i = 0; i < move_cnt; i++)
    {
        int j, move_deleted = 0;
        /*filter out blocked moves. used for finding double solutions.*/
        for (j = 0; j < blocked_moves; j++)
        {
            if (((movelist[i].u ^ blocked_movelist[j].u) & mv_move_mask.u) == 0)
            {
                move_deleted = 1;
                break;
            }
        }
        if (move_deleted)
            continue;
        /*filter out all moves that would put or let our king in check*/
        Search_Push_Status();
        Search_Make_Move(movelist[i]);
        if (Mvgen_King_In_Check(colour)) {
            Search_Retract_Last_Move();
            Search_Pop_Status();
            continue;
        }
        Search_Retract_Last_Move();
        Search_Pop_Status();
        actual_move_cnt++;
    }
    if (actual_move_cnt == 0)
        return(COMP_NO_MOVE);

    /*first try to look for a mate when only considering check-delivering
      moves in the last "check_depth" moves. the idea is that the last
      few plies in a mating problem usually is a series of checks, so limit
      the moves for the attacking side to checks. the defending side, of
      course, always has the full tange of answer moves.
      if the approach of "all attacking moves deliver check" does not work
      out, then try to allow non-checking moves in the first ply and then only
      checks. if that doesn't work, allow all moves for the first two attack
      plies, and then only checks. and so on. it is amazing how much time this
      little scheme can save.
      since the last ply must deliver check (or else it cannot be mate),
      check_depth has to stay greater than zero.*/
    for (check_depth = max_d, res = 0;
         ((check_depth > 0) && (res <= MATE_CUTOFF) && (time_is_up == TM_NO_TIMEOUT));
         check_depth -= 2)
    {
        Search_Reset_History();
        Hash_Clear_Tables();
        res = Search_Negamate(max_d, Alpha, Beta, colour, check_depth, pline,
                              blocked_movelist, blocked_moves, 1 /*root node*/, in_check);
    }

    Search_Reset_History();
    Hash_Clear_Tables();

    if (res > MATE_CUTOFF)
        return(COMP_MOVE_FOUND);
    else
        return(COMP_NO_MOVE);
}

/*you may wonder why 'black_started_game' isn't somewhat redundant to 'colour'.
usually, it is "white to move and mate", or sometimes also "black to move and
mate".
OK, but what if the user has a puzzle with "white to move", but instead enters
a position with black to move, then makes a black move that leads to the actual
puzzle and then starts the mating search?
In this case, the game screen display afterwards still has to be correct, and
it has to start with the black move.
Remember also that the mate search may not just be called right after entering
a position, it may also be called from within an actual game position, in which
case the game notation has to display the full game.*/
void NEVER_INLINE Search_Mate_Solver(int black_started_game, enum E_COLOUR colour)
{
    MOVE blockmovelst[MAXMV];
    int mate_depth, mate_plies, is_material_enough,
    mate_found = 1, blocked_moves = 0;
    int32_t dummy_conf_time;
    LINE pline;
    char mate_found_str[12];

    time_is_up = TM_NO_TIMEOUT;

    Util_Memzero(blockmovelst, sizeof(blockmovelst));

    /*should actually already have been caught via play.c / Play_Handling()*/
    is_material_enough = Eval_Setup_Initial_Material();
    if (!is_material_enough) /*draw due to insufficient material*/
    {
        (void) Hmi_Conf_Dialogue("no mate found:", "material draw.", &dummy_conf_time,
                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_RESTORE);
        return;
    }

    Util_Strcpy(mate_found_str, "mate in X.");

    mate_depth = (int) ((CFG_GET_OPT(CFG_MTI_MODE)) >> CFG_MTI_OFFSET);
    mate_depth++; /*config 0 is mate-in-1*/
    mate_plies = mate_depth * 2;
    mate_plies--; /*now we got the allowed mating distance in plies*/

    if ((mate_plies > MAX_DEPTH-2) || (mate_plies > 17))
    /*should not happen, maximum is 8 moves, but for future changes*/
    {
        (void) Hmi_Conf_Dialogue("solver error:", "mate too deep.", &dummy_conf_time,
                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
        return;
    }

    do /*look for mate solutions*/
    {
        enum E_COMP_RESULT mate_result;
        enum E_TIMEOUT time_status;

        /*ramp up the speed*/
        Hw_Set_Speed(SYSTEM_SPEED_HIGH, SYSTEM_MODE_COMP, CLK_ALLOW_LOW);
        if (CFG_GET_OPT(CFG_CLOCK_MODE) > CFG_CLOCK_100)
        {
            /*we are overclocked - check the firmware image. The point here is not
            the image, but the calculation. the system speed is already high here.*/
            unsigned int sys_test_result;
            sys_test_result = Hw_Check_FW_Image(); /*do some lengthy calculation stuff*/
            if (sys_test_result != HW_SYSTEM_OK)   /*ooops - overclocked operation is instable!*/
            {
                Hw_Set_Speed(SYSTEM_SPEED_LOW, SYSTEM_MODE_USER, CLK_FORCE_AUTO);  /*save energy and get the keyboard going*/
                (void) Hmi_Conf_Dialogue("O/C failed,", "normal speed.", &dummy_conf_time,
                                         HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                CFG_SET_OPT(CFG_CLOCK_MODE, CFG_CLOCK_100);        /*disable overclocking*/
                Hw_Set_Speed(SYSTEM_SPEED_HIGH, SYSTEM_MODE_COMP, CLK_ALLOW_LOW); /*ramp up for the computer move*/
            }
        }

        Util_Memzero(&pline, sizeof(LINE));

        Time_Set_Current(); /*for the display*/
        (void) Time_Init(mv_stack_p);

        Hmi_Build_Mating_Screen(mate_depth);

        mate_result = Search_Get_Mate_Solution(mate_plies, blockmovelst, blocked_moves, &pline, colour);
        time_status = time_is_up;

        Time_Set_Stop();
        /*ramp down the speed*/
        Hw_Set_Speed(SYSTEM_SPEED_LOW, SYSTEM_MODE_USER, CLK_FORCE_AUTO);

        /*double-beep only if we have actually found a solving move.
          means also: don't double-beep if there is a forced mate
          for the opponent instead.*/
        if (mate_result == COMP_MOVE_FOUND)
            Hmi_Signal(HMI_MSG_MOVE);
        else if (time_status != TM_USER_CANCEL) /*user triggered abort does not need attention*/
            Hmi_Signal(HMI_MSG_ATT);

        Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_MOVE, HW_MSG_PARAM_BACK_CONF);

        if (mate_result == COMP_MOVE_FOUND)
        {
            enum E_HMI_USER user_answer;
            int mv_cnt, display_depth;

            display_depth = (pline.line_len + 1) / 2;
            mate_found_str[8] = ((char) display_depth) + '0';
            (void) Hmi_Conf_Dialogue("mate found:", mate_found_str, &dummy_conf_time,
                                     HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);

            /*put the move sequence to the notation display, including "pretty print".*/
            for (mv_cnt = 0; mv_cnt < pline.line_len; mv_cnt++)
            {
                MOVE decomp_move = Mvgen_Decompress_Move(pline.line_cmoves[mv_cnt]);
                Search_Push_Status();
                Search_Make_Move(decomp_move);
                Hmi_Prepare_Pretty_Print(black_started_game);
            }

            /*open the notation screen.*/
            Hmi_Disp_Movelist(black_started_game, HMI_MENU_MODE_POS);

            /*take back all the moves for searching alternative solutions,
              i.e. revert to the original position.*/
            for (mv_cnt = 0; mv_cnt < pline.line_len; mv_cnt++)
            {
                Search_Retract_Last_Move();
                Search_Pop_Status();
            }

            user_answer = Hmi_Conf_Dialogue("search for", "other solutions?", &dummy_conf_time,
                                            HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
            if (user_answer == HMI_USER_CANCEL)
                return;
            /*add the current solution move to the blocklist*/
            blockmovelst[blocked_moves] = Mvgen_Decompress_Move(pline.line_cmoves[0]);
            blocked_moves++;
        } else
        {
            switch (time_status)
            {
            case TM_TIMEOUT:
                (void) Hmi_Conf_Dialogue("no mate found:", "time over.", &dummy_conf_time,
                                         HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                break;
            case TM_USER_CANCEL:
                (void) Hmi_Conf_Dialogue("no mate found:", "cancelled.", &dummy_conf_time,
                                         HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                break;
            case TM_NO_TIMEOUT:
            default:
                    if ((mate_result == COMP_MATE) && (blocked_moves == 0))
                    /*the entered position is already checkmate. that should be caught
                      after entering the position, but keep it here just in case
                      that the interface should change later on.*/
                    {
                        if (colour == WHITE)
                            (void) Hmi_Conf_Dialogue("white is mated:", "search finished.", &dummy_conf_time,
                                                     HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                        else
                            (void) Hmi_Conf_Dialogue("black is mated:", "search finished.", &dummy_conf_time,
                                                     HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                    } else
                        (void) Hmi_Conf_Dialogue("no mate found:", "search finished.", &dummy_conf_time,
                                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                break;
            }

            /*no (or no further) mate found, so we are done.*/
            mate_found = 0;
        }
    } while (mate_found);
}
