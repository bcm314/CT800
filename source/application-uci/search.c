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

#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ctdefs.h"
#include "move_gen.h"
#include "hashtables.h"
#include "eval.h"
#include "book.h"
#include "util.h"
#include "search.h"

extern char *Play_Translate_Moves(MOVE m);
extern int64_t Play_Get_Millisecs(void);
extern int64_t Play_Get_Own_Millisecs(void);
extern void Play_Print(const char *str);
extern int Play_Get_Abort(void);
extern void Play_Wait_For_Abort_Event(int32_t millisecs);

/*---------- external variables ----------*/
/*-- READ-ONLY  --*/
extern int game_started_from_0;
extern int32_t start_moves;

/*UCI options*/
extern unsigned int disable_book;
extern enum E_CURRMOVE show_currmove;
extern int contempt_val;
extern int contempt_end;
extern volatile unsigned int uci_debug;
extern int32_t eval_noise;

extern PIECE empty_p;
extern int fifty_moves;
/*these get set up in Eval_Setup_Initial_Material(), see eval.c for explanation*/
extern int start_pawns;
extern int dynamic_resign_threshold;
extern enum E_COLOUR computer_side;
extern uint64_t g_max_nodes;
extern PIECE Wpieces[16];
extern PIECE Bpieces[16];

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
extern uint64_t g_nodes;
extern uint64_t tb_hits;

extern TT_ST     *T_T;
extern TT_ST *Opp_T_T;
extern unsigned int hash_clear_counter;


/*---------- module global variables ----------*/

static enum E_TIMEOUT time_is_up;

/*for every root move, the expected reply from the opponent is cached in this
  array during the iterative deepening to help with the move sorting. for the
  UCI version, this is not that important because the hashtables are big enough,
  but for the embedded version with small hash tables, this is quite helpful.
  however, it is not damaging for the UCI version, either.*/
static CMOVE opp_move_cache[MAXMV];

/*use the Ciura sequence for the shell sort. more than 57 is not needed because
the rare maximum of pseudo-legal moves in real game positions is about 80 to 90.*/
static const int shell_sort_gaps[] = {1, 4, 10, 23, 57 /*, 132, 301, 701*/};

static const int PieceValFromType[PIECEMAX]= {0, 0, PAWN_V, KNIGHT_V, BISHOP_V, ROOK_V, QUEEN_V, INFINITY_, 0, 0,
                                              0, 0, PAWN_V, KNIGHT_V, BISHOP_V, ROOK_V, QUEEN_V, INFINITY_
                                             };

/*this is needed for deepening the PV against delay exchanges, which can cause
a horizon effect. only equal exchanges can do so because unequal ones either
would be a clear win or loss anyway.
note that for this purpose, minor pieces are assumed to be equal.*/
static const int ExchangeValue[PIECEMAX]=    {0, 0, PAWN_V, KNIGHT_V, KNIGHT_V, ROOK_V, QUEEN_V, INFINITY_, 0, 0,
                                              0, 0, PAWN_V, KNIGHT_V, KNIGHT_V, ROOK_V, QUEEN_V, INFINITY_
                                             };

