/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2015-2020, Rasmus Althoff <althoff@ct800.net>
 *  Copyright (C) 2010-2014, George Georgopoulos
 *
 *  This file is part of CT800/NGPlay.
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

/* ------------------------------------------------------ */
/*                      NGplay v 9.86                     */
/*    A C/C++ XBoard NegaScout Alpha/Beta Chess Engine    */
/* Code compiles using GNU g++ under Windows/Linux/Unix   */
/*                                                        */
/*  Features : NegaScout search, +LMR, +futility pruning  */
/*             iterative deepening, Null move heuristic,  */
/*             piece mobility + king safety evaluation,   */
/*             isolated + passed pawn evaluation,         */
/*             Basic Endgames (KQ,KR,KBB,KBN vs K)        */
/*                                                        */
/*         Author: George Georgopoulos (c) 2010-2014      */
/*         You are free to copy/derive from this code     */
/*         as long as you mention this copyright          */
/*         and keep the new code open source              */
/*                                                        */
/*    I should give credit to Tom Kerrigan and his TSCP   */
/* for inspiration about chess programming. Especially    */
/* his code for xboard interface and book opening helped  */
/* me a lot. However the main engine algorithm (search,   */
/* move generation and evaluation) was written completely */
/* from scratch.                                          */
/*    Also credit should be given to Bruce Moreland for   */
/* his excellent site which helped me a lot writing the   */
/* Principal Variation collection code.  thanks G.G.      */
/*                                                        */
/*                                                        */
/*                                                        */
/*                      NGplay v 9.87                     */
/* Few chesswise changes - cleaned up the C code so that  */
/* not just g++, but also gcc and any other ANSI C        */
/* compiler will take the code:                           */
/* - Removed default for the level variable in function   */
/*   headers as C (unlike C++) dows not allow parameter   */
/*   defaults for omitted parameters; instead the calling */
/*   functions use the new define NO_LEVEL which is just  */
/*   -1 and thus the former default value.                */
/* - replaced C++ // style comments by C comments.        */
/* - changed a lot of data types for the globals as to    */
/*   cut down the memory footprint for embedded systems.  */
/* - clarified the long decimal constants in the random   */
/*   function to be unsigned.                             */
/*                                                        */
/* => result: gcc does not complain even when using       */
/*    "gcc -Wall". The code should me more easily to      */
/*     port now, especially for embedded systems with     */
/*     picky compilers.                                   */
/*                                                        */
/* Changed a lot of C structures, defines, fixed some     */
/* bugs with the hash tables and the 3-fold repetition    */
/* recognition, replaced the quicksort by shellsort for   */
/* saving stack memory.                                   */
/* Moved the whole thing to a modest memory               */
/* footprint.                                             */
/* Used explicit NOINLINE when the function has a         */
/* considerable stack usage - inlining would add that up. */
/* Just to be sure that nothing breaks with new compiler  */
/* versions and different inlining behaviour.             */
/*                                                        */
/* Rewrote the opening book handling from line based to   */
/* position based (CRC32), from ASCII based to binary and */
/* made the opening book an inline compiled header file.  */
/*                                                        */
/* Massively enlarged the opening book (by more than a    */
/* factor of ten).                                        */
/*                                                        */
/* enhanced the rook handling (open files), added some    */
/* knight vs. bishop logic, added differently coloured    */
/* bishops logic, reduced the importance of queen         */
/* mobility during the opening, enhanced the king safety  */
/* evaluation, added some endgames (among them KP-K via   */
/* an endgame lookup table using free code from           */
/* Marcel van Kervinck), added backward pawn recognition, */
/* double pawn recognition, devalued wing pawn            */
/* majorities.                                            */
/*                                                        */
/* Thrown out the xboard code.                            */
/*                                                        */
/* Added 50-moves-draw recognition in the search tree if  */
/* a possible draw by that rule is only about 10 plies    */
/* from the board position.                               */
/* Added time controls.                                   */
/* Compeletely new handling of various options.           */
/* Added an embedded position viewer and editor.          */
/* Ported the whole thing to an ARM MCU.                  */
/*                                                        */
/* Author of these changes: Rasmus Althoff, 2016-2019.    */
/*                                                        */
/* ------------------------------------------------------ */

#include <stdint.h>
#include <stddef.h>
#include "ctdefs.h"
#include "confdefs.h"
#include "timekeeping.h"
#include "hardware.h"
#include "util.h"
#include "hmi.h"
#include "menu.h"
#include "move_gen.h"
#include "book.h"
#include "hashtables.h"
#include "eval.h"
#include "search.h"

#ifdef PC_PRINTF
#include <stdio.h>
#endif

/*--------- external variables ------------*/
/*-- READ-ONLY  --*/

/*-- READ-WRITE --*/
extern uint64_t hw_config;

/*--------- module variables ------------*/

static MOVE_REDO_STACK move_redo_stack;

/*--------- global variables ------------*/

GAME_INFO game_info;

/*holds the starting position of the game.
that does not have to be the initial chess position when a position was entered.*/
STARTING_POS starting_pos;

/* ------------- MAKE MOVE DEFINITIONS ----------------*/

/*bits 0-8: for gflags
bits 9-15: for en passant square
+1 because MAX_STACK is odd, and we can keep the alignment here*/
uint16_t cstack[MAX_STACK+1];
int cst_p;

CCM_RAM MVST move_stack[MAX_STACK+1];

/* ---------- TRANSPOSITION TABLE DEFINITIONS ------------- */

TT_ST     T_T[MAX_TT+CLUSTER_SIZE];
TT_ST Opp_T_T[MAX_TT+CLUSTER_SIZE];

/*pawn hash table*/
CCM_RAM TT_PTT_ST P_T_T[PMAX_TT+1];

/*separate table to avoid padding of the P_T_T table.*/
TT_PTT_ROOK_ST P_T_T_Rooks[PMAX_TT+1];

unsigned int hash_clear_counter;

/* -------------------- GLOBALS ------------------------- */

PIECE Wpieces[16];
PIECE Bpieces[16];
DATA_SECTION PIECE empty_p = {NULL, NULL,  0, 0, 0};
DATA_SECTION PIECE fence_p = {NULL, NULL, -1,-1,-1};

PIECE *board[120] = {
    &fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,
    &fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,
    &fence_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&fence_p,
    &fence_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&fence_p,
    &fence_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&fence_p,
    &fence_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&fence_p,
    &fence_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&fence_p,
    &fence_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&fence_p,
    &fence_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&fence_p,
    &fence_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&empty_p,&fence_p,
    &fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,
    &fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p,&fence_p
};

/*The reason that these tables are NOT as const is that they are heavily used
throughout the search process, and the ROM is much slower to access.
The prefetcher works well with code, but hardly with such scattered data access.

Astonishingly, 8 bit ints are even faster here than 32 bit ints because the
array addressing offset calculation is easier with 8 bits - although usually,
8 bit operations are slower on ARM.
*/

DATA_SECTION int8_t boardXY[120] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7,-1,
    -1, 8, 9,10,11,12,13,14,15,-1,
    -1,16,17,18,19,20,21,22,23,-1,
    -1,24,25,26,27,28,29,30,31,-1,
    -1,32,33,34,35,36,37,38,39,-1,
    -1,40,41,42,43,44,45,46,47,-1,
    -1,48,49,50,51,52,53,54,55,-1,
    -1,56,57,58,59,60,61,62,63,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

DATA_SECTION int8_t board64[64] = {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8
};

DATA_SECTION int8_t RowNum[120] = {
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,
    0,1,1,1,1,1,1,1,1,0,
    0,2,2,2,2,2,2,2,2,0,
    0,3,3,3,3,3,3,3,3,0,
    0,4,4,4,4,4,4,4,4,0,
    0,5,5,5,5,5,5,5,5,0,
    0,6,6,6,6,6,6,6,6,0,
    0,7,7,7,7,7,7,7,7,0,
    0,8,8,8,8,8,8,8,8,0,
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0
};

DATA_SECTION int8_t ColNum[120] = {
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,
    0,1,2,3,4,5,6,7,8,0,
    0,1,2,3,4,5,6,7,8,0,
    0,1,2,3,4,5,6,7,8,0,
    0,1,2,3,4,5,6,7,8,0,
    0,1,2,3,4,5,6,7,8,0,
    0,1,2,3,4,5,6,7,8,0,
    0,1,2,3,4,5,6,7,8,0,
    0,1,2,3,4,5,6,7,8,0,
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0
};

/*for move masking except MVV/LVA value*/
DATA_SECTION static MOVE mv_move_mask = {{0xFFU, 0xFFU, 0xFFU, 0}};

/*whether black (!) made the first move - can happen when entering a position.
  used for the move display.*/
int black_started_game;
int wking, bking;
int en_passant_sq;
unsigned int gflags; /*bit masks, see ctdefs.h*/

int mv_stack_p;

/*the "noise" option converted to a 0 - 100 integer.*/
int32_t eval_noise;

#ifdef G_NODES
uint64_t g_nodes;
#endif

int Starting_Mv;
enum E_COLOUR computer_side;
int fifty_moves;
int game_started_from_0;

LINE GlobalPV;
static MOVE player_move;

int dynamic_resign_threshold;

/*the following variables must be volatile because they might be changed from
the timer interrupt if the user wants to interrupt the computer's thinking.*/
volatile enum E_TIMEOUT time_is_up;

#ifdef DBGCUTOFF
uint64_t cutoffs_on_1st_move, total_cutoffs;
#endif

#ifdef DEBUG_STACK
size_t top_of_stack;
size_t max_of_stack;
#endif

/* -------------- UTILITY FUNCTIONS ---------------------------------- */

static void Play_Init_Pieces(void)
{
    int i;

    Util_Memzero(Wpieces, sizeof(Wpieces));
    Util_Memzero(Bpieces, sizeof(Bpieces));

    Wpieces[0].type = WKING;
    Wpieces[0].next = &Wpieces[1];
    Wpieces[0].prev = NULL;

    Wpieces[1].type = WQUEEN;
    Wpieces[1].next = &Wpieces[2];
    Wpieces[1].prev = &Wpieces[0];

    Wpieces[2].type = WROOK;
    Wpieces[2].next = &Wpieces[3];
    Wpieces[2].prev = &Wpieces[1];

    Wpieces[3].type = WROOK;
    Wpieces[3].next = &Wpieces[4];
    Wpieces[3].prev = &Wpieces[2];

    Wpieces[4].type = WBISHOP;
    Wpieces[4].next = &Wpieces[5];
    Wpieces[4].prev = &Wpieces[3];

    Wpieces[5].type = WBISHOP;
    Wpieces[5].next = &Wpieces[6];
    Wpieces[5].prev = &Wpieces[4];

    Wpieces[6].type = WKNIGHT;
    Wpieces[6].next = &Wpieces[7];
    Wpieces[6].prev = &Wpieces[5];

    Wpieces[7].type = WKNIGHT;
    Wpieces[7].next = &Wpieces[8];
    Wpieces[7].prev = &Wpieces[6];

    for (i=8; i<16; i++) {
        Wpieces[i].type = WPAWN;
        if (i==15) {
            Wpieces[i].next = NULL;
        } else {
            Wpieces[i].next = &Wpieces[i+1];
        }
        Wpieces[i].prev = &Wpieces[i-1];
    }

    Bpieces[0].type = BKING;
    Bpieces[0].next = &Bpieces[1];
    Bpieces[0].prev = NULL;

    Bpieces[1].type = BQUEEN;
    Bpieces[1].next = &Bpieces[2];
    Bpieces[1].prev = &Bpieces[0];

    Bpieces[2].type = BROOK;
    Bpieces[2].next = &Bpieces[3];
    Bpieces[2].prev = &Bpieces[1];

    Bpieces[3].type = BROOK;
    Bpieces[3].next = &Bpieces[4];
    Bpieces[3].prev = &Bpieces[2];

    Bpieces[4].type = BBISHOP;
    Bpieces[4].next = &Bpieces[5];
    Bpieces[4].prev = &Bpieces[3];

    Bpieces[5].type = BBISHOP;
    Bpieces[5].next = &Bpieces[6];
    Bpieces[5].prev = &Bpieces[4];

    Bpieces[6].type = BKNIGHT;
    Bpieces[6].next = &Bpieces[7];
    Bpieces[6].prev = &Bpieces[5];

    Bpieces[7].type = BKNIGHT;
    Bpieces[7].next = &Bpieces[8];
    Bpieces[7].prev = &Bpieces[6];

    for (i=8; i<16; i++) {
        Bpieces[i].type = BPAWN;
        if (i==15) {
            Bpieces[i].next = NULL;
        } else {
            Bpieces[i].next = &Bpieces[i+1];
        }
        Bpieces[i].prev = &Bpieces[i-1];
    }
}

static void Play_Empty_Board(void)
{
    int i;
    Play_Init_Pieces();
    for (i = 0; i < 64; i++) {
        board[board64[i]]=&empty_p;
    }
}

static void Play_Clear_All_Tables(void)
{
    Hash_Clear_Tables();
    Util_Memzero(P_T_T, sizeof(P_T_T));
    Util_Memzero(P_T_T_Rooks, sizeof(P_T_T_Rooks));
    hash_clear_counter = 0;
}

static int Play_Should_Swap_Move(int from, int to, const MOVE *restrict movelist, int move_cnt)
{
    int i;

    for (i = 0; i < move_cnt; i++)
    {
        /*is the move in the list? then don't swap.*/
        if ((from == movelist[i].m.from) && (to == movelist[i].m.to))
            return(0);
    }
    for (i = 0; i < move_cnt; i++)
    {
        /*is the swapped move in the list? then swap.*/
        if ((from == movelist[i].m.to) && (to == movelist[i].m.from))
            return(1);
    }
    /*neither move nor swapped move is in the list, so don't swap.*/
    return(0);
}


static void Play_Update_Special_Conditions(MOVE amove);

/*this is Search_Retract_Last_Move plus taking care of the 50 moves draw counter
and the time keeping.
returns 1 if undo is OK and 0 if we are in the starting position and there
is nothing to undo. This function is not used during the search, but for
the user interface, i.e. if the user wants to take back the last move (two plies).*/
static int Play_Undo_Last_Ply(void)
{
    int i;
    /*some notes on the limit checks and takeback order here:
    - when a ply gets made, mv_stack_p is incremented, and then the move gets onto the stack.
    - when a ply gets undone, the ply is read from the stack, and then mv_stack_p is decremented.
    consequence for the first move from white: position 0 in the stack is always empty!

    The first ply from white increments mv_stack_p to one and saves the move, the following
    black ply increments mv_stack_p to two and saves the stack. A full first move thus features
    mv_stack_p at 2. That's why we return when mv_stack_p is less then 2.

    The undo order of Search_Retract_Last_Move and Time_Undo is essential because Search_Retract_Last_Move has
    the first white ply at mv_stack_p=1, but decrements this to 0. Ply 0 is exactly what
    Time_Undo expects as first white ply.*/

    if (mv_stack_p < 1) /*nothing to take back*/
        return (0);

    game_info.valid = EVAL_INVALID;
    game_info.last_valid_eval = NO_RESIGN;

    /* put the undone moves onto the redo stack*/

    while (move_redo_stack.index >= MAX_MOVE_REDO-1)
        /*re(!)do stack is full, so shift everything down by one. that's a bit ugly, but this isn't a time-critical function.
        the advantage is that the undo is easier with a stack than with a ring buffer.*/
    {
        for (i = 0; i < MAX_MOVE_REDO - 1; i++)
        {
            move_redo_stack.buffer[i] = move_redo_stack.buffer[i+1];
        }
        move_redo_stack.index--;
    }

    /*the redo move stack is using a compressed move format.*/
    move_redo_stack.buffer[move_redo_stack.index] = Mvgen_Compress_Move(move_stack[mv_stack_p].move);
    move_redo_stack.index++;

    Search_Retract_Last_Move();
    Search_Pop_Status();
    Time_Undo(mv_stack_p);

    /*is the move before from the book?*/
    if ((mv_stack_p > 0) && (computer_side != NONE))
    {
        MOVE this_move;
        int dummy;

        this_move.u =  move_stack[mv_stack_p].move.u;
        Search_Retract_Last_Move();
        Search_Pop_Status();

        if (Book_Is_Line(&dummy, &this_move, 1))
            game_info.valid = EVAL_BOOK;

        Search_Push_Status();
        Search_Make_Move(this_move);
    }

    /*the global principal variation is messed up at this point anyway,
    so don't check whether the player move fits the global PV.*/
    player_move.u = MV_NO_MOVE_MASK;

    i = mv_stack_p;
    fifty_moves = 0;

    while (i > 0) /*the stack entry 0 itself does NOT contain a valid move.*/
    {
        if (!(move_stack[i].captured->type /* capture */ || move_stack[i].move.m.flag>1 /* pawn move */))
        {
            fifty_moves++;
        } else
            /*we just hit the most recent pawn or capture move, so stop the search.*/
        {
            return (1); /*successful undo*/
        }
        i--;
    }
    return(1); /*successful undo*/
}

static int Play_Undo_Last_Move(void)
{
    if (mv_stack_p < 2) /*nothing to take back*/
        return (0);
    Play_Undo_Last_Ply();
    Play_Undo_Last_Ply();
    Time_Set_Current();
    return(1);
}

static int Play_Redo_Last_Move(void)
{
    MOVE amove;
    int dummy;

    if (move_redo_stack.index < 2) /*nothing to redo*/
        return (0);

    game_info.valid = EVAL_INVALID;
    game_info.last_valid_eval = NO_RESIGN;

    /*the global principal variation is messed up at this point anyway,
    so don't check whether the player move fits the global PV.*/
    player_move.u = MV_NO_MOVE_MASK;

    Time_Redo(mv_stack_p);

    move_redo_stack.index--;
    /*the redo move stack is using a compressed move format.*/
    amove = Mvgen_Decompress_Move(move_redo_stack.buffer[move_redo_stack.index]);

    Play_Update_Special_Conditions(amove);
    Search_Push_Status();
    Search_Make_Move(amove);

    Time_Redo(mv_stack_p);

    move_redo_stack.index--;
    /*the redo move stack is using a compressed move format.*/
    amove = Mvgen_Decompress_Move(move_redo_stack.buffer[move_redo_stack.index]);

    /*is the upcoming move from the book?*/
    if ((computer_side != NONE) && (Book_Is_Line(&dummy, &amove, 1)))
        game_info.valid = EVAL_BOOK;

    Play_Update_Special_Conditions(amove);
    Search_Push_Status();
    Search_Make_Move(amove);

    Time_Set_Current();

    return(1);
}

/* ---------------- TEXT INPUT/OUTPUT ----------------------- */

#ifdef PC_PRINTF
#ifdef SHOW_BOARD
static char *Play_Piece_Icon(int x, int y)
{
    static char s[3];
    int c = ((x+y)%2) ? ' ' : '-';
    int piece_type = board[10*y+x+21]->type;
    s[0]=c;
    s[1]=c;
    s[2]='\0';
    if (piece_type != NO_PIECE)
        s[0] = Hmi_Get_Piece_Char(piece_type, MIXEDCASE);
    return(s);
}
#endif

void Play_Show_Board(void)
{
#ifdef SHOW_BOARD
     int x,y;
    fprintf(stderr,"\n\n");
    fprintf(stderr,"   +--+--+--+--+--+--+--+--+");
    fprintf(stderr,"\n");
    for (y=7; y>0; y--) {
        fprintf(stderr,"%2d ",y+1);
        for (x=0; x<8; x++) {
            fprintf(stderr,"|%s",Play_Piece_Icon(x,y));
        }
        fprintf(stderr,"|   ");
        if (y==7) {
            fprintf(stderr,"\n   +--+--+--+--+--+--+--+--+   ");
            printf("\n");
        } else if (y==6) {
            fprintf(stderr,"\n   +--+--+--+--+--+--+--+--+");
            fprintf(stderr,"\n");
        } else if (y==5) {
            fprintf(stderr,"\n   +--+--+--+--+--+--+--+--+   ");
            printf("\n");
        } else {
            fprintf(stderr,"\n   +--+--+--+--+--+--+--+--+\n");
        }
    }
    fprintf(stderr," 1 ");
    for (x=0; x<8; x++) {
        fprintf(stderr,"|%s",Play_Piece_Icon(x,y));
    }
    fprintf(stderr,"|\n   +--+--+--+--+--+--+--+--+\n");
    fprintf(stderr,"    a  b  c  d  e  f  g  h\n\n");
#endif
}

char *Play_Translate_Moves(MOVE m)
{
    static char mov[6];

    if (m.u != MV_NO_MOVE_MASK)
    {
        char fromx,fromy,tox,toy;

        fromx = m.m.from%10 - 1;
        tox   = m.m.to%10 - 1;
        fromy = m.m.from/10 - 2;
        toy   = m.m.to/10 - 2;

        mov[0] = fromx + 'a';
        mov[1] = fromy + '1';
        mov[2] = tox + 'a';
        mov[3] = toy + '1';

        switch(m.m.flag)
        {
        case WROOK  :
        case BROOK  :
            mov[4] = BROOK_CHAR;
            mov[5] = '\0';
            break;
        case WKNIGHT:
        case BKNIGHT:
            mov[4] = BKNIGHT_CHAR;
            mov[5] = '\0';
            break;
        case WBISHOP:
        case BBISHOP:
            mov[4] = BBISHOP_CHAR;
            mov[5] = '\0';
            break;
        case WQUEEN :
        case BQUEEN :
            mov[4] = BQUEEN_CHAR;
            mov[5] = '\0';
            break;
        default     :
            mov[4] = '\0';
            break;
        }
    } else
        Util_Strcpy(mov, "0000");
    return mov;
}

#endif /*PC_PRINTF*/


/* ----------------- MAIN FUNCTIONS ------------------------------------- */

static void Play_Set_Starting_Position(void)
{
    int i;
    Play_Empty_Board();

    /*now the starting position for the save-game-feature*/
    starting_pos.board[0] = starting_pos.board[7] = WROOK;
    starting_pos.board[1] = starting_pos.board[6] = WKNIGHT;
    starting_pos.board[2] = starting_pos.board[5] = WBISHOP;
    starting_pos.board[3] = WQUEEN;
    starting_pos.board[4] = WKING;

    starting_pos.board[56] = starting_pos.board[63] = BROOK;
    starting_pos.board[57] = starting_pos.board[62] = BKNIGHT;
    starting_pos.board[58] = starting_pos.board[61] = BBISHOP;
    starting_pos.board[59] = BQUEEN;
    starting_pos.board[60] = BKING;

    for (i = 8; i < 16; i++)
        starting_pos.board[i] = WPAWN;
    for (i = 48; i < 56; i++)
        starting_pos.board[i] = BPAWN;

    Eval_Zero_Initial_Material();

    Util_Memzero(starting_pos.board+16, 32);

    starting_pos.epsquare = BP_NOSQUARE;

    board[A1]=&Wpieces[2];
    Wpieces[2].xy = A1;
    board[H1]=&Wpieces[3];
    Wpieces[3].xy = H1; /* WROOKS */
    board[A8]=&Bpieces[2];
    Bpieces[2].xy = A8;
    board[H8]=&Bpieces[3];
    Bpieces[3].xy = H8; /* BROOKS */
    board[G1]=&Wpieces[6];
    Wpieces[6].xy = G1;
    board[B1]=&Wpieces[7];
    Wpieces[7].xy = B1; /* WKNIGHTS */
    board[G8]=&Bpieces[6];
    Bpieces[6].xy = G8;
    board[B8]=&Bpieces[7];
    Bpieces[7].xy = B8; /* BKNIGHTS */
    board[F1]=&Wpieces[4];
    Wpieces[4].xy = F1;
    board[C1]=&Wpieces[5];
    Wpieces[5].xy = C1; /* WBISHOPS */
    board[F8]=&Bpieces[4];
    Bpieces[4].xy = F8;
    board[C8]=&Bpieces[5];
    Bpieces[5].xy = C8; /* BBISHOPS */
    board[D1]=&Wpieces[1];
    Wpieces[1].xy=D1; /* WQUEEN */
    board[D8]=&Bpieces[1];
    Bpieces[1].xy=D8; /* BQUEEN */
    board[E1]=&Wpieces[0];
    Wpieces[0].xy = E1; /* WKING */
    board[E8]=&Bpieces[0];
    Bpieces[0].xy = E8; /* BKING */
    for (i = 0; i<8; i++) {
        board[i+A2] = &Wpieces[i+8];
        Wpieces[i+8].xy = i+A2; /* WPAWNS */
        board[i+A7] = &Bpieces[i+8];
        Bpieces[i+8].xy = i+A7; /* BPAWNS */
    }

    /*castling flags reset*/
    starting_pos.gflags = FLAGRESET;
    /*if white starts, then the (imaginary) move in move_stack[0] would
      have been a black move. reset all other flags.*/
    gflags = BLACK_MOVED;

    en_passant_sq = 0;
    wking = E1;
    bking = E8;
    black_started_game = 0;
    /*Move Stack Pointers reset */
    cst_p = 0;
    mv_stack_p = 0;
    /*zero out the stacks themselves. Not strictly necessary, but in case
    of problems, it is impossible to use bug reports which might depend on
    the last umpteen games played.*/
    Util_Memzero(&GlobalPV, sizeof(GlobalPV));
    Util_Memzero(move_stack, sizeof(move_stack));
    Util_Memzero(cstack, sizeof(cstack));
    fifty_moves = 0;
    player_move.u = MV_NO_MOVE_MASK;
    game_started_from_0 = 1;
    move_redo_stack.index = 0;
    game_info.valid = EVAL_INVALID;
    game_info.last_valid_eval = NO_RESIGN;
    dynamic_resign_threshold = RESIGN_EVAL;
    Time_Init_Game(computer_side, mv_stack_p);
    Hash_Init_Stack();
}


/* ------------------- GAME CONSOLE ----------------------------------- */

static void NEVER_INLINE Play_Get_Game_Info(char *evalbuffer, char *depthbuffer)
{
    if (game_info.valid == EVAL_MOVE)
    {
        int full_depth;

        if (Abs(game_info.eval) < MATE_CUTOFF)
        {
            if (game_info.eval != 0)
            {
                Util_Strcpy(evalbuffer, "eval: +99.99 ps");
                Util_Centipawns_To_String(evalbuffer + 6, game_info.eval);
                /*eliminate leading space if the eval has only one digit (before the .)*/
                if (evalbuffer[6] == ' ')
                {
                    int cp;
                    for (cp = 6; evalbuffer[cp] != 0; cp++)
                        evalbuffer[cp] = evalbuffer[cp + 1];
                }
            } else
                Util_Strcpy(evalbuffer, "eval: 0.00 ps");
        } else
        {
            if (game_info.eval > 0)
                Util_Strcpy(evalbuffer, "eval: +mate");
            else
                Util_Strcpy(evalbuffer, "eval: -mate");
        }
        full_depth = game_info.depth;
        if (full_depth <= 0) /*just be sure never to display nonsense*/
            Util_Strcpy(depthbuffer, "depth: --");
        else
        {
            int select_depth;
            char conv_buf[3];
            Util_Strcpy(depthbuffer, "depth: ");
            Util_Depth_To_String(conv_buf, full_depth);
            /*eliminate leading space if the full depth has only one digit*/
            if (full_depth < 10)
            {
                conv_buf[0] = conv_buf[1];
                conv_buf[1] = '\0';
            } else
                conv_buf[2] = '\0';
            Util_Strcat(depthbuffer, conv_buf);

            select_depth = GlobalPV.line_len;
            if (select_depth > full_depth)
                /*only do the x/y split display if the global PV is deeper than the full depth.*/
            {
                Util_Strcat(depthbuffer, "/");
                Util_Depth_To_String(conv_buf, select_depth);
                /*eliminate leading space if the selective depth has only one digit*/
                if (select_depth < 10)
                {
                    conv_buf[0] = conv_buf[1];
                    conv_buf[1] = '\0';
                } else
                    conv_buf[2] = '\0';
                Util_Strcat(depthbuffer, conv_buf);
            }

            Util_Strcat(depthbuffer, " pls");
        }

    } else if (game_info.valid == EVAL_BOOK)
    {
        Util_Strcpy(evalbuffer,  "eval: book");
        Util_Strcpy(depthbuffer, "depth: -- ");
    } else
        /* we are not in the book, and there was no move calculation. That can happen
            with forced moves, when there is only one legal move to play. In this case, the
            depth information is useless, but if there was a valid evaluation before, we take
            the last saved valid value. Usually, a forced move doesn't change the position value
            because it has shown up before, in the search tree.*/
    {
        if (game_info.last_valid_eval != NO_RESIGN) /*is there a valid move?*/
        {
            if (Abs(game_info.last_valid_eval) < MATE_CUTOFF)
            {
                if (game_info.last_valid_eval != 0)
                {
                    Util_Strcpy(evalbuffer, "eval: +99.99 ps");
                    Util_Centipawns_To_String(evalbuffer + 6, game_info.last_valid_eval);
                    /*eliminate leading space if the eval has only one digit (before the .)*/
                    if (evalbuffer[6] == ' ')
                    {
                        int cp;
                        for (cp = 6; evalbuffer[cp] != 0; cp++)
                            evalbuffer[cp] = evalbuffer[cp + 1];
                    }
                } else
                    Util_Strcpy(evalbuffer, "eval: 0.00 ps");

            } else
            {
                if (game_info.last_valid_eval > 0)
                    Util_Strcpy(evalbuffer, "eval: +mate");
                else
                    Util_Strcpy(evalbuffer, "eval: -mate");
            }
        } else /*ok, no way to guess an evaluation.*/
            Util_Strcpy(evalbuffer,  "eval:  --");
        Util_Strcpy(depthbuffer, "depth: --");
    }
}