/*for mapping board squares to file masks*/
static const uint8_t board_file_mask[120] = {
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
static const MOVE mv_move_mask = {{0xFFU, 0xFFU, 0xFFU, 0}};

/* --------- FUTILITY PRUNING DEFINITIONS ---------- */
#define FUTIL_DEPTH     4
static const int FutilityMargins[FUTIL_DEPTH] = {0, 240, 450, 600};

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

/* ------------- TIME/UCI CONTROL ----------------*/

static char printbuf[512]; /*with the depth limitation, this is more than sufficient.*/

static int64_t start_time, start_time_nps, stop_time, output_time, throttle_time, sleep_time;
static uint64_t nps_1ms, last_nodes, last_throttle_nodes, nps_startup_phase, nodes_current_second;

static uint64_t effective_max_nps_rate; /*kicks in after pre-search*/
static int effective_cpu_speed;

static MOVE uci_curr_move;
static unsigned int uci_curr_move_number;

#ifdef DBGCUTOFF
static uint64_t cutoffs_on_1st_move, total_cutoffs;
#endif

/*---------- local functions ----------*/

static int64_t Time_Passed(void)
{
    return(Play_Get_Millisecs() - start_time);
}

static void Time_Calc_Throttle(int64_t current_time)
{
    int64_t remaining_time = stop_time - current_time;
    /*will the move time be over in the next second?*/
    if (remaining_time < 1000LL)
    {
        if (remaining_time > 0) /*should be the case*/
        {
            /*adjust the effective CPU speed*/
            if (effective_cpu_speed < 100)
            {
                /*reduce the CPU speed proportionally to the shorter time
                  frame that remains.*/
                int64_t tmp_speed = effective_cpu_speed;
                tmp_speed *= remaining_time;
                tmp_speed += 500LL; /*rounding*/
                tmp_speed /= 1000LL;
                effective_cpu_speed = (int) tmp_speed;
            }

            /*adjust the kNPS rate*/
            effective_max_nps_rate *= remaining_time;
            effective_max_nps_rate += 500ULL; /*rounding*/
            effective_max_nps_rate /= 1000ULL;
        } else
        {
            effective_cpu_speed = 0;
            effective_max_nps_rate = 0ULL;
        }
    }
}

/*prints general statistics once per second during calculation.*/
static void Time_Output(int64_t current_time, int64_t subtract_time)
{
    if (current_time >= output_time)
    {
        int64_t time_passed, time_passed_calib;
        uint64_t nps_stat;
        uint16_t hash_used;
        int len;

        time_passed = current_time - start_time;

        if (time_passed > 0)
            nps_stat = (g_nodes * 1000LL) / time_passed;
        else
            nps_stat = 0;

        time_passed_calib = current_time - start_time_nps - subtract_time;

        /*update the NPS rate for time checking continuously. It may well change
        over time, e.g. when the hash tables get full.*/
        if (time_passed_calib > 0)
            nps_1ms = g_nodes / time_passed_calib;

        hash_used = (uint16_t) Hash_Get_Usage();

        /*convert the data to the UCI info string.*/
        strcpy(printbuf, "info time ");
        len = 10;
        len += Util_Tostring_I64(printbuf + len, time_passed);
        strcpy(printbuf + len, " nodes ");
        len += 7;
        len += Util_Tostring_U64(printbuf + len, g_nodes);
        strcpy(printbuf + len, " nps ");
        len += 5;
        len += Util_Tostring_U64(printbuf + len, nps_stat);
        strcpy(printbuf + len, " hashfull ");
        len += 10;
        len += Util_Tostring_U16(printbuf + len, hash_used);
        strcpy(printbuf + len, " tbhits ");
        len += 8;
        len += Util_Tostring_U64(printbuf + len, tb_hits);

        if (show_currmove == CURR_UPDATE)
        {
            char *mv_str;

            strcpy(printbuf + len, " currmove ");
            len += 10;
            mv_str = Play_Translate_Moves(uci_curr_move);
            strcpy(printbuf + len, mv_str);
            len += (mv_str[4] == 0) ? 4 : 5; /*move with or without promotion*/
            strcpy(printbuf + len, " currmovenumber ");
            len += 16;
            len += Util_Tostring_U16(printbuf + len, (uint16_t)(uci_curr_move_number + 1U));
        }
        printbuf[len++] = '\n';
        printbuf[len] = '\0';

        Play_Print(printbuf);

        nodes_current_second = 0; /*for the next second*/

        /*reduce effective throttle speed if the move time will be over
          in the next second.*/
        Time_Calc_Throttle(output_time);

        output_time += 1000LL;
    }
}

/*checks whether the allocated move time or node count has been reached.
  if not, the software CPU throttling is done here by inserting waiting
  slices.*/
static enum E_TIMEOUT Time_Check_Throttle(void)
{
    int64_t current_time;
    enum E_THROTTLE throttle_mode;

    /*check whether we need to check the time. that would involve a system
      call and is quite expensive, so it's only done about every millisecond.
      since the node rate is known / calibrated, this boils down to checking
      the time every so many nodes.*/
    nodes_current_second += g_nodes - last_throttle_nodes;
    last_throttle_nodes = g_nodes;
    if ((g_nodes - last_nodes < nps_1ms) && (nodes_current_second < effective_max_nps_rate))
    {
        return(TM_NO_TIMEOUT);
    }

    current_time = Play_Get_Millisecs();

    /*move time over?*/
    if (current_time >= stop_time)
        return(TM_TIMEOUT);

    /*search with total nodes limit?*/
    if ((g_max_nodes) && (g_nodes + (nps_1ms * 5LL) / 4LL >= g_max_nodes))
        return(TM_NODES);

    /*UCI stop or quit command?*/
    if (Play_Get_Abort())
        return(TM_ABORT);

    /*during initial NPS calibration, calculate the NPS
    relative to the absolute nodes vs. absolute time.*/
    if (nps_startup_phase)
    {
        int64_t time_passed_calib = current_time - start_time_nps - sleep_time;
        if (time_passed_calib > 0)
        {
            nps_1ms = g_nodes / time_passed_calib;
            if (nps_1ms < 500LL) nps_1ms = 500LL; /*can happen with 10ms timer resolution*/
            /*10ms initial calibration time because the CPU throttling in percentage
              mode works with 10ms slices. continuous calibration happens once per second
              in Time_Output().*/
            if (time_passed_calib >= 10LL)
                nps_startup_phase = 0;
        }
    }

    last_nodes = g_nodes;

    /*does throttling kick in, and if so, which one?*/
    if (nodes_current_second >= effective_max_nps_rate)
        throttle_mode = THROTTLE_NPS_RATE;
    else if (current_time >= throttle_time)
        throttle_mode = THROTTLE_CPU_PERCENT;
    else
        throttle_mode = THROTTLE_NONE;

    /*the throtting divides a second into a 100 slices of 10 ms each.
      n% CPU speed means 100-x% throttling, so 100% CPU speed is 0%
      throttling, which is no throttling. 1% CPU speed (the minimum)
      is 99% throttling.

      in case of NPS related throttling, the idea is to use every second
      until the nodes allowed in one second are spent, and then wait for
      the rest of the second.*/
    if (throttle_mode != THROTTLE_NONE)
    {
        int64_t stop_sleep_time, stop_throttle_time, start_throttle_time;

        start_throttle_time = current_time;

        /*output_time gets updated in Time_Output() during the waiting loop.*/
        stop_throttle_time = output_time;

        if (throttle_mode == THROTTLE_CPU_PERCENT)
        {
            /*partial sleep to circumvent the CPU wakeup to full speed,
              with busy waiting for the last 50 ms. the percentage refers
              to the full CPU speed, after all.*/
            stop_sleep_time = stop_throttle_time - 50LL;
            if (uci_debug)
            {
                int len;
                strcpy(printbuf, "info string debug: CPU [%] throttling at: ");
                len = 42;
                len += Util_Tostring_U16(printbuf + len, (uint16_t) effective_cpu_speed);
                strcpy(printbuf + len, " %\n");
                Play_Print(printbuf);
            }
        } else
        {
            /*full sleep. it doesn't matter whether the first 10-20 ms after
              wakeup still run at reduced CPU frequency because in kNPS mode,
              the engine can just calculate a bit longer in the next second.*/
            stop_sleep_time = stop_throttle_time;
            if (uci_debug)
            {
                /*scale down the NPS to kNPS with rounding*/
                int len;
                uint32_t throttle_knps = (uint32_t) ((effective_max_nps_rate + 500ULL) / 1000ULL);
                strcpy(printbuf, "info string debug: CPU [kNPS] throttling at: ");
                len = 45;
                len += Util_Tostring_U32(printbuf + len, throttle_knps);
                strcpy(printbuf + len, " kNPS\n");
                Play_Print(printbuf);
            }
        }

        /*check whether the move time limits the sleeps.*/
        if (stop_sleep_time > stop_time)
            stop_sleep_time = stop_time;

        while (current_time < stop_throttle_time)
        {
            /*add 1 ms to account for time resolution*/
            if (current_time < stop_sleep_time)
                Play_Wait_For_Abort_Event((int32_t) (stop_sleep_time - current_time + 1));

            current_time = Play_Get_Millisecs();

            if (current_time >= stop_time) /*move time over*/
                return(TM_TIMEOUT);

            if (Play_Get_Abort()) /*UCI stop or quit command*/
                return(TM_ABORT);

            Time_Output(current_time, sleep_time + current_time - start_throttle_time);
        }

        /*if percentage throttling could be possible, reset the status*/
        if (current_time >= throttle_time - 2000LL)
        {
            int64_t max_throttle_time;

            /*baseline: the next N 10 ms frames for computing.*/
            throttle_time = current_time + effective_cpu_speed * 10;

            /*output_time already updated? should be, actually.*/
            if (current_time >= output_time)
                max_throttle_time = output_time + 1000LL; /*not yet*/
            else
                max_throttle_time = output_time; /*already updated*/

            /*don't leak into another 1 second frame.*/
            if (throttle_time > max_throttle_time)
                throttle_time = max_throttle_time;
        }

        sleep_time += current_time - start_throttle_time;
    }

    Time_Output(current_time, sleep_time);

    return(TM_NO_TIMEOUT);
}

/*used for timing until move time is over or the user stops the engine.
  necessary for fixed UCI move time or infinite when there is nothing
  to calculate because e.g. maximum depth has been reached or a mate is
  already found.*/
static void Time_Wait_For_Abort(void)
{
    uint16_t hash_used = Hash_Get_Usage();

    for (;;)
    {
        int64_t time_passed, current_time, wakeup_time;

        current_time = Play_Get_Millisecs();

        /*move time over?*/
        if (current_time >= stop_time)
            return;

        /*has someone set the abort flag?*/
        if (Play_Get_Abort()) return;

        time_passed = current_time - start_time;

        if (current_time >= output_time)
        {
            int len;

            strcpy(printbuf, "info time ");
            len = 10;
            len += Util_Tostring_I64(printbuf + len, time_passed);
            strcpy(printbuf + len, " nodes ");
            len += 7;
            len += Util_Tostring_U64(printbuf + len, g_nodes);
            strcpy(printbuf + len, " nps 0 hashfull ");
            len += 16;
            len += Util_Tostring_U16(printbuf + len, hash_used);
            strcpy(printbuf + len, " tbhits ");
            len += 8;
            len += Util_Tostring_U64(printbuf + len, tb_hits);
            printbuf[len++] = '\n';
            printbuf[len] = '\0';
            Play_Print(printbuf);

            output_time += 1000LL;
        }

        /*next scheduled wakeup is either when the next outut is due or when
          the move time is over. or an abort event can be triggered via UCI
          stop/quit commands.*/
        if (stop_time > output_time)
            wakeup_time = output_time;
        else
            wakeup_time = stop_time;

        if (current_time < wakeup_time)
            Play_Wait_For_Abort_Event((int32_t) (wakeup_time - current_time));
    }
}

/*that's a shell sort which
a) doesn't need recursion, unlike quicksort, and
b) is faster than quicksort on lists with less than 100 entries.*/
void Search_Do_Sort(MOVE *restrict movelist, int N)
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

static int Search_Print_PV_Line(const LINE *aPVp, char *outbuf)
{
    int i, len;
    for (i = 0, len = 0; i < aPVp->line_len; i++)
    {
        MOVE decomp_move;
        char *mv_str;
        /*string the PV together. not using strcat in a loop because that
          creates hidden n^2 algorithms, which wouldn't really matter with the
          typical search depth, but still.*/
        outbuf[len++] = ' ';
        decomp_move = Mvgen_Decompress_Move(aPVp->line_cmoves[i]);
        mv_str = Play_Translate_Moves(decomp_move);
        strcpy(outbuf + len, mv_str);
        /*length goes up by 5 with promotion move, else by 4.*/
        len += (mv_str[4] == 0) ? 4 : 5;
    }

    return(len);
}

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
static void Search_Swap_Best_To_Top(MOVE *movelist, int len)
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

/*returns whether a mate has been seen despite the noise setting.
  for each move of depth, the engine shall overlook the mate with a probability
  of "eval_noise".*/
static int Search_Mate_Noise(int depth)
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
    if ((rand() % 101U) > prob)
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
static int32_t Search_Flatten_Difference(int32_t eval)
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

static int Search_Quiescence(int alpha, int beta, enum E_COLOUR colour, int do_checks, int qs_depth)
{
    MOVE movelist[MAXCAPTMV];
    enum E_COLOUR next_colour;
    int e, score, i, move_cnt, actual_moves, t, recapt;
    int is_material_enough, n_checks, n_check_pieces;
    unsigned has_move;

    g_nodes++;
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
        if (UNLIKELY (mv_stack_p - Starting_Mv >= MAX_DEPTH+MAX_QIESC_DEPTH-1) )
            return e;

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
                move_cnt = has_move = 0;
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
        if (UNLIKELY (mv_stack_p - Starting_Mv >= MAX_DEPTH+MAX_QIESC_DEPTH-1) )
            return e;

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

static int Search_Negamate(int depth, int alpha, int beta, enum E_COLOUR colour,
                           MOVE *restrict movelist, int move_cnt,
                           int check_depth, LINE *restrict pline, int in_check)
{
    MOVE xmvlist[MAXMV];
    MOVE dummy;
    LINE line;
    enum E_COLOUR next_colour;
    int i, a, checking, tt_value, actual_move_cnt, root_node;

    g_nodes++;
    pline->line_len = 0;

    /*prevent stack overflow.*/
    if (UNLIKELY(mv_stack_p - Starting_Mv >= MAX_DEPTH-1 ))
    /* We are too deep */
        return(0);

    if (Hash_Check_For_Draw())
        return(0);
    if (colour == WHITE) /*slightly different way to use the hash tables*/
    {
        if (Hash_Check_TT(    T_T, colour, alpha, beta, depth, move_stack[mv_stack_p].mv_pos_hash, &tt_value, &dummy))
            return(tt_value);
    } else /*black*/
    {
        if (Hash_Check_TT(Opp_T_T, colour, alpha, beta, depth, move_stack[mv_stack_p].mv_pos_hash, &tt_value, &dummy))
            return(tt_value);
    }

    /*first phase: get the moves, filter out the legal ones,
      prioritise check delivering moves, and if it is checkmate, return.*/

    if (move_cnt == 0) /*deferred move generation*/
    {
        root_node = 0;
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
    } else
        root_node = 1;

    next_colour = Mvgen_Opp_Colour(colour);

    for (i = 0, actual_move_cnt = 0, checking = 0; i < move_cnt; i++)
    {
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

    if (time_is_up == TM_NO_TIMEOUT)
        time_is_up = Time_Check_Throttle();

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

        if (root_node)
        {
            uci_curr_move.u = movelist[i].u;
            uci_curr_move_number = i;
            if ((show_currmove == CURR_ALWAYS) && (time_is_up == TM_NO_TIMEOUT))
            {
                /*output_time gets initialised to start_time + 1000LL. if it is
                  greater, then the first output_time must be through, which means
                  that output_time out at least start_time + 2000LL, so at least
                  1 second is passed: start the output after 1 second as per the
                  UCI spec.
                  this still gets around always calling the time functions, which are
                  expensive.*/
                if (output_time >= start_time + 1500LL)
                {
                    int len;
                    char *mv_str;
                    strcpy(printbuf, "info currmove ");
                    mv_str = Play_Translate_Moves(movelist[i]);
                    strcpy(printbuf + 14, mv_str);
                    len = (mv_str[4] == 0) ? (14 + 4) : (14 + 5);
                    strcpy(printbuf + len, " currmovenumber ");
                    len += 16;
                    len += Util_Tostring_U16(printbuf + len, (uint16_t)(i + 1));
                    printbuf[len++] = '\n';
                    printbuf[len] = '\0';
                    Play_Print(printbuf);
                }
            }
        }

        giving_check = (movelist[i].m.mvv_lva == MVV_LVA_CHECK) ? 1 : 0;
        score = -Search_Negamate(depth-1, -beta, -a, next_colour, xmvlist, 0, check_depth, &line, giving_check);
        Search_Retract_Last_Move();
        Search_Pop_Status();

        if (score > a)
        {
            a = score;
            /*update the PV*/
            pline->line_cmoves[0] = Mvgen_Compress_Move(movelist[i]);
            memcpy(pline->line_cmoves + 1, line.line_cmoves, sizeof(CMOVE) * line.line_len);
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
static void Search_Adjust_Priorities(MOVE * restrict movelist, int len,
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

static int Search_Endgame_Reduct(void)
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

static int Search_Negascout(int CanNull, int level, LINE *restrict pline, MOVE *restrict mlst,
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

        hash_best.u = MV_NO_MOVE_MASK;

        g_nodes++;

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
            Hash_Update_TT(    T_T, depth, 0, EXACT, move_stack[mv_stack_p].mv_pos_hash, smove);
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

        if (time_is_up == TM_NO_TIMEOUT)
            time_is_up = Time_Check_Throttle();

        for (i = 0; i < n; i++)
        {
            /*foreach child of node*/
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
                uci_curr_move.u = mlst[i].u;
                uci_curr_move_number = i;
                root_move_index = i;
                if ((show_currmove == CURR_ALWAYS) && (time_is_up == TM_NO_TIMEOUT))
                {
                  /*output_time gets initialised to start_time + 1000LL. if it is
                    greater, then the first output_time must be through, which means
                    that output_time is now at least start_time + 2000LL, so at least
                    1 second is passed: start the output after 1 second as per the
                    UCI spec.
                    this still gets around always calling the time functions, which are
                    expensive.*/
                    if (output_time >= start_time + 1500LL)
                    {
                        int len;
                        char *mv_str;
                        strcpy(printbuf, "info currmove ");
                        mv_str = Play_Translate_Moves(mlst[i]);
                        strcpy(printbuf + 14, mv_str);
                        len = (mv_str[4] == 0) ? (14 + 4) : (14 + 5);
                        strcpy(printbuf + len, " currmovenumber ");
                        len += 16;
                        len += Util_Tostring_U16(printbuf + len, (uint16_t)(i + 1));
                        printbuf[len++] = '\n';
                        printbuf[len] = '\0';
                        Play_Print(printbuf);
                    }
                }
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
                if ((mv_stack_p + start_moves < contempt_end) && (game_started_from_0))
                {
                    if (colour == computer_side)
                        t = contempt_val;
                    else
                        t = -contempt_val;
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
                    memcpy(pline->line_cmoves + 1, line.line_cmoves, sizeof(CMOVE) * line.line_len);
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
                        Hash_Update_TT(    T_T, depth, a, CHECK_BETA, move_stack[mv_stack_p].mv_pos_hash, mlst[i]);
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
                Hash_Update_TT(    T_T, depth, a, EXACT, move_stack[mv_stack_p].mv_pos_hash, hash_best);
            else
                Hash_Update_TT(Opp_T_T, depth, a, EXACT, move_stack[mv_stack_p].mv_pos_hash, hash_best);
        } else
        {
            hash_best.u = MV_NO_MOVE_MASK;
            if (level & 1)
                Hash_Update_TT(    T_T, depth, a, CHECK_ALPHA, move_stack[mv_stack_p].mv_pos_hash, hash_best);
            else
                Hash_Update_TT(Opp_T_T, depth, a, CHECK_ALPHA, move_stack[mv_stack_p].mv_pos_hash, hash_best);
        }
        return a;
    }
}

/*perform a shallow search of 1 ply + QS at root to sort the root move list.*/
static int Search_Play_And_Sort_Moves(MOVE *restrict movelist, int len,
                                      enum E_COLOUR Nextcolour, int *restrict score_drop)
{
    int sortV[MAXMV], i;

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
                if ((mv_stack_p + start_moves < contempt_end) && (game_started_from_0))
                    current_score = contempt_val;
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

static void Search_Print_Move_Output(int depth, int score, int64_t time_passed, int hash_report)
{
    uint64_t nps;
    int len;

    if (time_passed > 0)
        nps = (g_nodes * 1000ULL) / time_passed;
    else
        nps = 0;

    /*avoid sprintf for speed reasons in extreme bullet games where fast
      I/O can become important since the search depth is low and the output
      frequent in relation to the thinking time.
      using strcpy with the right length is also a bit faster than repeated
      strcat.*/
    strcpy(printbuf, "info depth ");
    len = 11;
    len += Util_Tostring_U16(printbuf + len, (uint16_t) depth);
    strcpy(printbuf + len, " seldepth ");
    len += 10;

    if (GlobalPV.line_len > depth)
        len += Util_Tostring_U16(printbuf + len, (uint16_t) GlobalPV.line_len);
    else
        len += Util_Tostring_U16(printbuf + len, (uint16_t) depth);

    if (score > MATE_CUTOFF)
    {
        int mate_moves = INFINITY_ - score;
        mate_moves++;
        mate_moves /= 2;
        strcpy(printbuf + len, " score mate ");
        len += 12;
        len += Util_Tostring_U16(printbuf + len, (uint16_t) mate_moves);
    } else if (score < -MATE_CUTOFF)
    {
        int mate_moves = INFINITY_ + score;
        mate_moves++;
        mate_moves /= 2;
        strcpy(printbuf + len, " score mate -");
        len += 13;
        len += Util_Tostring_U16(printbuf + len, (uint16_t) mate_moves);
    } else
    {
        strcpy(printbuf + len, " score cp ");
        len += 10;
        len += Util_Tostring_I16(printbuf + len, (int16_t) score);
    }

    strcpy(printbuf + len, " time ");
    len += 6;
    len += Util_Tostring_I64(printbuf + len, time_passed);

    strcpy(printbuf + len, " nodes ");
    len += 7;
    len += Util_Tostring_U64(printbuf + len, g_nodes);

    strcpy(printbuf + len, " nps ");
    len += 5;
    len += Util_Tostring_U64(printbuf + len, nps);

    if (hash_report)
    {
        uint16_t hash_used = Hash_Get_Usage();

        strcpy(printbuf + len, " hashfull ");
        len += 10;
        len += Util_Tostring_U16(printbuf + len, hash_used);
    }

    strcpy(printbuf + len, " tbhits ");
    len += 8;
    len += Util_Tostring_U64(printbuf + len, tb_hits);

    if (GlobalPV.line_len > 0)
    {
        strcpy(printbuf + len, " pv");
        len += 3;
        len += Search_Print_PV_Line(&GlobalPV, printbuf + len);
    }

    printbuf[len++] = '\n';
    printbuf[len] = '\0';

    Play_Print(printbuf); /*print everything at once so that the other thread won't interfere*/
}

/*tests whether the position is checkmate for the relevant colour.*/
static int Search_Is_Checkmate(enum E_COLOUR colour)
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
                g_nodes++;
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
                g_nodes++;
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
    memset(W_history, 0, sizeof(W_history));
    memset(B_history, 0, sizeof(B_history));
    memset(W_Killers, 0, sizeof(W_Killers));
    memset(B_Killers, 0, sizeof(B_Killers));
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
                }
            } else if (xy1==E8) { /* black castle */
                if (xy2==G8) {
                    board[H8] = board[F8];
                    board[H8]->xy = H8;
                    board[F8] = &empty_p;
                } else if (xy2==C8) {
                    board[A8] = board[D8];
                    board[A8]->xy = A8;
                    board[D8] = &empty_p;
                }
            }
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

/*the mating solver.*/
static enum E_COMP_RESULT
Search_Get_Mate_Solution(int mate_depth_mv, MOVE *restrict movelist, int move_cnt,
                         LINE *restrict pline, enum E_COLOUR colour, int in_check)
{
    int res, max_d;
    int Alpha = 0;
    int Beta  = INFINITY_;
    int check_depth;

    pline->line_len = 0;
    Starting_Mv = mv_stack_p;
    max_d = (mate_depth_mv * 2) - 1;

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
         ((check_depth > 0) && (res <= MATE_CUTOFF) && (time_is_up == TM_NO_TIMEOUT)); check_depth -= 2)
    {
        Search_Reset_History();
        Hash_Clear_Tables();
        res = Search_Negamate(max_d, Alpha, Beta, colour, movelist, move_cnt, check_depth, pline, in_check);
    }

    Search_Reset_History();
    Hash_Clear_Tables();

    if ((res > MATE_CUTOFF) && (time_is_up == TM_NO_TIMEOUT))
        return (COMP_MOVE_FOUND);
    else
        return(COMP_NO_MOVE);
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
static void Search_Sort_50_Moves(MOVE *restrict player_move, MOVE *restrict movelist,
                                 int move_cnt, enum E_COLOUR colour)
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

static int Search_Get_Root_Move_List(MOVE *restrict movelist, int *restrict move_cnt, enum E_COLOUR colour)
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

enum E_COMP_RESULT
Search_Get_Best_Move(MOVE *restrict answer_move, MOVE player_move, int64_t full_move_time,
                     int move_overhead, int exact_time, int max_depth, int cpu_speed,
                     uint64_t max_nps_rate, enum E_COLOUR colour, const MOVE *restrict given_moves,
                     int given_moves_len, int mate_mode, int mate_depth_mv,
                     uint64_t *restrict spent_nodes, int64_t *restrict spent_time)
{
    uint64_t printed_nodes;
    int64_t time_passed=0LL;
    int ret_mv_idx=0, move_cnt, is_material_enough, in_check, i, easy_depth,
        mate_in_1, min_thinking_time, is_normal_time, is_analysis;
    MOVE movelist[MAXMV];
    LINE line;

    start_time = Play_Get_Millisecs();
    start_time_nps = start_time; /*can be overwritten later after hash aging*/
    sleep_time = 0;

    /*get the time when to start the first throttle sleep phase in CPU
      percentage mode, but only after the pre-search*/
    throttle_time = start_time + INFINITE_TIME;

    /*limiter only after pre-search*/
    effective_max_nps_rate = MAX_THROTTLE_KNPS * 1000ULL;
    effective_cpu_speed = 100;

    /*also throttle the easy move depth threshold*/
    easy_depth = EASY_DEPTH;

    if ((cpu_speed <= 5) || (max_nps_rate <= 50000ULL))
        easy_depth = EASY_DEPTH - 2;
    else if ((cpu_speed <= 20) || (max_nps_rate <= 200000ULL))
        easy_depth = EASY_DEPTH - 1;

    output_time = start_time + 1000LL; /*output of hash filling etc. every second*/

    /*for extreme bullet games (1 second per game):
      calculate minimum thinking time.
      move:
       1  - 9: 10 ms
      10 - 35: 20 ms (most difficult game phase)
      36 - 40: 15 ms
      41 - 60: 10 ms
      61 - 80:  7 ms
      > 80   :  5 ms
      in-check moves get half of the that time.*/
    if (game_started_from_0)
    {
        int ply_number = (start_moves + mv_stack_p) / 2 + 1;
        if (ply_number < 10)
            min_thinking_time = 10;
        else if (ply_number <= 35)
            min_thinking_time = 20;
        else if (ply_number <= 40)
            min_thinking_time = 15;
        else if (ply_number <= 60)
            min_thinking_time = 10;
        else if (ply_number <= 80)
            min_thinking_time = 7;
        else
            min_thinking_time = 5;
    } else
    {
        /*no game information available, use 10 ms fixed.*/
        min_thinking_time = 10;
    }

    stop_time = start_time + full_move_time - move_overhead; /*account for GUI delays*/

    if (stop_time < start_time + min_thinking_time)
    {
        if (!exact_time)
            stop_time = start_time + min_thinking_time;
        /*with bullet time, drop the hash full report because that involves
          a scan over the first 32k of each main hash table.*/
        is_normal_time = 0;
        /*if unthrottled: reduce the "easy move" depth to something useful*/
        if (easy_depth == EASY_DEPTH)
            easy_depth = EASY_DEPTH - 2;
    } else
        is_normal_time = 1;

    time_is_up = TM_NO_TIMEOUT;

    uci_curr_move.u = MV_NO_MOVE_MASK;
    uci_curr_move_number = 0;
    g_nodes = 1; /*this node*/
    nodes_current_second = 1;
    printed_nodes = 0;
    tb_hits = 0;
    last_nodes = 0;
    last_throttle_nodes = 0;
    nps_1ms = 500LL;
    nps_startup_phase = 1;
    *spent_nodes = 1ULL;
    *spent_time = 0LL;
#ifdef DBGCUTOFF
    cutoffs_on_1st_move = total_cutoffs = 0ULL;
#endif

    answer_move->u = MV_NO_MOVE_MASK;
    mate_in_1 = 0;
    is_analysis = ((exact_time) && (full_move_time == INFINITE_TIME));

    Search_Reset_History();

    is_material_enough = Eval_Setup_Initial_Material();
    if ((!is_material_enough) && (uci_debug))
         Play_Print("info string debug: insufficient material draw.\n");

    Starting_Mv = mv_stack_p;
    in_check = Search_Get_Root_Move_List(movelist, &move_cnt, colour);

    if (move_cnt == 0) /*the GUI should have filtered this*/
    {
        if (in_check)
            return (COMP_MATE);
        else
            return(COMP_STALE);
    }

    if (given_moves_len)
    {
        int actual_move_cnt;

        for (i = 0, actual_move_cnt = 0;
             ((i < given_moves_len) && (actual_move_cnt < move_cnt)); i++)
        {
            Search_Find_Put_To_Top(movelist + actual_move_cnt, move_cnt - actual_move_cnt, given_moves[i]);
            /*if the given move has been legal, it must be found at the top now.
              filters also away double moves.*/
            if (((movelist[actual_move_cnt].u ^ given_moves[i].u) & mv_move_mask.u) == 0)
            {
                actual_move_cnt++;
                /*as soon as actual_move_cnt reaches move_cnt, the loop has to end
                  because all legal moves have already been selected.*/
            }
        }
        if (actual_move_cnt == 0) /*no legal moves given*/
            return(COMP_NO_MOVE);
        else
            move_cnt = actual_move_cnt;

        /*resort per MVV/LVA*/
        Search_Do_Sort(movelist, move_cnt);
    }

    /*immediate mate found and not sorted out?*/
    if (movelist[0].m.mvv_lva == MVV_LVA_MATE_1)
        mate_in_1 = 1;

    /*dedicated mate searcher mode*/
    if (mate_mode)
    {
        enum E_COMP_RESULT ret_mv_status;

        memset(&line, 0, sizeof(LINE));

        /*now activate potential throttling since no pre-search is done.*/

        effective_max_nps_rate = max_nps_rate;
        effective_cpu_speed = cpu_speed;
        /*reduce effective throttle speed if the move time will be over
          in the next second.*/
        Time_Calc_Throttle(start_time);
        /*set percentage throttling.*/
        if (effective_cpu_speed < 100)
            throttle_time = start_time + effective_cpu_speed * 10;

        if (!mate_in_1)
            ret_mv_status = Search_Get_Mate_Solution(mate_depth_mv, movelist, move_cnt, &line, colour, in_check);
        else
        {
            ret_mv_status = COMP_MOVE_FOUND;
            line.line_cmoves[0] = Mvgen_Compress_Move(movelist[0]);
            line.line_len = 1;
        }
        time_passed = Time_Passed();
        if (ret_mv_status == COMP_MOVE_FOUND)
        {
            memcpy(&GlobalPV, &line, sizeof(LINE));
            Search_Print_Move_Output(line.line_len, INFINITY_ - line.line_len, time_passed, is_normal_time);
            *answer_move = Mvgen_Decompress_Move(line.line_cmoves[0]);
            /*for mate mode, only wait with "go infinite"*/
            if ((is_analysis) && (time_is_up != TM_ABORT))
            {
                Time_Wait_For_Abort();
                time_passed = Time_Passed();
                Search_Print_Move_Output(line.line_len, INFINITY_ - line.line_len, time_passed, is_normal_time);
            }
        } else
        {
            GlobalPV.line_len = 0;
            Search_Print_Move_Output(mate_depth_mv * 2 - 1, 0, time_passed, is_normal_time);
            answer_move->u = MV_NO_MOVE_MASK;
            /*for mate mode, only wait with "go infinite"*/
            if ((is_analysis) && (time_is_up != TM_ABORT))
            {
                Time_Wait_For_Abort();
                time_passed = Time_Passed();
                Search_Print_Move_Output(mate_depth_mv * 2 - 1, 0, time_passed, is_normal_time);
            }
        }
        *spent_nodes = g_nodes;
        *spent_time = time_passed;
        return(ret_mv_status);
    } /*end mate searcher mode*/

    if ((disable_book == 0) && (full_move_time < INFINITE_TIME) &&
        (given_moves_len == 0) && (Book_Is_Line(&ret_mv_idx,movelist,move_cnt)))
    {
        game_info.valid = EVAL_BOOK;
        answer_move->u = movelist[ret_mv_idx].u;
        GlobalPV.line_len = 1;
        GlobalPV.line_cmoves[0] = Mvgen_Compress_Move(movelist[ret_mv_idx]);
        time_passed = Time_Passed();
        Search_Print_Move_Output(1, 1, time_passed, is_normal_time);
        *spent_time = time_passed;
        *spent_nodes = g_nodes;
        return(COMP_MOVE_FOUND);
    } else {
        static int64_t hash_clear_time = 0LL;
        int64_t reduced_move_time;
        MOVE no_threat_move, decomp_move;
        int d, sort_max, pv_hit = 0, score_drop, pos_score, nscore;
        CMOVE failsafe_cmove;

        no_threat_move.u = MV_NO_MOVE_MASK;

        /*if 50 moves draw is close, re-sort the list*/
        Search_Sort_50_Moves(&player_move, movelist, move_cnt, colour);

        /*drop age clearing under extreme time pressure*/
        if ((full_move_time >= move_overhead * 10LL) && (full_move_time >= hash_clear_time * 10LL))
        {
            int64_t start_clear_time = Play_Get_Millisecs();
            Hash_Cut_Tables(hash_clear_counter);
            hash_clear_time = Play_Get_Millisecs() - start_clear_time;
        }

        /*clearing the hash tables takes time that should not be used for
          NPS calibration.*/
        start_time_nps = Play_Get_Millisecs();

        /*if too much time has been used, don't start another iteration as it will not finish anyway.*/
        if (exact_time == 0)
        {
            int half_min_time = (min_thinking_time + 1)/2;
            if (in_check) /*there are not many moves anyway*/
            {
                full_move_time /= 2;
                stop_time = start_time + full_move_time - move_overhead;
                if (stop_time < start_time + half_min_time)
                    stop_time = start_time + half_min_time;
            }
            reduced_move_time = ((stop_time - start_time) * 55LL + 50LL) / 100LL;
        } else
            reduced_move_time = full_move_time;

        if (mate_in_1)
        {
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
        } else
        {
            MOVE hash_best;
            int dummy;

            hash_best.u = MV_NO_MOVE_MASK;

            /*check with alpha=+INF and beta=-INF so that only the depth check
              is actually performed.*/
            if (!Hash_Check_TT(T_T, colour, INFINITY_, -INFINITY_, PRE_DEPTH, move_stack[mv_stack_p].mv_pos_hash, &dummy, &hash_best))
                 hash_best.u = MV_NO_MOVE_MASK;

            if (uci_debug)
            {
                if (hash_best.u != MV_NO_MOVE_MASK)
                {
                    char *mv_str;
                    int len;
                    strcpy(printbuf, "info string debug: root hash hit ");
                    len = 33;
                    mv_str = Play_Translate_Moves(hash_best);
                    strcpy(printbuf + len, mv_str);
                    len += (mv_str[4] == 0) ? 4 : 5; /*move with or without promotion*/
                    strcpy(printbuf + len, ".\n");
                    Play_Print(printbuf);
                }
            }

            if ((player_move.u != MV_NO_MOVE_MASK) && (GlobalPV.line_len >= 3) &&
                (GlobalPV.line_cmoves[1] == Mvgen_Compress_Move(player_move)))
            {
                /*shift down the global PV*/
                for (i = 0; i < GlobalPV.line_len - 2; i++)
                    GlobalPV.line_cmoves[i] = GlobalPV.line_cmoves[i+2];
                GlobalPV.line_len -= 2;
                if (GlobalPV.line_len > PRE_DEPTH) /*else the pre-search is better*/
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
                if ((move_cnt < 2) && (exact_time == 0))
                {
                    if (uci_debug) Play_Print("info string debug: PV hit and forced move.\n");
                    time_passed = Time_Passed();
                    Search_Print_Move_Output(game_info.depth, game_info.eval, time_passed, is_normal_time);
                    *answer_move = Mvgen_Decompress_Move(GlobalPV.line_cmoves[0]);
                    *spent_time = time_passed;
                    *spent_nodes = g_nodes;
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


        /* If opponent answered following previous PV, add the computer reply choice first in the moves list*/
        if (pv_hit)
        {
            decomp_move = Mvgen_Decompress_Move(GlobalPV.line_cmoves[0]);

            if (uci_debug)
            {
                char *mv_str;
                int len;
                strcpy(printbuf, "info string debug: PV hit ");
                len = 26;

                mv_str = Play_Translate_Moves(decomp_move);
                strcpy(printbuf + len, mv_str);
                len += (mv_str[4] == 0) ? 4 : 5; /*move with or without promotion*/
                strcpy(printbuf + len, ".\n");
                Play_Print(printbuf);
            }
            Search_Find_Put_To_Top(movelist, move_cnt, decomp_move);
        }

        /*look whether the easy move detection is
          a) a PV hit
          OR
          b) mate in 1
          OR
          c) bad enough to be realistic: yes, the opponent might just hang a piece. But it could also be a trap.
          AND
          d) good enough: maybe in the deep search, we know we have a mate, then don't be content with a piece.*/
        if ((!(
            ((pv_hit) && (failsafe_cmove == GlobalPV.line_cmoves[0])) ||
            (mate_in_1) ||
            ((game_info.last_valid_eval != NO_RESIGN) &&
                ((sort_max - game_info.last_valid_eval) < EASY_MARGIN_UP) &&
                ((sort_max - game_info.last_valid_eval) > EASY_MARGIN_DOWN))
           )) ||
           (exact_time) || /*don't use time savings with exact time*/
           (given_moves_len)) /*that is used for analysis*/
        {
            score_drop = 0;
        }

        /*clear the level 2 move cache.*/
        memset(opp_move_cache, 0, sizeof(opp_move_cache));

        /*now activate throttling since the pre-search is over.*/
        effective_max_nps_rate = max_nps_rate;
        effective_cpu_speed = cpu_speed;
        /*reduce effective throttle speed if the move time will be over
          in the next second.*/
        Time_Calc_Throttle(start_time);
        /*set percentage throttling.*/
        if (effective_cpu_speed < 100)
            throttle_time = start_time + effective_cpu_speed * 10;

        nscore = pos_score;

        for (d=START_DEPTH; ((d < MAX_DEPTH) && (d <= max_depth) &&
                             ((!g_max_nodes) || (g_nodes < g_max_nodes))); d++)
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

                if (time_is_up != TM_NO_TIMEOUT)
                    break;

                if ((g_max_nodes) && (g_nodes >= g_max_nodes))
                    break;

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

            time_passed = Time_Passed();
            if (ret_mv_idx >= 0)
            {
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
                    memcpy(GlobalPV.line_cmoves + 1, line.line_cmoves, sizeof(CMOVE) * line.line_len);
                    GlobalPV.line_len = line.line_len + 1;
                    Search_Find_Put_To_Top_Root(movelist, opp_move_cache, move_cnt, decomp_move);
                }

                Search_Print_Move_Output(d, pos_score, time_passed, is_normal_time);
                printed_nodes = g_nodes; /*avoid double PV with fixed depth search.*/

                if ((((pos_score > MATE_CUTOFF) || (pos_score < -MATE_CUTOFF) || (move_cnt < 2)) && (exact_time == 0))
                    || (time_is_up != TM_NO_TIMEOUT))
                    break;
            }
            /*if the pre-sorting has shown an outstanding move, and this move is still at the head of the PV,
            then just do it. Saves time and prevents the opponent from having a certain ponder hit in case he
            is using pondering. Still invest up to 10% of the total time.
            if we have an easy move that also is a PV hit, then the PV is retained if it is longer than
            the "easy depth", which is the desired effect.*/
            if ((score_drop >= EASY_THRESHOLD) && (d >= easy_depth) &&
                (failsafe_cmove == GlobalPV.line_cmoves[0]))
            {
                if (uci_debug) Play_Print("info string debug: quick reply, easy move detected.\n");
                break;
            }

            if (time_passed > reduced_move_time) /*more than 55% already used except with exact time*/
            {
                time_is_up = TM_TIMEOUT;
                break;
            }
        } /*for (d=...) end of Iterative Deepening loop*/

        time_passed = Time_Passed();

        if (printed_nodes < g_nodes) /*avoid double PV with fixed depth search.*/
            Search_Print_Move_Output(game_info.depth, game_info.eval, time_passed, is_normal_time);

        if (pos_score < -dynamic_resign_threshold) /*does not happen in UCI mode*/
        {
            /*computer resigns, but still return the move found in case the player wants to play it out*/
            *answer_move = Mvgen_Decompress_Move(GlobalPV.line_cmoves[0]);
            if ((is_analysis) && (time_is_up != TM_ABORT))
            /*timeout is also caused by abort command*/
            {
                Time_Wait_For_Abort();
                time_passed = Time_Passed();
                Search_Print_Move_Output(game_info.depth, game_info.eval, time_passed, is_normal_time);
            }
            return(COMP_RESIGN);
        }
    }
#ifdef DBGCUTOFF
    printf("info string cutoff %u\n", (unsigned)((cutoffs_on_1st_move*1000ULL) / total_cutoffs));
#endif

    *answer_move = Mvgen_Decompress_Move(GlobalPV.line_cmoves[0]);
    if ((is_analysis) && (time_is_up != TM_ABORT))
    /*timeout is also caused by abort command*/
    {
        Time_Wait_For_Abort();
        time_passed = Time_Passed();
        Search_Print_Move_Output(game_info.depth, game_info.eval, time_passed, is_normal_time);
    }
    *spent_nodes = g_nodes;
    *spent_time = time_passed;
    return(COMP_MOVE_FOUND);
}