/*gets the first six plies of the PV.*/
static void NEVER_INLINE Play_Get_Pv_Info(char *line1, char *line2)
{
    int i, len;

    line1[0] = line2[0] = '\0';

    for (i = 1, len = 0; ((i < GlobalPV.line_len) && (i < 4)); i++)
    {
        MOVE decomp_move = Mvgen_Decompress_Move(GlobalPV.line_cmoves[i]);
        len += Util_Convert_Moves(decomp_move, line1 + len);
        line1[len++] = ' ';
    }
    if (len > 0)
        /*remove the space character at the end of the line*/
        line1[len - 1] = '\0';

    for (i = 4, len = 0; ((i < GlobalPV.line_len) && (i < 7)); i++)
    {
        MOVE decomp_move = Mvgen_Decompress_Move(GlobalPV.line_cmoves[i]);
        len += Util_Convert_Moves(decomp_move, line2 + len);
        line2[len++] = ' ';
    }
    if (len > 0)
        /*remove the space character at the end of the line*/
        line2[len - 1] = '\0';
}

static int NEVER_INLINE Play_Get_User_Input(char *buffer)
{
    int32_t conf_time = 0;
    int player_side, chars_entered, cursor_pos, move_cnt;
    MOVE movelist[MAXMV];
    char textline1[21], textline2[21], user_char;

    Hmi_Clear_Input_Move();

    chars_entered = 0;
    cursor_pos = 0;
    buffer[0] = '\0';
    if (((mv_stack_p + black_started_game) & 1L) == 0)
        player_side = WHITE;
    else
        player_side = BLACK;

    /*get the list of legal moves for the coordinate swapping feature.*/
    (void) Search_Get_Root_Move_List(movelist, &move_cnt, player_side, NO_MATE_CHECK);

    for (;;)
    {
        /*when gathering user input from sources other than the keypad, such
          as an external board, allow Hw_Getch() sleep only when the other
          source has been silent. Otherwise, call it with SLEEP_FORBIDDEN.*/
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        if (mv_stack_p > 0)
        /*no time checking in the initial position - that starts after
          white's first move.*/
        {
            /*because of the printing which inserts a hyphen, the cursor position is
            one further right after the second entered character.*/
            if (Time_Player_Intermediate_Check(player_side, HMI_MOVE_POS+cursor_pos) == TIME_FAIL)
            {
                int user_answer;
                if (player_side == WHITE)
                {
                    if (Bpieces[0].next != NULL) /*if black has only the king, timeout still is draw.*/
                        (void) Hmi_Conf_Dialogue("time: white lost.", "0 : 1", &conf_time,
                                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                    else
                        (void) Hmi_Conf_Dialogue("time: draw.", "1/2 : 1/2", &conf_time,
                                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                }
                else
                {
                    if (Wpieces[0].next != NULL) /*if white has only the king, timeout still is draw.*/
                        (void) Hmi_Conf_Dialogue("time: black lost.", "1 : 0", &conf_time,
                                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                    else
                        (void) Hmi_Conf_Dialogue("time: draw.", "1/2 : 1/2", &conf_time,
                                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                }
                user_answer = Hmi_Conf_Dialogue("    new game: OK", "cont. in TPM: CL", &conf_time,
                                                HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
                if (user_answer == HMI_USER_CANCEL) /*the user wants to continue the game in time per move mode*/
                {
                    Hmi_Restore_Viewport();
                    Time_Change_To_TPM_After_Loss();
                    Hmi_Set_Cursor(HMI_MOVE_POS + cursor_pos, DISP_CURSOR_ON);
                } else
                {
                    /*new viewport will be opened in main() anyway, so don't restore.*/
                    return (HMI_NEW_GAME);
                }
            }
        }

        if (Hmi_Battery_Info(USER_TURN, &conf_time ))
        {
            Time_Add_Conf_Delay(conf_time);
            Hmi_Set_Cursor(HMI_MOVE_POS + cursor_pos, DISP_CURSOR_ON);
        }

        if (user_key != KEY_NONE)
        {
            uint32_t user_key_mask;

            /*virtual key for cancelling a started item only.
              might be useful once a DGT board or so gets attached, then
              keyboard input and started board input can collide.*/
            if ((user_key == KEY_V_FCL) && ((chars_entered == 1) || (chars_entered == 3)))
            {
                Hmi_Disp_Move_Char('\b', chars_entered);
                chars_entered--;
                buffer[chars_entered] = '\0';
                cursor_pos = chars_entered;
                if (cursor_pos > 1)
                    cursor_pos++;
                continue;
            }

            user_key_mask = 1UL << user_key;

            if ((user_key_mask & KEY_SQUARES_MASK) != 0)
            /*append character to buffer, display it, store cursor display etc*/
            {
                switch (chars_entered)
                {
                case 0:
                case 2:
                    user_char = Util_Key_To_Char(user_key);
                    Hmi_Disp_Move_Char(user_char, chars_entered);
                    buffer[chars_entered] = user_char;
                    chars_entered++;
                    buffer[chars_entered] = '\0';
                    break;

                case 1:
                case 3:
                    user_char = Util_Key_To_Digit(user_key);
                    if (chars_entered == 3) /*source and destination given.*/
                    {
                        int from, to;

                        if ((buffer[0] == buffer[2]) && /*are the files the same?*/
                            (buffer[1] == user_char)) /*are the ranks the same?*/
                        {
                            /*erase target square.
                              maybe there is a bouncing external chess board.*/
                            Hmi_Disp_Move_Char('\b', 3);
                            chars_entered = 2;
                            buffer[chars_entered] = '\0';
                            break;
                        }

                        /*now check whether source and destination look OK.*/
                        from = board64[(buffer[0] - 'a') + (buffer[1] - '1') * 8];
                        to   = board64[(buffer[2] - 'a') + (user_char - '1') * 8];

                        /*as to avoid confusing the user, only exchange squares
                          if it makes a legal move out of an illegal entry.*/
                        if (Play_Should_Swap_Move(from, to, movelist, move_cnt))
                        {
                            /*swap from/to in the display*/
                            buffer[3] = buffer[1];
                            buffer[1] = user_char;
                            user_char = buffer[0];
                            buffer[0] = buffer[2];
                            buffer[2] = user_char;
                            chars_entered = 4;
                            buffer[4] = '\0';
                            Hmi_Disp_Move_Line(buffer, 4);
                            break;
                        }
                    } /*end of treatment for destination rank entry*/

                    Hmi_Disp_Move_Char(user_char, chars_entered);
                    buffer[chars_entered] = user_char;
                    chars_entered++;
                    buffer[chars_entered] = '\0';
                    break;

                case 4:
                    /*this can only be promotions. do some sanity checking to prevent
                      the queening parameter from appearing after every possible move.*/
                    if ((user_key_mask & KEY_PROM_ALL_MASK) != 0)
                    {
                        if ((buffer[1] == '7') && (buffer[3] == '8')) /*white promotion*/
                        {
                            int xy;
                            if (buffer[0] >= 'a')
                                xy = buffer[0] - 'a' + A1;
                            else
                                xy = buffer[0] - 'A' + A1;
                            xy += (buffer[1] - '1') * 10;
                            if (board[xy]->type == WPAWN)
                            {
                                int file1, file2;
                                file1 = (int) buffer[0];
                                file2 = (int) buffer[2];
                                if (Abs(file1-file2) <= 1)
                                {
                                    user_char = Util_Key_To_Prom(user_key);
                                    Hmi_Disp_Move_Char(user_char, chars_entered);
                                    buffer[chars_entered] = user_char;
                                    chars_entered++;
                                    buffer[chars_entered] = '\0';
                                }
                            }
                        } else if ((buffer[1] == '2') && (buffer[3] == '1')) /*black promotion*/
                        {
                            int xy;
                            if (buffer[0] >= 'a')
                                xy = buffer[0] - 'a' + A1;
                            else
                                xy = buffer[0] - 'A' + A1;
                            xy += (buffer[1] - '1') * 10;
                            if (board[xy]->type == BPAWN)
                            {
                                int file1, file2;
                                file1 = (int) buffer[0];
                                file2 = (int) buffer[2];
                                if (Abs(file1-file2) <= 1)
                                {
                                    user_char = Util_Key_To_Prom(user_key);
                                    Hmi_Disp_Move_Char(user_char, chars_entered);
                                    buffer[chars_entered] = user_char;
                                    chars_entered++;
                                    buffer[chars_entered] = '\0';
                                }
                            }
                        }
                    }
                    break;

                default:
                    break;
                }
                cursor_pos = chars_entered;
                if (cursor_pos > 1)
                    cursor_pos++;
            } else if (user_key == KEY_ENT)
            {
                if (chars_entered > 3) /*a move has at least 4 characters.*/
                {
                    buffer[chars_entered] = '\0';
                    /*the user has had the opportunity to get in the menu for disabling the O/C mode
                    -> prevent device bricking (overclock is beyond chip specification).*/
                    Hw_User_Interaction_Passed();
                    return (HMI_GENERAL_INPUT);
                }
            } else if (user_key == KEY_CL) /*backspace*/
            {
                if (chars_entered > 0)
                {
                    buffer[0] = '\0';
                    chars_entered = 0;
                    cursor_pos = 0;
                    Hmi_Clear_Input_Move();
                }
            } else if (user_key == KEY_POS_DISP)
            /*if the character just pressed was the "display position" character*/
            {
                (void) Hmi_Display_Current_Board((player_side == WHITE), &conf_time, HMI_RESTORE);
                Time_Add_Conf_Delay(conf_time);
                Hmi_Set_Cursor(HMI_MOVE_POS + cursor_pos, DISP_CURSOR_ON);
            } else if (user_key == KEY_INFO)
            /*if the user pressed the "info" key, display the evaluation, the reached depth and a bit of the PV.*/
            {
                Play_Get_Game_Info(textline1, textline2);
                if ((game_info.valid == EVAL_MOVE) && (GlobalPV.line_len > 1))
                /*otherwise, there is no valid PV anyway*/
                {
                    enum E_HMI_USER user_answer;

                    user_answer = Hmi_Conf_Dialogue(textline1, textline2, &conf_time,
                                                    HMI_NO_TIMEOUT, HMI_MULTI_STAT, HMI_NO_RESTORE);
                    Time_Add_Conf_Delay(conf_time);
                    if (user_answer == HMI_USER_OK)
                    {
                        Play_Get_Pv_Info(textline1, textline2);
                        (void) Hmi_Conf_Dialogue(textline1, textline2, &conf_time,
                                                 HMI_NO_TIMEOUT, HMI_PV, HMI_RESTORE);
                        Time_Add_Conf_Delay(conf_time);
                    } else
                        Hmi_Restore_Viewport();
                } else
                {
                    (void) Hmi_Conf_Dialogue(textline1, textline2, &conf_time,
                                             HMI_NO_TIMEOUT, HMI_MONO_STAT, HMI_RESTORE);
                    Time_Add_Conf_Delay(conf_time);
                }
                Hmi_Set_Cursor(HMI_MOVE_POS + cursor_pos, DISP_CURSOR_ON);
            } else if (user_key == KEY_UNDO) /*undo move*/
            {
                buffer[0] = '\0';
                return (HMI_UNDO_MOVE);
            } else if (user_key == KEY_REDO) /*redo move*/
            {
                buffer[0] = '\0';
                return (HMI_REDO_MOVE);
            } else if (user_key == KEY_GO) /*change sides*/
            {
                buffer[0] = '\0';
                /*the user has had the opportunity to get in the menu for disabling the O/C mode
                -> prevent device bricking (overclock is beyond chip specification).*/
                Hw_User_Interaction_Passed();
                game_info.eval = -game_info.eval;
                if (game_info.last_valid_eval != NO_RESIGN)
                    game_info.last_valid_eval = -game_info.last_valid_eval;
                Play_Clear_All_Tables();
                return (HMI_COMP_GO);
            } else if (user_key == KEY_MENU) /*configuration menu selected*/
            {
                uint64_t old_config = hw_config;

                /*user menu access goes here*/
                int user_answer = Menu_Main(&conf_time, mv_stack_p, black_started_game, game_started_from_0);

                if ( (CFG_HAS_OPT(CFG_COMP_SIDE_MODE, CFG_COMP_SIDE_NONE)) ||
                     (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_MTI)) )
                /*in mate search mode, the computer has to be triggered manually.*/
                    computer_side = NONE;

                Time_Add_Conf_Delay(conf_time);

                if (user_answer == HMI_MENU_NEW_GAME)
                    return (HMI_NEW_GAME);
                if (user_answer == HMI_MENU_NEW_POS)
                    return (HMI_NEW_POS);

                if ((old_config & CFG_TIME_RELATED) != (hw_config & CFG_TIME_RELATED))
                /*time control mode changed*/
                {
                    if (mv_stack_p < 2)
                    {
                        /*reinitialise the time when it's during the first move
                        - maybe we changed to serious time controls via the menu?*/
                        Time_Init_Game(computer_side, mv_stack_p);
                        Time_Give_Bonus(WHITE, computer_side);
                        if (mv_stack_p == 1)
                            Time_Give_Bonus(BLACK, computer_side);
                    } else
                        /*can only be a change to TPM or AN*/
                        Time_Set_Current();

                    Time_Set_Start(mv_stack_p);
                }

                Hmi_Build_Game_Screen(computer_side, black_started_game, game_started_from_0, HMI_NO_CONFIRM, &game_info);
                Hmi_Disp_Move_Line(buffer, chars_entered);
            }
        } /*if there is a key pressed*/
    } /*end of endless loop.
        we return after ENTER, UNDO, REDO, GO, NEW GAME, NEW POS
        from within the loop.*/

    return (HMI_GENERAL_INPUT); /*to avoid compiler warnings*/
}

static enum E_HMI_INPUT NEVER_INLINE
Play_Get_Player_Move(int *xy1, int *xy2, int *flag)
{
    char buf[10];
    int x1,y1,x2,y2;
#ifdef PC_PRINTF
    int prompted = 0;
#endif
    int32_t conf_time;

    for (;;) {
#ifdef PC_PRINTF
        if (!prompted) {
            fprintf(stderr,"\nGive move: ");
            prompted = 1;
        }
#endif
        int user_answer = Play_Get_User_Input(buf);
        Hmi_Set_Cursor(0, DISP_CURSOR_OFF);
        if (user_answer == HMI_NEW_GAME)
            return(HMI_NEW_GAME);
        if (user_answer == HMI_NEW_POS)
            return(HMI_NEW_POS);
        /*undo is only allowed for full moves.*/
        if (user_answer == HMI_UNDO_MOVE)
        {
            if (mv_stack_p > 1) {
                if (Time_Undo_OK())
                    return (HMI_UNDO_MOVE);
                else
                /*the timekeeping stack is empty. if the user really wants this
                this undo, the time control mode must be changed to time per move.
                so ask the user for confirmation.*/
                {
                    if (Hmi_Conf_Dialogue("time undo fails,", "change to TPM?", &conf_time,
                                          HMI_NO_TIMEOUT, HMI_QUESTION, HMI_RESTORE) == HMI_USER_OK)
                    {
                        Time_Add_Conf_Delay(conf_time);
                        return (HMI_UNDO_MOVE);
                    }
                    else
                    /*the else of the HMI interaction. The user declined, but we still spent time with the dialogue!*/
                    {
                        Time_Add_Conf_Delay(conf_time);
                        Hmi_Signal(HMI_MSG_ERROR);
#ifdef PC_PRINTF
                        fprintf(stderr,"\r\nNo undo!");
#endif
                    }
                }
            } else
            {
                Hmi_Signal(HMI_MSG_ERROR);
#ifdef PC_PRINTF
                fprintf(stderr,"\r\nNothing to undo!");
#endif
            }
        } else
        /*redo is only allowed if there is something to redo.*/
        if (user_answer == HMI_REDO_MOVE)
        {
            if (move_redo_stack.index > 1) {
                return (HMI_REDO_MOVE);
            } else
            {
                Hmi_Signal(HMI_MSG_ERROR);
#ifdef PC_PRINTF
                fprintf(stderr,"\r\nNothing to redo!");
#endif
            }
        } else
        /*switching sides*/
        if (user_answer == HMI_COMP_GO)
            return (HMI_COMP_GO);
        else
        {
            if ((buf[0] >= 'a') && (buf[0] <= 'h')) {
                x1 = buf[0] - 'a';
                if ((buf[1] >= '1') && (buf[1] <= '8')) {
                    y1 = buf[1] - '1';
                    if ((buf[2] >= 'a') && (buf[2] <= 'h')) {
                        x2 = buf[2] - 'a';
                        if ((buf[3] >= '1') && (buf[3] <= '8')) {
                            y2 = buf[3] - '1';
                            break;
                        }
                    }
                }
            }
        }
    }
    *xy1 = 10*y1 + x1 + 21;
    *xy2 = 10*y2 + x2 + 21;
    /*if the user promotes a pawn, but does not specify the piece, assume that he wants a queen.*/
    if (board[*xy1]->type == WPAWN) {
        if (*xy2 >= A8) {
            switch(buf[4]) {
            case WROOK_CHAR:
                *flag = WROOK;
                break;
            case WKNIGHT_CHAR:
                *flag = WKNIGHT;
                break;
            case WBISHOP_CHAR:
                *flag = WBISHOP;
                break;
            case WQUEEN_CHAR:
            default:
                *flag = WQUEEN;
                break;
            }
        } else *flag = WPAWN;
    } else if (board[*xy1]->type == BPAWN) {
        if (*xy2 <= H1) {
            switch(buf[4]) {
            case WROOK_CHAR:
                *flag = BROOK;
                break;
            case WKNIGHT_CHAR:
                *flag = BKNIGHT;
                break;
            case WBISHOP_CHAR:
                *flag = BBISHOP;
                break;
            case WQUEEN_CHAR:
            default:
                *flag = BQUEEN;
                break;
            }
        } else *flag = BPAWN;
    } else {
        *flag = 1;
    }
    return (HMI_ENTER_PLY);
}

/*update the 50 moves counter*/
static void Play_Update_Special_Conditions(MOVE amove)
{
    int moving_piece = board[amove.m.from]->type;

    /* pawn move or capture?*/
    if ((moving_piece == WPAWN) ||
        (moving_piece == BPAWN) ||
        (board[amove.m.to]->type > 0))
    {
        fifty_moves = 0;
    } else
        fifty_moves++;
}

/* exclude all colour moves that would put colour into check*/
static int NEVER_INLINE Play_Find_Legal_Moves(MOVE *restrict movelist, int move_cnt, enum E_COLOUR colour)
{
    int i, actual_cnt;

    for (i = 0, actual_cnt = 0; i < move_cnt; i++)
    {
        Search_Try_Move(movelist[i]);
        if (Mvgen_King_In_Check(colour))
        {
            movelist[i].m.flag = 0;
            Search_Retract_Last_Move();
            continue;
        }
        Search_Retract_Last_Move();
        actual_cnt++;
    }
    return (actual_cnt);
}


/*for the HMI module, setting the check and mate flags in the pretty-print routine*/
enum E_HMI_CHECK NEVER_INLINE Play_Get_Status(enum E_COLOUR colour)
{
    if (Mvgen_King_In_Check(colour))
    {
        MOVE movelist[MAXMV];
        int move_cnt;

        move_cnt = Mvgen_Find_All_Moves(movelist, NO_LEVEL, colour, UNDERPROM);
        move_cnt = Play_Find_Legal_Moves(movelist, move_cnt, colour);
        if (move_cnt == 0)
            return (HMI_CHECK_STATUS_MATE);
        else
            return (HMI_CHECK_STATUS_CHECK);
    } else
        return(HMI_CHECK_STATUS_NONE);
}

static int NEVER_INLINE Play_Get_Mated_Or_Mat_Draw(enum E_COLOUR colour)
{
    int is_material_enough;
    unsigned dummy;

    (void) Eval_Static_Evaluation(&is_material_enough, colour, &dummy, &dummy, &dummy);

    if (!is_material_enough)
        return(1);
    else
    {
        MOVE movelist[MAXMV];
        int move_cnt, actual_cnt;

        /*check whether colour is checkmated or stalemate*/
        move_cnt = Mvgen_Find_All_Moves(movelist, NO_LEVEL, colour, UNDERPROM);
        actual_cnt = Play_Find_Legal_Moves(movelist, move_cnt, colour);
        if (actual_cnt == 0)
            return(1);
    }
    return(0);
}

static int NEVER_INLINE Play_Get_Repetition_Or_Fifty_Draw(void)
{
    if (Hash_Repetitions() >= 3)
        return(1);
    else if (fifty_moves >= 100)
        return(1);
    else if (mv_stack_p >= MAX_PLIES-1)
        return(1);

    return(0);
}

/*checks whether a move is in a move list.*/
int Play_Move_Is_Valid(MOVE key_move, const MOVE *restrict movelist, int move_cnt)
{
    int i;
    for (i = 0; i < move_cnt; i++)
        if (((movelist[i].u ^ key_move.u) & mv_move_mask.u) == 0)
            return(1);

    return(0);
}

/*play handling: for white, part 1. returns the following state.*/
static enum E_PST_STATE NEVER_INLINE
Play_Handling_White(int *restrict history_messed_up)
{
    enum E_COMP_RESULT comp_result;
    enum E_HMI_USER user_answer;
    int wn, from, to, fl;
    MOVE amove, xmoves[MAXMV];
    int is_material_enough;
    int32_t conf_time;
    unsigned dummy;

    amove.u = MV_NO_MOVE_MASK;

    /*the system time is a signed in32t, counting milliseconds. that's good for 24 days of operation.
    since the absolute time isn't relevant, but only the time each move takes, resetting the system
    time before each move gives 24 days per move.*/
    Hw_Set_System_Time(0);
    comp_result = COMP_MOVE_FOUND;
    (void) Eval_Static_Evaluation(&is_material_enough, WHITE, &dummy, &dummy, &dummy);
    if (!is_material_enough)
        comp_result = COMP_MAT_DRAW;
    else
    {
        int actual_cnt;
        /* Check if white is checkmated */
        wn = Mvgen_Find_All_Moves(xmoves, NO_LEVEL, WHITE, UNDERPROM);
        actual_cnt=Play_Find_Legal_Moves(xmoves, wn, WHITE);
        if (actual_cnt == 0) {
#ifdef PC_PRINTF
            Play_Show_Board();
#endif
            if (Mvgen_White_King_In_Check())
                comp_result = COMP_MATE;
            else
                comp_result = COMP_STALE;
        }
    }
    if (comp_result != COMP_MOVE_FOUND) /*game has actually ended*/
    {
        /*be sure to have the last ply in the pretty print format.*/
        Hmi_Prepare_Pretty_Print(black_started_game);
        if (computer_side == BLACK) /*computer made the last ply*/
        {
            Time_Set_Current(); /*for the display*/
            Hmi_Build_Game_Screen(computer_side, black_started_game, game_started_from_0, HMI_CONFIRM, &game_info);
        }
        user_answer = HMI_USER_CANCEL;
        switch (comp_result)
        {
        case COMP_MATE:
            user_answer = Hmi_Conf_Dialogue(" white is mated.", "0 : 1", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_POS_SEL, HMI_NO_RESTORE);
            break;
        case COMP_STALE:
            user_answer = Hmi_Conf_Dialogue("stalemate.", "1/2 : 1/2", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_POS_SEL, HMI_NO_RESTORE);
            break;
        case COMP_MAT_DRAW:
            user_answer = Hmi_Conf_Dialogue("material draw.", "1/2 : 1/2", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_POS_SEL, HMI_NO_RESTORE);
            break;
        default:
            break;
        }
        if (user_answer == HMI_USER_DISP)
        {
            if (computer_side == WHITE)
                (void) Hmi_Display_Current_Board(HMI_BLACK_BOTTOM, &conf_time, HMI_NO_RESTORE); /*let the user check the position.*/
            else
                (void) Hmi_Display_Current_Board(HMI_WHITE_BOTTOM, &conf_time, HMI_NO_RESTORE); /*let the user check the position.*/

            Hmi_Disp_Movelist(black_started_game, HMI_MENU_MODE_POS);
        }

        if (mv_stack_p < 2) /*nothing to take back. maybe a checkmate position was entered.*/
            user_answer = Hmi_Conf_Dialogue("no undo,", "start new game.", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
        else
            user_answer = Hmi_Conf_Dialogue("new game: OK", "    undo: CL", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
        if (user_answer == HMI_USER_CANCEL) /*the user wants to undo the last move*/
        {
            Hmi_Restore_Viewport();
            *history_messed_up = 1;
            if (computer_side == WHITE)
                /*that's a bit tricky. doing undo here means that the user as black has mated
                the computer and wants to undo his win. so we must undo only the last ply here and
                not the last move!*/
            {
                Play_Undo_Last_Ply();
                move_redo_stack.index = 0; /*that would be cumbersome with redo for only one ply*/
                /*different to normal undo: we jump to PlayBlack BEFORE the bonus is added, so we
                don't add the bonus here.*/
                return(PST_PLAY_BLACK);
            } else {
                Play_Undo_Last_Move();
                return(PST_PLAY_WHITE);
            }
        }
        return(PST_EXIT);
    }
    if ( computer_side == WHITE) {
        int32_t move_time, dummy_conf_time;
#ifdef DEBUG_STACK
        char stack_display[12];
#endif

#ifdef PC_PRINTF
#ifdef SHOW_BOARD
        printf("\n Board before Computer starts thinking ");
        Play_Show_Board();
#endif
#endif
        game_info.valid = EVAL_INVALID;
        game_info.eval = 0;
        Time_Give_Bonus(WHITE, WHITE);
        if ((CFG_GET_OPT(CFG_SPEAKER_MODE) == CFG_SPEAKER_ON) || (CFG_GET_OPT(CFG_SPEAKER_MODE) == CFG_SPEAKER_CLICK))
            Time_Delay(BEEP_CLICK + 10UL, SLEEP_ALLOWED); /*wait for the keyboard click to end*/

        if (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_MTI))
        {
            /*mate solver ramps up the speed itself and allow throttled speed from the start*/
            Search_Mate_Solver(black_started_game, WHITE);
            computer_side = NONE;
            return(PST_PLAY_WHITE);
        }

        /*ramp up the speed*/
        Hw_Set_Speed(SYSTEM_SPEED_HIGH, SYSTEM_MODE_COMP, CLK_FORCE_HIGH);
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
                Hw_Set_Speed(SYSTEM_SPEED_HIGH, SYSTEM_MODE_COMP, CLK_FORCE_HIGH); /*ramp up for the computer move*/
            }
        }

        if (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_ANA))
            Hmi_Build_Analysis_Screen(black_started_game);
        else
        {
            /*only for displaying the correct time*/
            Time_Set_Current(); /*for the display*/
            Hmi_Build_Game_Screen(computer_side, black_started_game, game_started_from_0, HMI_NO_CONFIRM, &game_info);
        }

        /*the screen buildup doesn't count for the time measurement*/
        move_time = Time_Init(mv_stack_p);
        comp_result = Search_Get_Best_Move(&amove, player_move, move_time, WHITE);
        Time_Set_Stop();

        /*ramp down the speed*/
        Hw_Set_Speed(SYSTEM_SPEED_LOW, SYSTEM_MODE_USER, CLK_FORCE_AUTO);
        Time_Enforce_Disp_Toggle(NO_DISP_TOGGLE);
        Hmi_Signal(HMI_MSG_MOVE);
        Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_MOVE, HW_MSG_PARAM_BACK_CONF);

#ifdef DEBUG_STACK
        Util_Strcpy(stack_display, "0x12345678.");
        Util_Long_Int_To_Hex( ((uint32_t) max_of_stack), stack_display+2);
        (void) Hmi_Conf_Dialogue("stack usage:", stack_display, &dummy_conf_time,
                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_RESTORE);
#endif

        if(game_info.valid == EVAL_MOVE)
            game_info.last_valid_eval = game_info.eval;
        switch (comp_result)
        {
        case COMP_MOVE_FOUND:     /*do nothing, computer found a move.*/
            break;
        case COMP_RESIGN:        /*computer wishes to resign, but still found a move.*/
            user_answer = Hmi_Conf_Dialogue("white resigns.", "0 : 1", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
            /*the conf_time does NOT add up here because we already stopped time counting some lines above.*/
            if (user_answer == HMI_USER_OK)
            {
                user_answer = Hmi_Conf_Dialogue("starting", "new game.", &conf_time,
                                                HMI_NO_TIMEOUT, HMI_POS_SEL, HMI_NO_RESTORE);

                if (user_answer == HMI_USER_DISP)
                {
                    if (computer_side == WHITE)
                        (void) Hmi_Display_Current_Board(HMI_BLACK_BOTTOM, &conf_time, HMI_NO_RESTORE); /*let the user check the position.*/
                    else
                        (void) Hmi_Display_Current_Board(HMI_WHITE_BOTTOM, &conf_time, HMI_NO_RESTORE); /*let the user check the position.*/

                    Hmi_Disp_Movelist(black_started_game, HMI_MENU_MODE_POS);
                }
                return(PST_EXIT);
            }
            Hmi_Restore_Viewport();
            /*cancel, the user wants to play to the very end.*/
            dynamic_resign_threshold = NO_RESIGN;
            break;

        /*the following cases cannot happen because this has already been checked some lines further up.*/
        case COMP_MATE:        /*computer says he went checkmate - no move found.*/
        case COMP_STALE:        /*computer says it is stalemate - no move found.*/
        case COMP_MAT_DRAW:        /*computer says it is draw due to insufficient materal - no move computed.*/
#ifdef PC_PRINTF
            printf("OOOOPS!!!");
#endif
            break;
        default:
            break;
        }

        Play_Update_Special_Conditions(amove);
        *history_messed_up = 0;
#ifdef PC_PRINTF
        printf("\n Computer decided to play: %s in %7.2lf secs",Play_Translate_Moves(amove), SECONDS_PASSED);
#endif
    } else {
        int move_valid;
        Time_Give_Bonus(WHITE, NONE);
        Time_Set_Current(); /*for the display*/
        Hmi_Build_Game_Screen(computer_side, black_started_game, game_started_from_0, HMI_NO_CONFIRM, &game_info);
        Time_Set_Start(mv_stack_p);
        do {
            enum E_HMI_INPUT comm;
#ifdef PC_PRINTF
            Play_Show_Board();
#endif
            comm = Play_Get_Player_Move(&from, &to, &fl);
            if (comm == HMI_NEW_GAME)
                return(PST_EXIT);
            if (comm == HMI_NEW_POS)
                /*a different position was entered or loaded.
                the computer either has the same side as before or the one loaded from the position,
                but that doesn't matter at this point. The question now is whose move it is. That depends on
                whether black has started (entered position) and whether the ply is odd/even.*/
            {
                if ((mv_stack_p & 1L) == 0) /*normally, that's white's move*/
                {
                    if (!black_started_game) /*except if black started the position, of course.*/
                        return(PST_PLAY_WHITE);
                    else
                        return(PST_PLAY_BLACK);
                } else
                {
                    if (!black_started_game)
                        return(PST_PLAY_BLACK);
                    else
                        return(PST_PLAY_WHITE);
                }
            }
            while ((comm==HMI_UNDO_MOVE)|| (comm==HMI_REDO_MOVE)) {
                if (comm==HMI_UNDO_MOVE)
                {
                    if (Play_Undo_Last_Move())
                    {
                        wn = Mvgen_Find_All_Moves(xmoves, NO_LEVEL, WHITE, UNDERPROM);
                        (void) Play_Find_Legal_Moves(xmoves, wn, WHITE);
                        Time_Give_Bonus(WHITE, NONE);
                        Time_Set_Start(mv_stack_p);
                        Hmi_Build_Game_Screen(computer_side, black_started_game, game_started_from_0, HMI_NO_CONFIRM, &game_info);
#ifdef PC_PRINTF
                        Play_Show_Board();
#endif
                        *history_messed_up = 1;
                    }
                }
                if (comm==HMI_REDO_MOVE)
                {
                    if (Play_Redo_Last_Move())
                    {
                        wn = Mvgen_Find_All_Moves(xmoves, NO_LEVEL, WHITE, UNDERPROM);
                        (void) Play_Find_Legal_Moves(xmoves, wn, WHITE);
                        Time_Give_Bonus(WHITE, NONE);
                        Time_Set_Start(mv_stack_p);
                        Hmi_Build_Game_Screen(computer_side, black_started_game, game_started_from_0, HMI_NO_CONFIRM, &game_info);
#ifdef PC_PRINTF
                        Play_Show_Board();
#endif
                        *history_messed_up = 1;
                        if (Play_Get_Mated_Or_Mat_Draw(WHITE)) /*redo hit the mate again*/
                            return(PST_PLAY_WHITE);
                        if (Play_Get_Repetition_Or_Fifty_Draw()) /*redo hit repetition or 50 moves draw*/
                            return(PST_POST_WHITE);
                    }
                }
                (void) Hw_Save_Game(HW_AUTO_SAVE);
                comm = Play_Get_Player_Move(&from, &to, &fl);
                if (comm == HMI_NEW_GAME)
                    return(PST_EXIT);
            }
            if (comm==HMI_COMP_GO) {
                Time_Clear_Conf_Delay();
                computer_side = WHITE;
                if (mv_stack_p == 0)
                    /*until right now, the computer had black, so white received a possible player bonus.
                    now that the computer takes the white side, that must be cancelled.*/
                    Time_Init_Game(computer_side, mv_stack_p);
                return(PST_PLAY_WHITE);
            }
            amove.m.flag    = fl;
            amove.m.from    = from;
            amove.m.to      = to;
            amove.m.mvv_lva = 0;

            move_valid = Play_Move_Is_Valid(amove, xmoves, wn);
            if (!move_valid)
                Hmi_Signal(HMI_MSG_ERROR);
            else
                Hmi_Signal(HMI_MSG_OK);
        } while (!move_valid);
        Time_Set_Stop();
        if (mv_stack_p == 0)
            /*white's first move does not count from the time.*/
            Time_Cancel_Used_Time();
        if (*history_messed_up == 0)
            player_move.u = amove.u;
        else
            player_move.u = MV_NO_MOVE_MASK;
        Play_Update_Special_Conditions(amove);
    }
    Time_Countdown(WHITE, mv_stack_p);
    if (Time_Control(WHITE) == TIME_WHITE_LOST)
    {
        if (Bpieces[0].next != NULL) /*if black has only the king, a timeout by white still is draw.*/
            (void) Hmi_Conf_Dialogue("time: white lost.", "0 : 1", &conf_time,
                                     HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
        else
            (void) Hmi_Conf_Dialogue("time: draw.", "1/2 : 1/2", &conf_time,
                                     HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
        user_answer = Hmi_Conf_Dialogue("    new game: OK", "cont. in TPM: CL", &conf_time,
                                        HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
        if (user_answer == HMI_USER_CANCEL) /*the user wants to continue the game in time per move mode*/
        {
            Hmi_Restore_Viewport();
            Time_Change_To_TPM_After_Loss();
        } else
        {
            /*new viewport will be opened in main() anyway, so don't restore.*/
            return(PST_EXIT);
        }
    }
    Search_Push_Status();
    Search_Make_Move(amove);
    move_redo_stack.index = 0; /*regular moves delete the redo possibility*/
#ifdef DBGMATERIAL
    printf("\nMaterial=%lf\n",move_stack[mv_stack_p].material/100.0);
#endif

    return(PST_POST_WHITE);
}

/*play handling: for white, part 2. returns the following state.*/
static enum E_PST_STATE NEVER_INLINE
Play_Handling_White_Post(int *restrict history_messed_up, int conf_computer_side)
{
    int32_t conf_time;
    enum E_COMP_RESULT comp_result = COMP_MOVE_FOUND;

    if (Hash_Repetitions() >= 3)
        comp_result = COMP_THREE_REP;
    else if ((fifty_moves>=100) && (Play_Get_Status(BLACK) != HMI_CHECK_STATUS_MATE)) {
        /*checkmate takes priority over a 50 moves draw. at this point, the
        current white move has been played on the board, so Black could be
        checkmated. Will be processed in the play handling for the next
        black move, just like any other checkmate.
        otherwise, i.e. no checkmate, it's a 50 moves draw here.*/
        comp_result = COMP_FIFTY_MOVES;
    } else if (mv_stack_p >= MAX_PLIES-1)
        comp_result = COMP_STACK_FULL;

    if (comp_result != COMP_MOVE_FOUND)
    {
        enum E_HMI_USER user_answer;

        /*be sure to have the last ply in the pretty print format.*/
        Hmi_Prepare_Pretty_Print(black_started_game);

        if (computer_side == WHITE) /*computer made the last ply*/
        {
            Time_Set_Current(); /*for the display*/
            Hmi_Build_Game_Screen(computer_side, black_started_game, game_started_from_0, HMI_CONFIRM, &game_info);
        }

        user_answer = HMI_USER_CANCEL;
        switch (comp_result)
        {
        case COMP_THREE_REP:
            user_answer = Hmi_Conf_Dialogue("3 repetitions.", "1/2 : 1/2", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_POS_SEL, HMI_NO_RESTORE);
            break;
        case COMP_FIFTY_MOVES:
            user_answer = Hmi_Conf_Dialogue("50 moves rule.", "1/2 : 1/2", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_POS_SEL, HMI_NO_RESTORE);
            break;
        case COMP_STACK_FULL:
                user_answer = Hmi_Conf_Dialogue(" only 250 moves.", "1/2 : 1/2", &conf_time,
                                                HMI_NO_TIMEOUT, HMI_POS_SEL, HMI_NO_RESTORE);
            break;
        default:
            break;
        }
        if (user_answer == HMI_USER_DISP)
        {
            if (computer_side == BLACK)
                (void) Hmi_Display_Current_Board(HMI_WHITE_BOTTOM, &conf_time, HMI_NO_RESTORE); /*let the user check the position.*/
            else
                (void) Hmi_Display_Current_Board(HMI_BLACK_BOTTOM, &conf_time, HMI_NO_RESTORE); /*let the user check the position.*/

            Hmi_Disp_Movelist(black_started_game, HMI_MENU_MODE_POS);
        }

        if (mv_stack_p < 2) /*nothing to take back. maybe a position was entered.*/
            user_answer = Hmi_Conf_Dialogue("no undo,", "start new game.", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
        else
            user_answer = Hmi_Conf_Dialogue("new game: OK", "    undo: CL", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
        if (user_answer == HMI_USER_CANCEL) /*the user wants to undo the last move*/
        {
            Hmi_Restore_Viewport();
            *history_messed_up = 1;
            if (computer_side == WHITE)
            {
                Play_Undo_Last_Move();
                /*different to normal undo: we jump to PlayBlack BEFORE the bonus is added, so we
                don_t add the bonus here.*/
                return(PST_PLAY_BLACK);
            } else
            {
                Play_Undo_Last_Ply();
                move_redo_stack.index = 0; /*that would be cumbersome with redo for only one ply*/
                return(PST_PLAY_WHITE);
            }
        }
        return(PST_EXIT);
    }
    /*this autosave is special: if the current game has started from the initial position
    and the computer plays white with the first move, then the autosave must be avoided.
    Suppose that the config is "random start" and the last game was interrupted due to
    power failure, then the user can access the menu for loading the saved game only upon
    black's first move. But by then, the autosave here would have overwritten that game
    already.
    However, if the game has started with an entered position, then the user must have
    accessed the menu for entering a position, so he obviously didn't want to continue
    the autosaved game.*/
    if ((conf_computer_side != WHITE) || (mv_stack_p > 1) || (game_started_from_0 == 0))
        (void) Hw_Save_Game(HW_AUTO_SAVE);

    return(PST_PLAY_BLACK);
}

/*play handling: for black, part 1. returns the following state.*/
static enum E_PST_STATE NEVER_INLINE
Play_Handling_Black(int *restrict history_messed_up)
{
    enum E_COMP_RESULT comp_result;
    enum E_HMI_USER user_answer;
    int bn, from, to, fl;
    MOVE amove, xmoves[MAXMV];
    int is_material_enough;
    int32_t conf_time;
    unsigned dummy;

    amove.u = MV_NO_MOVE_MASK;

    Hw_Set_System_Time(0);
    comp_result = COMP_MOVE_FOUND;
    (void) Eval_Static_Evaluation(&is_material_enough, BLACK, &dummy, &dummy, &dummy);
    if (!is_material_enough)
        comp_result = COMP_MAT_DRAW;
    else
    {
        int actual_cnt;
        /* Check if black is checkmated */
        bn = Mvgen_Find_All_Moves(xmoves, NO_LEVEL, BLACK, UNDERPROM);
        actual_cnt = Play_Find_Legal_Moves(xmoves, bn, BLACK);
        if (actual_cnt == 0) {
#ifdef PC_PRINTF
            Play_Show_Board();
#endif
            if (Mvgen_Black_King_In_Check())
                comp_result = COMP_MATE;
            else
                comp_result = COMP_STALE;
        }
    }

    if (comp_result != COMP_MOVE_FOUND) /*game has actually ended*/
    {
        /*be sure to have the last ply in the pretty print format.*/
        Hmi_Prepare_Pretty_Print(black_started_game);

        if (computer_side == WHITE) /*computer made the last ply*/
        {
            Time_Set_Current(); /*for the display*/
            Hmi_Build_Game_Screen(computer_side, black_started_game, game_started_from_0, HMI_CONFIRM, &game_info);
        }

        user_answer = HMI_USER_CANCEL;
        switch (comp_result)
        {
        case COMP_MATE:
            user_answer = Hmi_Conf_Dialogue(" black is mated.", "1 : 0", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_POS_SEL, HMI_NO_RESTORE);
            break;
        case COMP_STALE:
            user_answer = Hmi_Conf_Dialogue("stalemate.", "1/2 : 1/2", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_POS_SEL, HMI_NO_RESTORE);
            break;
        case COMP_MAT_DRAW:
            user_answer = Hmi_Conf_Dialogue("material draw.", "1/2 : 1/2", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_POS_SEL, HMI_NO_RESTORE);
            break;
        default:
            break;
        }
        if (user_answer == HMI_USER_DISP)
        {
            if (computer_side == BLACK)
                (void) Hmi_Display_Current_Board(HMI_WHITE_BOTTOM, &conf_time, HMI_NO_RESTORE); /*let the user check the position.*/
            else
                (void) Hmi_Display_Current_Board(HMI_BLACK_BOTTOM, &conf_time, HMI_NO_RESTORE); /*let the user check the position.*/

            Hmi_Disp_Movelist(black_started_game, HMI_MENU_MODE_POS);
        }

        if (mv_stack_p < 2) /*nothing to take back. maybe a checkmate position was entered.*/
            user_answer = Hmi_Conf_Dialogue("no undo,", "start new game.", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
        else
            user_answer = Hmi_Conf_Dialogue("new game: OK", "    undo: CL", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
        if (user_answer == HMI_USER_CANCEL) /*the user wants to undo the last move*/
        {
            Hmi_Restore_Viewport();
            *history_messed_up = 1;
            if (computer_side == BLACK)
                /*that's a bit tricky. doing undo here means that the user as white has mated
                the computer and wants to undo his win. so we must undo only the last ply here and
                not the last move!*/
            {
                Play_Undo_Last_Ply();
                move_redo_stack.index = 0; /*that would be cumbersome with redo for only one ply*/
                /*different to normal undo: we jump to PlayWhite BEFORE the bonus is added, so we
                don't add the bonus here.*/
                return(PST_PLAY_WHITE);
            } else {
                Play_Undo_Last_Move();
                return(PST_PLAY_BLACK);
            }
        }
        return(PST_EXIT);
    }

    if (computer_side == BLACK) {
        int32_t move_time, dummy_conf_time;
#ifdef DEBUG_STACK
        char stack_display[12];
#endif

#ifdef PC_PRINTF
#ifdef SHOW_BOARD
        printf("\n Board before Computer starts thinking ");
        Play_Show_Board();
#endif
#endif
        game_info.valid = EVAL_INVALID;
        game_info.eval = 0;
        Time_Give_Bonus(BLACK, BLACK);
        if ((CFG_GET_OPT(CFG_SPEAKER_MODE) == CFG_SPEAKER_ON) || (CFG_GET_OPT(CFG_SPEAKER_MODE) == CFG_SPEAKER_CLICK))
            Time_Delay(BEEP_CLICK + 10UL, SLEEP_ALLOWED); /*wait for the keyboard click to end*/

        if (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_MTI))
        {
            /*mate solver ramps up the speed itself and allow throttled speed from the start*/
            Search_Mate_Solver(black_started_game, BLACK);
            computer_side = NONE;
            return(PST_PLAY_BLACK);
        }

       /*ramp up the speed*/
        Hw_Set_Speed(SYSTEM_SPEED_HIGH, SYSTEM_MODE_COMP, CLK_FORCE_HIGH);
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
                Hw_Set_Speed(SYSTEM_SPEED_HIGH, SYSTEM_MODE_COMP, CLK_FORCE_HIGH); /*ramp up for the computer move*/
            }
        }
        if (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_ANA))
            Hmi_Build_Analysis_Screen(black_started_game);
        else
        {
            /*only for displaying the correct time*/
            Time_Set_Current(); /*for the display*/
            Hmi_Build_Game_Screen(computer_side, black_started_game, game_started_from_0, HMI_NO_CONFIRM, &game_info);
        }

        /*the screen buildup doesn't count for the time measurement*/
        move_time = Time_Init(mv_stack_p);
        comp_result = Search_Get_Best_Move(&amove, player_move, move_time, BLACK);
        Time_Set_Stop();

        /*ramp down the speed*/
        Hw_Set_Speed(SYSTEM_SPEED_LOW, SYSTEM_MODE_USER, CLK_FORCE_AUTO);
        Time_Enforce_Disp_Toggle(NO_DISP_TOGGLE);
        Hmi_Signal(HMI_MSG_MOVE);
        Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_MOVE, HW_MSG_PARAM_BACK_CONF);
#ifdef DEBUG_STACK
        Util_Strcpy(stack_display, "0x12345678.");
        Util_Long_Int_To_Hex( ((uint32_t) max_of_stack), stack_display+2);
        (void) Hmi_Conf_Dialogue("stack usage:", stack_display, &dummy_conf_time,
                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_RESTORE);
#endif

        if(game_info.valid == EVAL_MOVE)
            game_info.last_valid_eval = game_info.eval;
        switch (comp_result)
        {
        case COMP_MOVE_FOUND:     /*do nothing, computer found a move.*/
            break;
        case COMP_RESIGN:        /*computer wishes to resign, but still found a move.*/
            user_answer = Hmi_Conf_Dialogue("black resigns.", "1 : 0", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
            /*the conf_time does NOT add up here because we already stopped time counting some lines above.*/
            if (user_answer == HMI_USER_OK)
            {
                user_answer = Hmi_Conf_Dialogue("starting", "new game.", &conf_time,
                                                HMI_NO_TIMEOUT, HMI_POS_SEL, HMI_NO_RESTORE);
                if (user_answer == HMI_USER_DISP)
                {
                    if (computer_side == BLACK)
                        (void) Hmi_Display_Current_Board(HMI_WHITE_BOTTOM, &conf_time, HMI_NO_RESTORE); /*let the user check the position.*/
                    else
                        (void) Hmi_Display_Current_Board(HMI_BLACK_BOTTOM, &conf_time, HMI_NO_RESTORE); /*let the user check the position.*/

                    Hmi_Disp_Movelist(black_started_game, HMI_MENU_MODE_POS);
                }
                return(PST_EXIT);
            }
            Hmi_Restore_Viewport();
            /*cancel, the user wants to play to the very end.*/
            dynamic_resign_threshold = NO_RESIGN;
            break;

        /*the following cases cannot happen because this has already been checked some lines further up.*/
        case COMP_MATE:        /*computer says he went checkmate - no move found.*/
        case COMP_STALE:        /*computer says it is stalemate - no move found.*/
        case COMP_MAT_DRAW:        /*computer says it is draw due to insufficient materal - no move computed.*/
#ifdef PC_PRINTF
            printf("OOOOPS!!!");
#endif
            break;
        default:
            break;
        }
        Play_Update_Special_Conditions(amove);
        *history_messed_up = 0;
#ifdef PC_PRINTF
        printf("\n Computer decided to play : %s in %7.2lf secs",Play_Translate_Moves(amove), SECONDS_PASSED);
#endif
    } else {
        int move_valid;
        Time_Give_Bonus(BLACK, NONE);
        Time_Set_Current(); /*for the display*/
        Hmi_Build_Game_Screen(computer_side, black_started_game, game_started_from_0, HMI_NO_CONFIRM, &game_info);
        Time_Set_Start(mv_stack_p);
        do {
            enum E_HMI_INPUT comm;
#ifdef PC_PRINTF
            Play_Show_Board();
#endif
            comm = Play_Get_Player_Move(&from, &to,&fl);
            if (comm == HMI_NEW_GAME)
                return(PST_EXIT);
            if (comm == HMI_NEW_POS)
                /*a different position was entered or loaded.
                the computer either has the same side as before or the one loaded from the position,
                but that doesn't matter at this point. The question now is whose move it is. That depends on
                whether black has started (entered position) and whether the ply is odd/even.*/
            {
                if ((mv_stack_p & 1L) == 0) /*normally, that's white's move*/
                {
                    if (!black_started_game) /*except if black started the position, of course.*/
                        return(PST_PLAY_WHITE);
                    else
                        return(PST_PLAY_BLACK);
                } else
                {
                    if (!black_started_game)
                        return(PST_PLAY_BLACK);
                    else
                        return(PST_PLAY_WHITE);
                }
            }
            while ((comm==HMI_UNDO_MOVE)|| (comm==HMI_REDO_MOVE)) {
                if (comm==HMI_UNDO_MOVE)
                {
                    if (Play_Undo_Last_Move())
                    {
                        bn = Mvgen_Find_All_Moves(xmoves, NO_LEVEL, BLACK, UNDERPROM);
                        (void) Play_Find_Legal_Moves(xmoves, bn, BLACK);
                        Time_Give_Bonus(BLACK, NONE);
                        Time_Set_Start(mv_stack_p);
                        Hmi_Build_Game_Screen(computer_side, black_started_game, game_started_from_0, HMI_NO_CONFIRM, &game_info);
#ifdef PC_PRINTF
                        Play_Show_Board();
#endif
                        *history_messed_up = 1;
                    }
                }
                if (comm==HMI_REDO_MOVE)
                {
                    if (Play_Redo_Last_Move())
                    {
                        bn = Mvgen_Find_All_Moves(xmoves, NO_LEVEL, BLACK, UNDERPROM);
                        (void) Play_Find_Legal_Moves(xmoves, bn, BLACK);
                        Time_Give_Bonus(BLACK, NONE);
                        Time_Set_Start(mv_stack_p);
                        Hmi_Build_Game_Screen(computer_side, black_started_game, game_started_from_0, HMI_NO_CONFIRM, &game_info);
#ifdef PC_PRINTF
                        Play_Show_Board();
#endif
                        *history_messed_up = 1;
                        if (Play_Get_Mated_Or_Mat_Draw(BLACK)) /*redo hit the mate again*/
                            return(PST_PLAY_BLACK);
                        if (Play_Get_Repetition_Or_Fifty_Draw()) /*redo hit repetition or 50 moves draw*/
                            return(PST_POST_BLACK);
                    }
                }
                (void) Hw_Save_Game(HW_AUTO_SAVE);
                comm = Play_Get_Player_Move(&from, &to, &fl);
                if (comm == HMI_NEW_GAME)
                    return(PST_EXIT);
            }
            if (comm==HMI_COMP_GO) {
                Time_Clear_Conf_Delay();
                computer_side = BLACK;
                if (mv_stack_p == 1)
                    /*until right now, the computer had white, so white did not receive a possible player bonus.
                    on the other hand, black did get that bonus. that must be cancelled.*/
                {
                    Time_Init_Game(computer_side, mv_stack_p);
                    Time_Give_Bonus(WHITE, computer_side);
                }
                return(PST_PLAY_BLACK);
            }
            amove.m.flag    = fl;
            amove.m.from    = from;
            amove.m.to      = to;
            amove.m.mvv_lva = 0;

            move_valid = Play_Move_Is_Valid(amove, xmoves, bn);
            if (!move_valid)
                Hmi_Signal(HMI_MSG_ERROR);
            else
                Hmi_Signal(HMI_MSG_OK);
        } while (!move_valid);
        Time_Set_Stop();
        if (*history_messed_up == 0)
            player_move.u = amove.u;
        else
            player_move.u = MV_NO_MOVE_MASK;
        Play_Update_Special_Conditions(amove);
    }
    Time_Countdown(BLACK, mv_stack_p);
    if (Time_Control(BLACK) == TIME_BLACK_LOST)
    {
        if (Wpieces[0].next != NULL) /*if white has only the king, a timeout by black still is draw.*/
            (void) Hmi_Conf_Dialogue("time: black lost.", "1 : 0", &conf_time,
                                     HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
        else
            (void) Hmi_Conf_Dialogue("time: draw.", "1/2 : 1/2", &conf_time,
                                     HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
        user_answer = Hmi_Conf_Dialogue("    new game: OK", "cont. in TPM: CL", &conf_time,
                                        HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
        if (user_answer == HMI_USER_CANCEL) /*the user wants to continue the game in time per move mode*/
        {
            Hmi_Restore_Viewport();
            Time_Change_To_TPM_After_Loss();
        } else
        {
            /*new viewport will be opened in main() anyway, so don't restore.*/
            return(PST_EXIT);
        }
    }
    Search_Push_Status();
    Search_Make_Move(amove);
    move_redo_stack.index = 0; /*regular moves delete the redo possibility*/
#ifdef DBGMATERIAL
    printf("\nMaterial=%lf\n",move_stack[mv_stack_p].material/100.0);
#endif

    return(PST_POST_BLACK);
}

/*play handling: for black, part 2. returns the following state.*/
static enum E_PST_STATE NEVER_INLINE
Play_Handling_Black_Post(int *restrict history_messed_up)
{
    int32_t conf_time;
    enum E_COMP_RESULT comp_result = COMP_MOVE_FOUND;

    if (Hash_Repetitions() >= 3)
        comp_result = COMP_THREE_REP;
    else if ((fifty_moves>=100) && (Play_Get_Status(WHITE) != HMI_CHECK_STATUS_MATE)) {
        /*checkmate takes priority over a 50 moves draw. at this point, the
        current black move has been played on the board, so White could be
        checkmated. Will be processed in the play handling for the next
        white move, just like any other checkmate.
        otherwise, i.e. no checkmate, it's a 50 moves draw here.*/
        comp_result = COMP_FIFTY_MOVES;
    } else if (mv_stack_p >= MAX_PLIES-1)
        comp_result = COMP_STACK_FULL;

    if (comp_result != COMP_MOVE_FOUND)
    {
        enum E_HMI_USER user_answer;

        /*be sure to have the last ply in the pretty print format.*/
        Hmi_Prepare_Pretty_Print(black_started_game);

        if (computer_side == BLACK) /*computer made the last ply*/
        {
            Time_Set_Current(); /*for the display*/
            Hmi_Build_Game_Screen(computer_side, black_started_game, game_started_from_0, HMI_CONFIRM, &game_info);
        }

        user_answer = HMI_USER_CANCEL;
        switch (comp_result)
        {
        case COMP_THREE_REP:
            user_answer = Hmi_Conf_Dialogue("3 repetitions.", "1/2 : 1/2", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_POS_SEL, HMI_NO_RESTORE);
            break;
        case COMP_FIFTY_MOVES:
            user_answer = Hmi_Conf_Dialogue("50 moves rule.", "1/2 : 1/2", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_POS_SEL, HMI_NO_RESTORE);
            break;
        case COMP_STACK_FULL:
                user_answer = Hmi_Conf_Dialogue(" only 250 moves.", "1/2 : 1/2", &conf_time,
                                                HMI_NO_TIMEOUT, HMI_POS_SEL, HMI_NO_RESTORE);
            break;
        default:
            break;
        }
        if (user_answer == HMI_USER_DISP)
        {
            if (computer_side == WHITE)
                (void) Hmi_Display_Current_Board(HMI_BLACK_BOTTOM, &conf_time, HMI_NO_RESTORE); /*let the user check the position.*/
            else
                (void) Hmi_Display_Current_Board(HMI_WHITE_BOTTOM, &conf_time, HMI_NO_RESTORE); /*let the user check the position.*/

            Hmi_Disp_Movelist(black_started_game, HMI_MENU_MODE_POS);
        }

        if (mv_stack_p < 2) /*nothing to take back. maybe a position was entered.*/
            user_answer = Hmi_Conf_Dialogue("no undo,", "start new game.", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
        else
            user_answer = Hmi_Conf_Dialogue("new game: OK", "    undo: CL", &conf_time,
                                            HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
        if (user_answer == HMI_USER_CANCEL) /*the user wants to undo the last move*/
        {
            Hmi_Restore_Viewport();
            *history_messed_up = 1;
            if (computer_side == BLACK)
            {
                Play_Undo_Last_Move();
                /*different to normal undo: we jump to PlayWhite BEFORE the bonus is added, so we
                don't add the bonus here.*/
                return(PST_PLAY_WHITE);
            } else
            {
                Play_Undo_Last_Ply();
                move_redo_stack.index = 0; /*that would be cumbersome with redo for only one ply*/
                return(PST_PLAY_BLACK);
            }
        }
        return(PST_EXIT);
    }
    (void) Hw_Save_Game(HW_AUTO_SAVE);

    return(PST_NEW_WH_MOVE);
}

/*the state machine for the main play handling.*/
static void NEVER_INLINE Play_Handling(void)
{
    int conf_computer_side, history_messed_up;
    enum E_PST_STATE play_state = PST_NEW_WH_MOVE;

    conf_computer_side = computer_side;

    while (play_state != PST_EXIT)
    {
        if (play_state == PST_NEW_WH_MOVE)
        {
            /*no undo/redo operations that would have ruined the history;
            this means that taking advantage of PV hits is possible now.*/
            history_messed_up = 0;
            play_state = PST_PLAY_WHITE;
        }

        if (play_state == PST_PLAY_WHITE)
            play_state = Play_Handling_White(&history_messed_up);

        /*the "after white move" handling is special in that it needs the original
        colour configuration. the point here is to disable the autosave after the first
        white ply if the computer has started the game automatically, i.e. either by
        being configured to take white or with random colours and getting white.
        otherwise, that specific autosave would overwrite an existing autosave from
        a previous power cut.*/
        if (play_state == PST_POST_WHITE)
            play_state = Play_Handling_White_Post(&history_messed_up, conf_computer_side);

        if (play_state == PST_PLAY_BLACK)
            play_state = Play_Handling_Black(&history_messed_up);

        if (play_state == PST_POST_BLACK)
            play_state = Play_Handling_Black_Post(&history_messed_up);
    }
}

void Play_Save_Status(BACKUP_GAME *ptr)
{
    int i;

    /*straight-forward copy*/
    if (black_started_game)
        ptr->blackstart_searchdisp |= SAVE_BLACK_START; /*zero init in Hw_Save_Game()*/
    ptr->game_started_from_0 = game_started_from_0;
    ptr->dynamic_resign_threshold = dynamic_resign_threshold;
    ptr->computer_side = (uint8_t) computer_side;
    ptr->player_move.u = player_move.u;
    Util_Memcpy(&(ptr->starting_pos), &(starting_pos), sizeof(STARTING_POS));
    Util_Memcpy(&(ptr->game_info), &(game_info), sizeof(GAME_INFO));
    Util_Memcpy(&(ptr->GlobalPV), &(GlobalPV), sizeof(LINE));
    Util_Memcpy(&(ptr->move_redo_stack), &(move_redo_stack), sizeof(MOVE_REDO_STACK));

    ptr->mv_stack_p = mv_stack_p;

    /*just save all moves in the move stack, compressed*/
    for (i = 0; i <= MAX_PLIES; i++)
        ptr->movelist[i] = Mvgen_Compress_Move(move_stack[i].move);
}

static void Play_Convert_Starting_Pos(const STARTING_POS *load_starting_pos, int black_starts)
{
    int i, j, wcnt, bcnt, xy;
    PIECE *p;

    Play_Empty_Board();

    /*first pass: scan through all pawns*/
    for (i = 0, wcnt = 8, bcnt = 8; i < 8; i++) /*the file*/
    {
        for (j = 8; j < 56; j += 8) /*move up the file*/
        {
            if (load_starting_pos->board[i+j] == WPAWN)
            {
                xy = board64[i+j];
                Wpieces[wcnt].xy = xy;
                board[xy] = &Wpieces[wcnt];
                wcnt++;
            } else if (load_starting_pos->board[i+j] == BPAWN)
            {
                xy = board64[i+j];
                Bpieces[bcnt].xy = xy;
                board[xy] = &Bpieces[bcnt];
                bcnt++;
            }
        }
    }
    /*second pass: rooks*/
    for (i = 0, wcnt = 2, bcnt = 2; i < 64; i++)
    {
        if (load_starting_pos->board[i] == WROOK)
        {
            if ((wcnt == 2) || (wcnt == 3))
            {
                xy = board64[i];
                Wpieces[wcnt].xy = xy;
                board[xy] = &Wpieces[wcnt];
                wcnt++;
            } else /*extra rooks from promotion. find the first empty pawn.*/
            {
                for (j = 8; j < 16; j++)
                    if (Wpieces[j].xy == 0)
                        break;
                if (j < 16) /*else, the input position is faulty anyway.*/
                {
                    wcnt = j;
                    Wpieces[wcnt].type = WROOK;
                    xy = board64[i];
                    Wpieces[wcnt].xy = xy;
                    board[xy] = &Wpieces[wcnt];
                }
            }
        } else if (load_starting_pos->board[i] == BROOK)
        {
            if ((bcnt == 2) || (bcnt == 3))
            {
                xy = board64[i];
                Bpieces[bcnt].xy = xy;
                board[xy] = &Bpieces[bcnt];
                bcnt++;
            } else /*extra rooks from promotion. find the first empty pawn.*/
            {
                for (j = 8; j < 16; j++)
                    if (Bpieces[j].xy == 0)
                        break;
                if (j < 16) /*else, the input position is faulty anyway.*/
                {
                    bcnt = j;
                    Bpieces[bcnt].type = BROOK;
                    xy = board64[i];
                    Bpieces[bcnt].xy = xy;
                    board[xy] = &Bpieces[bcnt];
                }
            }
        }
    }
    /*third pass: bishops*/
    for (i = 0, wcnt = 4, bcnt = 4; i < 64; i++)
    {
        if (load_starting_pos->board[i] == WBISHOP)
        {
            if ((wcnt == 4) || (wcnt == 5))
            {
                xy = board64[i];
                Wpieces[wcnt].xy = xy;
                board[xy] = &Wpieces[wcnt];
                wcnt++;
            } else /*extra bishops from promotion. find the first empty pawn.*/
            {
                for (j = 8; j < 16; j++)
                    if (Wpieces[j].xy == 0)
                        break;
                if (j < 16) /*else, the input position is faulty anyway.*/
                {
                    wcnt = j;
                    Wpieces[wcnt].type = WBISHOP;
                    xy = board64[i];
                    Wpieces[wcnt].xy = xy;
                    board[xy] = &Wpieces[wcnt];
                }
            }
        } else if (load_starting_pos->board[i] == BBISHOP)
        {
            if ((bcnt == 4) || (bcnt == 5))
            {
                xy = board64[i];
                Bpieces[bcnt].xy = xy;
                board[xy] = &Bpieces[bcnt];
                bcnt++;
            } else /*extra bishops from promotion. find the first empty pawn.*/
            {
                for (j = 8; j < 16; j++)
                    if (Bpieces[j].xy == 0)
                        break;
                if (j < 16) /*else, the input position is faulty anyway.*/
                {
                    bcnt = j;
                    Bpieces[bcnt].type = BBISHOP;
                    xy = board64[i];
                    Bpieces[bcnt].xy = xy;
                    board[xy] = &Bpieces[bcnt];
                }
            }
        }
    }
    /*fourth pass: knights*/
    for (i = 0, wcnt = 6, bcnt = 6; i < 64; i++)
    {
        if (load_starting_pos->board[i] == WKNIGHT)
        {
            if ((wcnt == 6) || (wcnt == 7))
            {
                xy = board64[i];
                Wpieces[wcnt].xy = xy;
                board[xy] = &Wpieces[wcnt];
                wcnt++;
            } else /*extra knights from promotion. find the first empty pawn.*/
            {
                for (j = 8; j < 16; j++)
                    if (Wpieces[j].xy == 0)
                        break;
                if (j < 16) /*else, the input position is faulty anyway.*/
                {
                    wcnt = j;
                    Wpieces[wcnt].type = WKNIGHT;
                    xy = board64[i];
                    Wpieces[wcnt].xy = xy;
                    board[xy] = &Wpieces[wcnt];
                }
            }
        } else if (load_starting_pos->board[i] == BKNIGHT)
        {
            if ((bcnt == 6) || (bcnt == 7))
            {
                xy = board64[i];
                Bpieces[bcnt].xy = xy;
                board[xy] = &Bpieces[bcnt];
                bcnt++;
            } else /*extra knights from promotion. find the first empty pawn.*/
            {
                for (j = 8; j < 16; j++)
                    if (Bpieces[j].xy == 0)
                        break;
                if (j < 16) /*else, the input position is faulty anyway.*/
                {
                    bcnt = j;
                    Bpieces[bcnt].type = BKNIGHT;
                    xy = board64[i];
                    Bpieces[bcnt].xy = xy;
                    board[xy] = &Bpieces[bcnt];
                }
            }
        }
    }
    /*fifth pass: queens*/
    for (i = 0, wcnt = 1, bcnt = 1; i < 64; i++)
    {
        if (load_starting_pos->board[i] == WQUEEN)
        {
            if (wcnt == 1)
            {
                xy = board64[i];
                Wpieces[wcnt].xy = xy;
                board[xy] = &Wpieces[wcnt];
                wcnt++;
            } else /*extra queens from promotion. find the first empty pawn.*/
            {
                for (j = 8; j < 16; j++)
                    if (Wpieces[j].xy == 0)
                        break;
                if (j < 16) /*else, the input position is faulty anyway.*/
                {
                    wcnt = j;
                    Wpieces[wcnt].type = WQUEEN;
                    xy = board64[i];
                    Wpieces[wcnt].xy = xy;
                    board[xy] = &Wpieces[wcnt];
                }
            }
        } else if (load_starting_pos->board[i] == BQUEEN)
        {
            if (bcnt == 1)
            {
                xy = board64[i];
                Bpieces[bcnt].xy = xy;
                board[xy] = &Bpieces[bcnt];
                bcnt++;
            } else /*extra queens from promotion. find the first empty pawn.*/
            {
                for (j = 8; j < 16; j++)
                    if (Bpieces[j].xy == 0)
                        break;
                if (j < 16) /*else, the input position is faulty anyway.*/
                {
                    bcnt = j;
                    Bpieces[bcnt].type = BQUEEN;
                    xy = board64[i];
                    Bpieces[bcnt].xy = xy;
                    board[xy] = &Bpieces[bcnt];
                }
            }
        }
    }
    /*sixth pass: kings*/
    for (i = 0; i < 64; i++)
    {
        if (load_starting_pos->board[i] == WKING)
        {
            xy = board64[i];
            Wpieces[0].xy = xy;
            board[xy] = &Wpieces[0];
            wking = xy;
        } else if (load_starting_pos->board[i] == BKING)
        {
            xy = board64[i];
            Bpieces[0].xy = xy;
            board[xy] = &Bpieces[0];
            bking = xy;
        }
    }

    /* Fix non board piece pointers */
    for (p=Wpieces[0].next; p!=NULL; p=p->next) {
        if (p->xy == 0) {
            p->prev->next = p->next;
            if (p->next)
                p->next->prev = p->prev;
        }
    }
    for (p=Bpieces[0].next; p!=NULL; p=p->next) {
        if (p->xy == 0) {
            p->prev->next = p->next;
            if (p->next)
                p->next->prev = p->prev;
        }
    }
    gflags = (load_starting_pos->gflags & CASTL_FLAGS);

    /* if white starts, then the (imaginary) move in move_stack[0] would have been a black move.*/
    if (!black_starts)
        gflags |= BLACK_MOVED;

    /*if we have an en passant square AND if that square on the board
    is free, take the en passant square - else ignore it.*/
    if ((load_starting_pos->epsquare < BP_NOSQUARE) &&
        (load_starting_pos->board[load_starting_pos->epsquare] == 0))
        en_passant_sq = board64[load_starting_pos->epsquare];
    else
        en_passant_sq = 0;
}

static enum E_POS_STATE NEVER_INLINE Play_King_Positions_Ok(int pos_wking, int pos_bking)
{
    int row_diff, col_diff;

    col_diff = (pos_wking % 8) - (pos_bking % 8);
    if (col_diff < 0) col_diff = -col_diff;
    row_diff = (pos_wking / 8) - (pos_bking / 8);
    if (row_diff < 0) row_diff = -row_diff;

    if ((row_diff > 1) ||(col_diff > 1))
        return(POS_OK);

    return(POS_KING_INVALID);
}

enum E_POS_STATE NEVER_INLINE Play_Load_Position(const STARTING_POS *load_starting_pos, int black_starts, int pos_wking, int pos_bking)
{
    int i, wh_pieces, bl_pieces, dummy;
    MOVE movelist[400]; /*biiiig.. so that during the check, no overflow can happen.
                        this is on the stack and outside the search, so space isn't a problem here.*/
    int move_cnt_w, move_cnt_b;


    for (i = 0, wh_pieces = 0, bl_pieces = 0; i < 64; i++)
    {
        if ((load_starting_pos->board[i] >= WPAWN) && (load_starting_pos->board[i] <= WKING))
            wh_pieces++;
        if ((load_starting_pos->board[i] >= BPAWN) && (load_starting_pos->board[i] <= BKING))
            bl_pieces++;
    }

    if ((wh_pieces > 16) || (bl_pieces > 16))
        /*invalid position - should not have been possible to enter?!*/
    {
        Play_Set_Starting_Position();
        return(POS_TOO_MANY_PIECES);
    }

    game_started_from_0 = 0;
    black_started_game = black_starts;
    Util_Memcpy(&(starting_pos), load_starting_pos, sizeof(STARTING_POS));
    Play_Convert_Starting_Pos(&(starting_pos), black_starts);

    cst_p = 0;
    mv_stack_p = 0;
    fifty_moves = 0;
    Util_Memzero(&GlobalPV, sizeof(GlobalPV));
    Util_Memzero(move_stack, sizeof(move_stack));
    Util_Memzero(cstack, sizeof(cstack));
    player_move.u = MV_NO_MOVE_MASK;
    Util_Memzero(&move_redo_stack, sizeof(move_redo_stack));
    game_info.valid = EVAL_INVALID;
    game_info.last_valid_eval = NO_RESIGN;
    dynamic_resign_threshold = RESIGN_EVAL;

    if (Play_King_Positions_Ok(pos_wking, pos_bking) != POS_OK)
    {
        Play_Set_Starting_Position();
        return(POS_KING_INVALID);
    }

    if (((black_starts != 0) && (Mvgen_White_King_In_Check())) ||
            ((black_starts == 0) && (Mvgen_Black_King_In_Check())))
    {
        Play_Set_Starting_Position();
        return(POS_CHECKS_INVALID);
    }

    move_cnt_w = Mvgen_Find_All_White_Moves(movelist, NO_LEVEL, UNDERPROM);
    move_cnt_b = Mvgen_Find_All_Black_Moves(movelist, NO_LEVEL, UNDERPROM);
    if ((move_cnt_w > MAXMV) || (move_cnt_b > MAXMV))
    {
        Play_Set_Starting_Position();
        return(POS_TOO_MANY_MOVES);
    }

    move_cnt_w = Mvgen_Find_All_White_Captures_And_Promotions(movelist, UNDERPROM);
    move_cnt_b = Mvgen_Find_All_Black_Captures_And_Promotions(movelist, UNDERPROM);
    if ((move_cnt_w > MAXCAPTMV) || (move_cnt_b > MAXCAPTMV))
    {
        Play_Set_Starting_Position();
        return(POS_TOO_MANY_CAPTS);
    }

    move_cnt_w = Mvgen_White_King_In_Check_Info(movelist, &dummy);
    move_cnt_b = Mvgen_Black_King_In_Check_Info(movelist, &dummy);
    if ((move_cnt_w > CHECKLISTLEN) || (move_cnt_b > CHECKLISTLEN))
    {
        Play_Set_Starting_Position();
        return(POS_TOO_MANY_CHECKS);
    }

    if ((CFG_GET_OPT(CFG_COMP_SIDE_MODE) != CFG_COMP_SIDE_NONE) &&
        (CFG_GET_OPT(CFG_GAME_MODE     ) != CFG_GAME_MODE_MTI))
        /*in mate search mode, the computer has to be triggered manually.*/
    {
        /*don't give the computer the first move.
        if the player wants the computer to move, he can still press GO.*/
        if (black_starts == 0)
            computer_side = BLACK;
        else
            computer_side = WHITE;
    } else
        computer_side = NONE;

    Hash_Init_Stack();

    return(POS_OK);
}

void Play_Load_Status(const BACKUP_GAME *ptr)
{
    int loaded_mv_stack_p;

    /*straight-forward copy*/
    black_started_game = (ptr->blackstart_searchdisp & SAVE_BLACK_START) ? 1 : 0;
    game_started_from_0 = ptr->game_started_from_0;

    Util_Memcpy(&(starting_pos), &(ptr->starting_pos), sizeof(STARTING_POS));

    cst_p = 0;
    fifty_moves = 0;

    mv_stack_p = 0; /*for repeating all the moves*/
    loaded_mv_stack_p = ptr->mv_stack_p;
    if (loaded_mv_stack_p > MAX_PLIES)
        loaded_mv_stack_p = MAX_PLIES;

    Play_Convert_Starting_Pos(&(starting_pos), black_started_game);

    Hash_Init_Stack();

    /*ok, so now we have the position from where the saved game started, including the castling stuff.
    now we must loop through the moves that were made to arrive at the actual saving point.*/

    while (mv_stack_p < loaded_mv_stack_p)
    {
        /*the moves are saved using compressed format.*/
        MOVE load_move = Mvgen_Decompress_Move(ptr->movelist[mv_stack_p+1]);
        Play_Update_Special_Conditions(load_move);
        Search_Push_Status();
        Search_Make_Move(load_move); /*mv_stack_p gets incremented there.*/
    }

    /*get the other stuff in place.*/
    Util_Memcpy(&(game_info), &(ptr->game_info), sizeof(GAME_INFO));
    Util_Memcpy(&(GlobalPV), &(ptr->GlobalPV), sizeof(LINE));
    Util_Memcpy(&(move_redo_stack), &(ptr->move_redo_stack), sizeof(MOVE_REDO_STACK));

    dynamic_resign_threshold = ptr->dynamic_resign_threshold;
    computer_side = (enum E_COLOUR) ptr->computer_side;
    player_move.u = ptr->player_move.u;
}

/*display power on self test faults. this gets an extra routine
so that the stack usage for the string operations is not permanent.
that's also why this function must not be inlined.*/
static void NEVER_INLINE Play_POST_Fault_Display(uint32_t test_result)
{
    unsigned int errcnt;
    int32_t dummy_conf_time;
    char error_line1[20];
    char error_line2[20];

    Hmi_Signal(HMI_MSG_FAILURE);

    /*how many errors do we have?*/
    errcnt = 0;
    if (test_result & HW_RAM_FAIL)
        errcnt++;
    if (test_result & HW_ROM_FAIL)
        errcnt++;
    if (test_result & HW_XTAL_FAIL)
        errcnt++;
    if (test_result & HW_KEYS_FAIL)
        errcnt++;

    if (errcnt > 1) /*display with correct plural*/
        Util_Strcpy(error_line1, "system faults:");
    else
        Util_Strcpy(error_line1, "system fault:");

    error_line2[0] = '\0';

    if (test_result & HW_RAM_FAIL)
    {
        errcnt--;
        if (errcnt) /*more errors following?*/
            Util_Strcpy(error_line2, "RAM/");
        else
            Util_Strcpy(error_line2, "RAM");
    }
    if (test_result & HW_ROM_FAIL)
    {
        errcnt--;
        if (errcnt) /*more errors following?*/
            Util_Strcat(error_line2, "ROM/");
        else
            Util_Strcat(error_line2, "ROM");
    }
    if (test_result & HW_XTAL_FAIL)
    {
        errcnt--;
        if (errcnt) /*more errors following?*/
            Util_Strcat(error_line2, "CLK/");
        else
            Util_Strcat(error_line2, "CLK");
    }

    if (test_result & HW_KEYS_FAIL)
    {
        /*last error anyway*/
        Util_Strcat(error_line2, "KEY");
    }

    Util_Strcat(error_line2, ".");

    (void) Hmi_Conf_Dialogue(error_line1, error_line2, &dummy_conf_time,
                             HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
}


/*display why the system was reset in case it was not by a power reset.
this way, during normal operation, the user is not annoyed by an
unnecessary dialogue, and if an unexpected reset occurs without a
dialogue, then it must have been due to power loss. None of the
other reset causes should happen, especially not the watchdog
- except of course for an intentional system reset. Since that is
taken care of in the hardware layer, any watchdog that has been reported
until here has been an unintentional one - a software bug.

there can be more than one reset cause, e.g. first a power
interrupt, then a watchdog fault before the user can confirm the
dialogue, and then the reset pin hits in because the pullup resistor
on the board is not properly mounted. SW reset (reset initiated by the
software, i.e. on purpose) should not happen because it is not being
used.

this gets an extra routine so that the stack usage for the string
operations is not permanent. that's also why this function must not
be inlined.

For debugging, also power interrupts can be displayed.*/
static void NEVER_INLINE Play_Sysreset_Cause_Display(void)
{
    unsigned int reset_cause;

    reset_cause = Hw_Get_Reset_Cause();

    /*the system can also be reset WITHOUT any reported cause here, namely by a
    purposely triggered watchdog reset. this will not have the WDG bit set here,
    and so we just skip the dialogue.*/
    if (reset_cause & (HW_SYSRESET_WDG | HW_SYSRESET_PIN | HW_SYSRESET_SW /*| HW_SYSRESET_POWER*/))
    {
        unsigned int errcnt;
        int32_t dummy_conf_time;
        char error_line2[20];

        Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_KEY, HW_MSG_PARAM_BACK_CONF);
        Hmi_Signal(HMI_MSG_FAILURE);

        /*how many causes do we have?*/
        errcnt = 0;
        /*        if (reset_cause & HW_SYSRESET_POWER)
            errcnt++;*/
        if (reset_cause & HW_SYSRESET_WDG)
            errcnt++;
        if (reset_cause & HW_SYSRESET_PIN)
            errcnt++;
        if (reset_cause & HW_SYSRESET_SW)
            errcnt++;

        error_line2[0] = '\0';
        /*
        if (reset_cause & HW_SYSRESET_POWER)
        {
            errcnt--;
            if (errcnt)
                Util_Strcpy(error_line2, "PWR/");
            else
                Util_Strcpy(error_line2, "PWR");
        }
        */
        if (reset_cause & HW_SYSRESET_WDG)
        {
            errcnt--;
            if (errcnt) /*more causes following?*/
                Util_Strcat(error_line2, "WDG/");
            else
                Util_Strcat(error_line2, "WDG");
        }
        if (reset_cause & HW_SYSRESET_PIN)
        {
            errcnt--;
            if (errcnt) /*more causes following?*/
                Util_Strcat(error_line2, "PIN/");
            else
                Util_Strcat(error_line2, "PIN");
        }

        if (reset_cause & HW_SYSRESET_SW)
        {
            /*last cause anyway*/
            Util_Strcat(error_line2, "SWR");
        }

        Util_Strcat(error_line2, ".");

        (void) Hmi_Conf_Dialogue("system reset by:", error_line2, &dummy_conf_time,
                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
    }

    Hw_Clear_Reset_Cause();
}

/*warn the user when a manually saved game blocks autosave.*/
static void NEVER_INLINE Play_Handle_Autosave_Warning(void)
{
    if (Hw_Tell_Autosave_State() == HW_AUTOSAVE_OFF)
    {
        int32_t dummy_conf_time;
        Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_KEY, HW_MSG_PARAM_BACK_CONF);
        Hmi_Signal(HMI_MSG_ATT);
        (void) Hmi_Conf_Dialogue("auto-save off,", "manual save on.", &dummy_conf_time,
                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
    }
}


int FUNC_USED main(VAR_UNUSED int argc, VAR_UNUSED char **argv)
{
    int startup_voltage_ok;
    uint32_t sys_status;
    enum E_BAT_CHECK bat_check;

#ifdef DEBUG_STACK
    top_of_stack = (size_t)(&sys_status);
#endif

    /*configure the hardware and load the config.
      system speed is set to HIGH in USER mode.*/
    Hw_Setup_System();

    /*enable the battery shutdown monitoring in Hw_Getch().*/
    Hmi_Setup_System();

    sys_status = Hw_Check_RAM_ROM_XTAL_Keys();

    /*init the lookup tables.*/
    Eval_Init_Pawns();
    Hash_Init();

    if (sys_status != HW_SYSTEM_OK) /*damaged firmware, RAM, XTAL or keypad?*/
    {
        /*for the dialogue, save energy.*/
        Hw_Set_Speed(SYSTEM_SPEED_LOW, SYSTEM_MODE_USER, CLK_FORCE_AUTO);
        /*tell the user what's wrong.*/
        Play_POST_Fault_Display(sys_status);
        /*ramp up the speed for the battery test during the startup screen.*/
        Hw_Set_Speed(SYSTEM_SPEED_HIGH, SYSTEM_MODE_USER, CLK_FORCE_HIGH);
    } else
        /*check the LEDs and beeper.*/
        Hmi_Signal(HMI_MSG_TEST);

    /*show the start display with device ID and version. Also necessary
      for waiting at least 320ms to get the random generator seed
      initialised using the LSB of the battery reading (every 10ms).
      also use the waiting time for battery check.*/
    Hmi_Show_Start_Screen();

    startup_voltage_ok = Hw_Battery_Newgame_Ok();

    Hw_Set_Speed(SYSTEM_SPEED_LOW, SYSTEM_MODE_KEEP, CLK_FORCE_AUTO);

    /*init the random generator.*/
    Hw_Seed();

    /*display the reset cause.*/
    Play_Sysreset_Cause_Display();

    /*if the auto-save is disabled, warn the user.*/
    Play_Handle_Autosave_Warning();

#ifndef PC_VERSION
    /*if A1 has been pressed during the startup screen, activate
    Easter egg mode.
    disabled if in case of startup errors because it would be confusing,
    or if the battery voltage is not fine anyway.*/
    if ((sys_status == HW_SYSTEM_OK) && (startup_voltage_ok))
    {
        if (Hw_Getch(SLEEP_FORBIDDEN) == KEY_A1)
        {
            /*clear the keyboard queue*/
            while (Hw_Getch(SLEEP_FORBIDDEN) != KEY_NONE) ;

            /*and go for the easter egg*/
            Hmi_Game_Of_Life();
        }
    }
#endif

    /*first battery check has just been during startup screen.*/
    bat_check = BAT_NO_CHECK;

    while (PIGS_DO_NOT_FLY)
    {
#ifndef PC_VERSION
        /*clear the keyboard queue*/
        while (Hw_Getch(SLEEP_FORBIDDEN) != KEY_NONE) ;
#endif

        /*in case the working configuration has been changed automatically during the
        last game, e.g. due to side switching in game-in time control mode, we get
        the persistent configuration again with every new game, just to be sure.*/
        Hw_Load_Config(&hw_config);

        /*display contrast setting as configured*/
        Hw_Disp_Set_Conf_Contrast();

        /*now for the battery check*/

        if (bat_check == BAT_CHECK)
        /*first battery check has been during the startup screen.*/
        {
            /*battery stress test before a new game.*/
            Hw_Set_Speed(SYSTEM_SPEED_HIGH, SYSTEM_MODE_USER, CLK_FORCE_HIGH);

            Hmi_Show_Bat_Wait_Screen(BAT_MON_DELAY);

            startup_voltage_ok = Hw_Battery_Newgame_Ok();

            /*save energy.*/
            Hw_Set_Speed(SYSTEM_SPEED_LOW, SYSTEM_MODE_USER, CLK_FORCE_AUTO);
        } else
            bat_check = BAT_CHECK;

        if (!startup_voltage_ok)
        {
            Hmi_Battery_Shutdown(USER_TURN);
            return(0); /*will not be reached.*/
        }

        Hw_Set_System_Time(0);

        if ((CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_MTI)) ||
            (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_ANA)))
        /*in search mode, the computer has to be triggered manually.*/
            computer_side = NONE;
        else
        {
            switch (CFG_GET_OPT(CFG_COMP_SIDE_MODE))
            {
            case CFG_COMP_SIDE_NONE:
                computer_side = NONE;
                break;
            case CFG_COMP_SIDE_WHITE:
                computer_side = WHITE;
                break;
            case CFG_COMP_SIDE_RND:
                if ((Hw_Rand() & 0x01U) == 0)
                    computer_side = BLACK;
                else
                    computer_side = WHITE;
                break;
            default:
            case CFG_COMP_SIDE_BLACK:
                computer_side = BLACK;
                break;
            }
        }

        Play_Set_Starting_Position();

        Play_Handling();

    }
    return (0); /*will never be reached, just to avoid compiler warnings*/
}
