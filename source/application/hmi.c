/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2015-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of the CT800 (user interface functions).
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

/*the main purpose of this file is to provide a middle layer between the
application and the hardware driver. this file maps the application's
needs to a 20x4 character screen, and it calls the appropriate hardware
related routines for the actual display.

most of the formatting is hardcoded because the whole screen layout would
have to be reworked for another screen anyway.

besides the dialogue system and the game screen layout, also the menu
and the position editor/viewer are implemented here.*/

/*most of the screen buffers except the main screen are kept on the stack.
since the menu isn't accessible during search, the full allocated stack is
available, so every subfunction can happily allocate its own screen buffer.*/

#include <stdint.h>
#include <stddef.h>
#include "ctdefs.h"
#include "confdefs.h"
#include "search.h"
#include "hmi.h"
#include "menu.h"
#include "posedit.h"
#include "timekeeping.h"
#include "hardware.h"
#include "util.h"
#include "move_gen.h"

/*--------- external variables ------------*/
/*-- READ-ONLY  --*/
/*for the position editor / display*/
extern PIECE Wpieces[16];
extern PIECE Bpieces[16];
extern PIECE *board[120];
/*for the move display / pretty print*/
extern int mv_stack_p;
extern MVST move_stack[MAX_STACK+1];
/*from hardware_pc/arm.c - the boot screen display (defaults or settings loaded)*/
extern uint64_t hw_config;
/*energy related stuff - for display during the thinking
time of computer or player*/
extern volatile uint32_t battery_status; /*might get changed in the timer interrupt*/

/*-- READ-WRITE --*/
extern int battery_confirmation;

/*--------- external functions ------------*/

/*for pretty print*/
int Eval_Is_Light_Square(int square);
enum E_HMI_CHECK NEVER_INLINE Play_Get_Status(enum E_COLOUR colour);

/*--------- module variables ------------*/

/*the permanent screen buffers. separating buffer space and buffer pointers
  allows fast buffer switching without content copy.*/
static char hmi_buffer_mem_0[81], hmi_buffer_mem_1[81];

/*buffer pointer for the active display mode*/
static char *hmi_buffer = hmi_buffer_mem_0;
/*buffer pointer for the inactive display mode*/
static char *hmi_alt_buffer = hmi_buffer_mem_1;

/*which search info display mode is active*/
static enum E_SEARCH_DISP hmi_search_disp_mode;
/*alternate display only during computer's turn*/
static int comp_turn;

/*the list of what piece has been moving with what move, for the game screen.*/
static uint8_t moving_piece[MOVING_PIECE_SIZE];

/* The purpose of this buffer: in 9 out of 10 cases, only the last seconds
  digit changes, so why always update all 7 time-related characters on the
  display? It makes sense only to update what has changed.*/
static char running_time_cache[9];

/*in analysis mode, the time until current PV start move is displayed. this
  is useful e.g. for the BT tests. whenever a new depth with new PV comes
  in, it must be checked whether the PV start move has changed.*/
static char pv_start_move[6];

/*--------- module static functions ------------*/

/*store the currently moving piece.*/
static void Hmi_Store_Moving_Piece(int mv_stack_p, char piece_type, int is_capture, int is_check, int is_mate)
{
    uint8_t store_byte;

    switch (piece_type)
    {
    case WKNIGHT_CHAR: store_byte = 0x01U; break;
    case WBISHOP_CHAR: store_byte = 0x02U; break;
    case WROOK_CHAR  : store_byte = 0x03U; break;
    case WQUEEN_CHAR : store_byte = 0x04U; break;
    case WKING_CHAR  : store_byte = 0x05U; break;
    default          : store_byte = 0x00U; break;
    }

    if (is_capture)
        store_byte |= 0x08U;
    if (is_check)
        store_byte |= 0x10U;
    if (is_mate)
        store_byte |= 0x20U;

    moving_piece[mv_stack_p] = store_byte;
}

/*load the currently moving piece.*/
static char Hmi_Load_Moving_Piece(int mv_stack_p, int *is_capture, int *is_check, int *is_mate)
{
    uint8_t store_byte = moving_piece[mv_stack_p];

    *is_capture = (store_byte & 0x08U) ? 1 : 0;
    *is_check   = (store_byte & 0x10U) ? 1 : 0;
    *is_mate    = (store_byte & 0x20U) ? 1 : 0;

    store_byte &= 0x07U;
    switch (store_byte)
    {
    case 0x01U: return WKNIGHT_CHAR;
    case 0x02U: return WBISHOP_CHAR;
    case 0x03U: return WROOK_CHAR;
    case 0x04U: return WQUEEN_CHAR;
    case 0x05U: return WKING_CHAR;
    default   : return WPAWN_CHAR;
    }
}

static enum E_HMI_USER NEVER_INLINE
Hmi_No_Restore_Dialogue(const char *line1, const char *line2,
                        int32_t timeout, enum E_HMI_DIALOGUE dialogue_mode)
{
    int quest_len, i, fillspace, use_timeout;
    enum E_HMI_USER user_answer;
    int32_t end_timer;
    char dialogue_viewport[81];

    /*optionally, the dialogue can time out, e.g. shutdown dialogue*/
    if (timeout != HMI_NO_TIMEOUT)
    {
        use_timeout = 1;
        end_timer = Hw_Get_System_Time() + timeout;
    } else
    {
        /*normal dialogue without timeout.*/
        end_timer = 0;
        use_timeout = 0;
    }

    /*give a caption to the dialogue box what kind of dialogue it is*/

    switch (dialogue_mode)
    {
    case HMI_QUESTION:
    case HMI_POS_SEL:
        Util_Strcpy(dialogue_viewport, "+------SELECT------+|");
        break;
    case HMI_MONO_STAT:
        Util_Strcpy(dialogue_viewport, "+-------STAT-------+|");
        dialogue_mode = HMI_INFO;
        break;
    case HMI_MULTI_STAT:
        Util_Strcpy(dialogue_viewport, "+-------STAT-------+|");
        dialogue_mode = HMI_QUESTION;
        break;
    case HMI_PV:
        Util_Strcpy(dialogue_viewport, "+--------PV--------+|");
        dialogue_mode = HMI_INFO;
        break;
    case HMI_INFO:
    case HMI_NO_FEEDBACK:
    default:
        Util_Strcpy(dialogue_viewport, "+-------INFO-------+|");
        break;
    }

    /*now for the first line of the message.*/
    quest_len = Util_Strlen(line1);

    if (quest_len >= 18)
    /* just fill in the first 18 characters.*/
    {
        for (i = 0; i < 18; i++)
            dialogue_viewport[i + 21] = line1[i];
    } else
    /*try to align the message to the centre of the line.*/
    {
        fillspace = (18 - quest_len) >> 1;
        for (i = 0; i < fillspace; i++)
            dialogue_viewport[i + 21] = ' ';
        Util_Strcpy(dialogue_viewport + 21 + fillspace, line1);
        for (i = fillspace + quest_len; i < 18 ; i++)
            dialogue_viewport[i + 21] = ' ';
    }

    dialogue_viewport[39] = '|';
    dialogue_viewport[40] = '|';

    /*now for the second line of the message.*/
    quest_len = Util_Strlen(line2);

    if (quest_len >= 18)
    /* just fill in the first 18 characters.*/
    {
        for (i = 0; i < 18; i++)
            dialogue_viewport[i + 41] = line2[i];
    } else
    /*try to align the message to the centre of the line.*/
    {
        fillspace = (18 - quest_len) >> 1;
        for (i = 0; i < fillspace; i++)
            dialogue_viewport[i + 41] = ' ';
        Util_Strcpy(dialogue_viewport + 41 + fillspace, line2);
        for (i = fillspace + quest_len; i < 18 ; i++)
            dialogue_viewport[i + 41] = ' ';
    }
    /*tell the user what his answering options are.*/
    if (dialogue_mode == HMI_QUESTION)
        Util_Strcpy(dialogue_viewport + 59,"|+----<OK>--<CL>----+");
    else if (dialogue_mode == HMI_POS_SEL)
        Util_Strcpy(dialogue_viewport + 59,"|+----<OK>-<POS>----+");
    else
        Util_Strcpy(dialogue_viewport + 59,"|+-------<OK>-------+");

    /*display the dialogue box.*/
    Hw_Disp_Show_All(dialogue_viewport, HW_DISP_DIALOGUE);

    Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_KEY, HW_MSG_PARAM_BACK_CONF);

    /*used e.g. for system error stuff where we're in an interrupt context, so
    the usual keyboard handling is not available.*/
    if (dialogue_mode == HMI_NO_FEEDBACK)
        return(HMI_USER_OK);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_USER_INVALID;

    do {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);
        /*evaluate the feedback depending on whether this is a question or information dialogue.*/
        if (user_key == KEY_ENT)
            user_answer = HMI_USER_OK;
        if ((user_key == KEY_CL) && (dialogue_mode == HMI_QUESTION))
            user_answer = HMI_USER_CANCEL;
        if ((user_key == KEY_POS_DISP) && (dialogue_mode == HMI_POS_SEL))
            user_answer = HMI_USER_DISP;

        /*if a timeout is necessary, fake a user "OK" after the timeout.*/
        if ((use_timeout) && (Hw_Get_System_Time() > end_timer))
            user_answer = HMI_USER_OK;

    } while (user_answer == HMI_USER_INVALID);

    /*and tell the caller what the user decided (only relevant if this dialogue has been a question).*/
    return(user_answer);
}

/*gives the buffer index for the viewport, given a square and normal or flipped display*/
static int Hmi_Square_To_Viewport(int square, int white_bottom)
{
    if ((square >= A1) && (square <= H1))
        return ((white_bottom) ? (square - A1 + 60) : (19 + A1 - square));
    if ((square >= A2) && (square <= H2))
        return ((white_bottom) ? (square - A2 + 40) : (39 + A2 - square));
    if ((square >= A3) && (square <= H3))
        return ((white_bottom) ? (square - A3 + 20) : (59 + A3 - square));
    if ((square >= A4) && (square <= H4))
        return ((white_bottom) ? (square - A4 +  0) : (79 + A4 - square));
    if ((square >= A5) && (square <= H5))
        return ((white_bottom) ? (square - A5 + 72) : ( 7 + A5 - square));
    if ((square >= A6) && (square <= H6))
        return ((white_bottom) ? (square - A6 + 52) : (27 + A6 - square));
    if ((square >= A7) && (square <= H7))
        return ((white_bottom) ? (square - A7 + 32) : (47 + A7 - square));
    if ((square >= A8) && (square <= H8))
        return ((white_bottom) ? (square - A8 + 12) : (67 + A8 - square));

    return(0); /*should not happen*/
}

/*viewport is an 81 (!) bytes buffer, 4x20 characters plus room for 0 termination.
what we want to get is something like this,
initial position with white at the bottom:
= = = =4::8rnbqkbnr
= = = = 3::7pppppppp
PPPPPPPP2::6 = = = =
RNBQKBNR1::5= = = =

initial position with black at the bottom:
= = = =5::1RNBKQBNR
= = = = 6::2PPPPPPPP
pppppppp7::3 = = = =
rnbkqbnr8::4= = = =

it gets the position that is currently on the board.*/
static void Hmi_Get_Current_Board(char *viewport, int white_bottom)
{
    int i;
    Hmi_Init_Board(viewport, white_bottom); /*get the empty board display*/
    for (i = 0; i < 16; i++)
    {
        if (Wpieces[i].xy != NO_PIECE) /*if the piece is on the board*/
            viewport[Hmi_Square_To_Viewport(Wpieces[i].xy, white_bottom)] = Hmi_Get_Piece_Char(Wpieces[i].type, MIXEDCASE);
        if (Bpieces[i].xy != NO_PIECE) /*if the piece is on the board*/
            viewport[Hmi_Square_To_Viewport(Bpieces[i].xy, white_bottom)] = Hmi_Get_Piece_Char(Bpieces[i].type, MIXEDCASE);
    }
}

/*makes a piece list which looks like that (initial position):
white pieces:
K,Q,2R,BB,2N,8P
black pieces:
k,q,2r,bb,2n,8p

the bishops are displayed as:
BB for "both bishops"
LB for "light squared bishop",
DB for "dark squared bishop"

the case that a player has two light squared bishops simply is ignored,
so that will be displayed just as "LB". Doesn't happen in real games
anyway. Displaying them separately would not work either, consider the
following worst case scenario:
K,2Q,2R,2LB,2DB,2N,2P
that's already 21 characters, but the line only holds 20.
OK, we could omit the kings, but I don't like that idea.*/
static void Hmi_Get_Current_Piecelist(char *board_viewport)
{
    int queens, rooks, lightbishops, darkbishops, knights, pawns, material;
    PIECE *p;
    Util_Strcpy(board_viewport, "white:              K,                  black:              k,                  ");
    Hmi_Set_Bat_Display(board_viewport);

    material = 0;

    /*first, for the white pieces*/
    queens = 0;
    rooks = 0;
    lightbishops = 0;
    darkbishops = 0;
    knights = 0;
    pawns = 0;
    for (p = Wpieces[0].next; p != NULL; p = p->next)
    {
        switch (p->type) /*king is ignored, that's already filled in*/
        {
        case WQUEEN:
            queens++;
            material += 9;
            break;
        case WROOK:
            rooks++;
            material += 5;
            break;
        case WBISHOP:
            if (Eval_Is_Light_Square(p->xy)) lightbishops++; else darkbishops++;
            material += 3;
            break;
        case WKNIGHT:
            knights++;
            material += 3;
            break;
        case WPAWN:
            pawns++;
            material += 1;
            break;
        default:
            break;
        }
    }
    Hmi_Print_Piecelist(board_viewport + 20, WHITE, queens, rooks, lightbishops, darkbishops, knights, pawns);

    /*now for the black pieces*/
    queens = 0;
    rooks = 0;
    lightbishops = 0;
    darkbishops = 0;
    knights = 0;
    pawns = 0;
    for (p = Bpieces[0].next; p != NULL; p = p->next)
    {
        switch (p->type) /*king is ignored, that's already filled in*/
        {
        case BQUEEN:
            queens++;
            material -= 9;
            break;
        case BROOK:
            rooks++;
            material -= 5;
            break;
        case BBISHOP:
            if (Eval_Is_Light_Square(p->xy)) lightbishops++; else darkbishops++;
            material -= 3;
            break;
        case BKNIGHT:
            knights++;
            material -= 3;
            break;
        case BPAWN:
            pawns++;
            material -= 1;
            break;
        default:
            break;
        }
    }
    Hmi_Print_Piecelist(board_viewport + 60, BLACK, queens, rooks, lightbishops, darkbishops, knights, pawns);
    Hmi_Add_Material_Disp(board_viewport, material);
}

static int Hmi_Is_Promotion(CMOVE m)
{
    MOVE decomp_move = Mvgen_Decompress_Move(m);

    switch(decomp_move.m.flag)
    {
    case WROOK  :
    case BROOK  :
    case WKNIGHT:
    case BKNIGHT:
    case WBISHOP:
    case BBISHOP:
    case WQUEEN :
    case BQUEEN :
        return(1);
    }
    return(0);
}

/*gets the first eight plies of the PV.*/
static void Hmi_Get_Pv_Info(char *line1, char *line2, LINE *pv)
{
    int i, len, prom_moves;

    line1[0] = line2[0] = '\0';

    for (i = 0, prom_moves = 0; ((i < pv->line_len) && (i < 4)); i++)
        if (Hmi_Is_Promotion(pv->line_cmoves[i]))
            prom_moves++;

    /*one promotion move per line fits without eliminating a whitespace.*/

    for (i = 0, len = 0; ((i < pv->line_len) && (i < 4)); i++)
    {
        MOVE decomp_move = Mvgen_Decompress_Move(pv->line_cmoves[i]);
        len += Util_Convert_Moves(decomp_move, line1 + len);

        if ((Hmi_Is_Promotion(pv->line_cmoves[i])) && (prom_moves > 1))
            prom_moves--;
        else
        {
            line1[len] = ' ';
            len++;
        }
    }
    if (len > 0)
        /*remove the space character at the end of the line*/
        line1[len - 1] = '\0';

    for (i = 4, prom_moves = 0; ((i < pv->line_len) && (i < 8)); i++)
        if (Hmi_Is_Promotion(pv->line_cmoves[i]))
            prom_moves++;

    for (i = 4, len = 0; ((i < pv->line_len) && (i < 8)); i++)
    {
        MOVE decomp_move = Mvgen_Decompress_Move(pv->line_cmoves[i]);
        len += Util_Convert_Moves(decomp_move, line2 + len);

        if ((Hmi_Is_Promotion(pv->line_cmoves[i])) && (prom_moves > 1))
            prom_moves--;
        else
        {
            line2[len] = ' ';
            len++;
        }
    }
    if (len > 0)
        /*remove the space character at the end of the line*/
        line2[len - 1] = '\0';
}

static int Hmi_Castling_Pretty_Print(char *buffer, char piece_type)
{
    if ((piece_type != 'K') || (buffer[0] != 'e'))
        return(0);

    if (!Util_Strcmp(buffer, "e1-g1") || !Util_Strcmp(buffer, "e8-g8"))
    {
        Util_Strcpy(buffer, "   0-0");
        return(1);
    }
    if (!Util_Strcmp(buffer, "e1-c1") || !Util_Strcmp(buffer, "e8-c8"))
    {
        Util_Strcpy(buffer, " 0-0-0");
        return(1);
    }

    return(0);
}

/*converts the coordinate move notation into long algebraic notation*/
static void Hmi_Move_Pretty_Print(char *buffer, char piece_type, int is_capture, int is_check, int is_mate)
{
    int len;
    /*make room for the '-' or 'x' between the from and the to square*/
    buffer[6] = '\0';
    buffer[5] = buffer[4];
    buffer[4] = buffer[3];
    buffer[3] = buffer[2];

    /*insert '-' for a move or 'x' for a capture*/
    buffer[2] = (is_capture) ? 'x' : '-';

    /*castling is special anyway*/
    if (Hmi_Castling_Pretty_Print(buffer, piece_type) == 0)
    {
        /*not castling. make room for the moving piece denominator.*/
        int i;

        buffer[7] = '\0';

        for (i = 6; i > 0; i--)
            buffer[i] = buffer[i - 1];

        /*and insert the moving piece name or space for pawn.*/
        buffer[0] = (piece_type == WPAWN_CHAR) ? ' ' : piece_type;
    }
    len = Util_Strlen(buffer);

    /*add a '#' if the move gives checkmate, or else, add '+' if it gives check*/
    if (is_mate)
        buffer[len++] = '#';
    else if (is_check)
        buffer[len++] = '+';

    /*maximum characters of a move is 8, e.g. ' b7-b8Q#'.
    if the move takes less than 8 characters, fill up the rest with ' '.*/
    while (len < 8)
        buffer[len++] = ' ';

    /*for formatting reasons, 8 characters are too many. usually, moves take
    7 characters like 'Rc1-c7+'. the example ' b7-b8Q#' is the only sort
    that takes 8, and that is due to the space in the beginning plus trailing
    promotion piece plus check/mate. so this kind of move needs to be shifted
    in the buffer to eliminate the leading space.

    The drawback is that the '-' or 'x' letter will not be neatly in a row with
    the corresponding letter of the other moves.

    But on the other hand, there are two cases where the formatting would be problematic:
    a) in the "notation view" in the first line if the BAT for low battery shows up,
    then there would not be a space between the move and BAT.
    b) in the game screen in the third line if the game mode is game-in or tournament
    and the remaining time of the corresponding player is 1:00:00 or more. In this
    case, the space between move and time would also be lacking.*/
    if ((buffer[7] != ' ') && (buffer[0] == ' '))
    {
        int i;

        for (i = 0; i < 7; i++)
            buffer[i] = buffer[i + 1];
    }
    buffer[7] = '\0';
}

/*prepare the alternate hmi buffer for search*/
void NEVER_INLINE Hmi_Build_Alternate_Screen(int black_started_game, char *viewport)
{
    int movenumber;

    Util_Strcpy(viewport, "IN    . E: --                                               D: 0 M: --      0:00");
    Hmi_Set_Bat_Display(viewport);
    if (black_started_game != 0)
        black_started_game = 1;
    movenumber  = (mv_stack_p + 2 + black_started_game) / 2;
    Util_Itoa(viewport + 3, movenumber);
}

/*insert search info for analysis mode and alternate search info display*/
static void Hmi_Update_Search_Disp(char *viewport, int eval, int depth,
                                   const char *pv_line_1, const char *pv_line_2)
{
    int i;

    Hmi_Set_Bat_Display(viewport);

    /*evaluation*/
    if (Abs(eval) < MATE_CUTOFF)
        Util_Centipawns_To_String(viewport + 10, eval);
    else
    {
        viewport[10] = ' ';
        viewport[11] = (eval > 0) ? '+' : '-';
        Util_Strins(viewport + 12, "mate");
    }

    /*depth*/
    Util_Depth_To_String(viewport + 62, depth);

    /*first four moves of the PV come into the 2nd display line,
    next four into the third line*/
    for (i = 0; ((i < 20) && (pv_line_1[i] != 0)); i++)
        viewport[i + 20] = pv_line_1[i];
    for (; i < 20; i++)
        viewport[i + 20] = ' ';

    for (i = 0; ((i < 20) && (pv_line_2[i] != 0)); i++)
        viewport[i + 40] = pv_line_2[i];
    for (; i < 20; i++)
        viewport[i + 40] = ' ';
}



/*--------- global functions ------------*/



void Hmi_Restore_Viewport(void)
{
    /*if the viewport to be restored has been a dialogue, use
      the fancy display, otherwise not.*/
    if ((hmi_buffer[0 ] == '+') && (hmi_buffer[19] == '+') &&
        (hmi_buffer[20] == '|') && (hmi_buffer[39] == '|') &&
        (hmi_buffer[40] == '|') && (hmi_buffer[59] == '|') &&
        (hmi_buffer[60] == '+') && (hmi_buffer[79] == '+'))
    {
        Hw_Disp_Show_All(hmi_buffer, HW_DISP_DIALOGUE);
    } else
        Hw_Disp_Show_All(hmi_buffer, HW_DISP_RAW);
}

/*displays the game notation in pretty-print format.
if the game has been started by black (via an entered position), the first half-move
by white is missing and will be displayed as "1. ...".
if this function has been called via the menu, then the menu button quits the whole
menu and goes back to the game screen. If this function ahs been called during the
end-of-game dialogue, i.e. after the position display, then the menu button shall be
ignored.*/
enum E_HMI_MENU NEVER_INLINE Hmi_Disp_Movelist(int black_started_game, enum E_HMI_MEN_POS menu_mode)
{
    enum E_HMI_MENU user_answer;
    int disp_mv_stack_p;

    /*make sure that "true" is integer 1*/
    if (black_started_game != 0)
        black_started_game = 1;

    /*start with ply 0 (which doesn't exist) if black has started the game*/
    disp_mv_stack_p = 1 - black_started_game;

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do {
        char menu_buffer[81];
        int disp_ply, movenumber, offset, user_key_valid;

        if (mv_stack_p > 0)
            Util_Strcpy(menu_buffer, "LST                                     CL                                      ");
        else
            Util_Strcpy(menu_buffer, "LST                       no moves.     CL                                      ");
        /*is there a previous display page? then display the back-icon*/
        if (disp_mv_stack_p > 4 - black_started_game)
#ifdef PC_VERSION
            menu_buffer[60] = '<';
#else
            menu_buffer[60] = '\x7f';
#endif
        /*is there a following display page? then display the next-icon*/
        if (mv_stack_p - disp_mv_stack_p >= 4)
#ifdef PC_VERSION
            menu_buffer[79] = '>';
#else
            menu_buffer[79] = '\x7e';
#endif

        /*display up to four plies, starting with the beginning of this page.
        offset refers to the display line of the displayed ply.
        take care not to display more plies than the game has.*/
        for (disp_ply = disp_mv_stack_p, offset = 0; ((disp_ply < disp_mv_stack_p+4) && (disp_ply <= mv_stack_p)); offset += 20, disp_ply++)
        {
            if (disp_ply == 0) /*may happen with the first (omitted) ply when black has started the position*/
                Util_Strins(menu_buffer + 6, "1. ...");
            else
            {
                char buffer[10], mv_piece_char;
                int is_capture, is_check, is_mate, i;

                movenumber = (disp_ply - 1 + black_started_game) / 2 + 1;
                /*put the move number into the displayline*/
                Util_Itoa(menu_buffer + offset + 4, movenumber);
                menu_buffer[offset + 7] = '.';

                /*convert the move to a string*/
                Util_Convert_Moves(move_stack[disp_ply].move, buffer);
                /*get all the surrounding data*/
                mv_piece_char = Hmi_Load_Moving_Piece(disp_ply, &is_capture, &is_check, &is_mate);
                /*and convert the move string to long algebraic notation*/
                Hmi_Move_Pretty_Print(buffer, mv_piece_char, is_capture, is_check, is_mate);

                /*put the algebraic move notation into the display buffer; offset is the display line*/
                for (i = 0; i < 7; i++)
                    menu_buffer[offset + 9 + i] = buffer[i];
            }
        }

        Hmi_Set_Bat_Display(menu_buffer);
        Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

        /*if we come here from the menu, the backlight will only work for 15s,
        triggered by the keystroke that brought us here. but 15s is not
        enough to transcibe the moves.*/
        Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_POS, HW_MSG_PARAM_BACK_CONF);

        user_key_valid = 0;
        while (user_key_valid == 0)
        {
            enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

            switch (user_key)
            {
            case KEY_CL:
            case KEY_ENT:
                /*just leave the menu, do nothing*/
                user_answer = HMI_MENU_OK;
                user_key_valid = 1;
                break;
            case KEY_MENU:
                if (menu_mode == HMI_MENU_MODE_MEN)
                {
                    user_answer = HMI_MENU_LEAVE;
                    user_key_valid = 1;
                }
                break;
            case KEY_MENU_PLUS:
                if (mv_stack_p - disp_mv_stack_p >= 4)
                {
                    disp_mv_stack_p += 4;
                    user_key_valid = 1;
                }
                /*when transcibing moves, 15s of backlight is not enough. you always have to hit the
                backlight key to write down four plies, and that is annoying. Better give 30s of
                backlight automatically.*/
                Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_POS, HW_MSG_PARAM_BACK_CONF);
                break;
            case KEY_MENU_MINUS:
                if (disp_mv_stack_p > 4 - black_started_game)
                {
                    disp_mv_stack_p -= 4;
                    user_key_valid = 1;
                }
                /*when transcibing moves, 15s of backlight is not enough. you always have to hit the
                backlight key to write down four plies, and that is annoying. Better give 30s of
                backlight automatically.*/
                Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_POS, HW_MSG_PARAM_BACK_CONF);
                break;
             default:
                break;
            }
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}

/*for the position editor to clear the pretty print stuff when
entering a new position.*/
void Hmi_Clear_Pretty_Print(void)
{
    Util_Memzero(moving_piece, sizeof(moving_piece));
}

/*viewport is an 81 (!) bytes buffer, 4x20 characters plus room for 0 termination.
This gives an empty board display that will get filled with the pieces elsewhere.

for the ARM-version, 0xff shall be displayed in the black squares, which is a black rectangle.*/
void Hmi_Init_Board(char *viewport, int white_bottom)
{
    if (white_bottom)
#ifdef PC_VERSION
        Util_Strcpy(viewport, " = = = =4::8 = = = == = = = 3::7= = = =  = = = =2::6 = = = == = = = 1::5= = = = ");
#else
        Util_Strcpy(viewport, " \xff \xff \xff \xff""4::8 \xff \xff \xff \xff\xff \xff \xff \xff 3::7\xff \xff \xff \xff  \xff \xff \xff \xff""2::6 \xff \xff \xff \xff\xff \xff \xff \xff 1::5\xff \xff \xff \xff ");
#endif
    else
#ifdef PC_VERSION
        Util_Strcpy(viewport, " = = = =5::1 = = = == = = = 6::2= = = =  = = = =7::3 = = = == = = = 8::4= = = = ");
#else
        Util_Strcpy(viewport, " \xff \xff \xff \xff""5::1 \xff \xff \xff \xff\xff \xff \xff \xff 6::2\xff \xff \xff \xff  \xff \xff \xff \xff""7::3 \xff \xff \xff \xff\xff \xff \xff \xff 8::4\xff \xff \xff \xff ");
#endif
}

char Hmi_Get_Piece_Char(int type, enum E_DISP_CASE case_mode)
{
    char ret;
    switch(type) {
    case BROOK:
        ret = BROOK_CHAR;
        break;
    case WROOK:
        ret = WROOK_CHAR;
        break;
    case BKNIGHT:
        ret = BKNIGHT_CHAR;
        break;
    case WKNIGHT:
        ret = WKNIGHT_CHAR;
        break;
    case BBISHOP:
        ret = BBISHOP_CHAR;
        break;
    case WBISHOP:
        ret = WBISHOP_CHAR;
        break;
    case BQUEEN:
        ret = BQUEEN_CHAR;
        break;
    case WQUEEN:
        ret = WQUEEN_CHAR;
        break;
    case BKING:
        ret = BKING_CHAR;
        break;
    case WKING:
        ret = WKING_CHAR;
        break;
    case BPAWN:
        ret = BPAWN_CHAR;
        break;
    case WPAWN:
        ret = WPAWN_CHAR;
        break;
    default:
        ret = '?'; /*should not happen*/
        break;
    }
    if (case_mode == UPPERCASE)
        if ((ret >= 'a') && (ret <= 'z'))
            ret -= 'a' - 'A';
    return(ret);
}

void Hmi_Print_Piecelist(char *viewport_line, enum E_COLOUR colour, int queens,
                         int rooks, int lightbishops, int darkbishops,
                         int knights, int pawns)
{
    int i = 2; /*after the comma that follows the king in the display*/

    if (queens)
    {
        if (queens > 1)
            viewport_line[i++] = (char) (queens + '0');
        viewport_line[i++] = (colour == WHITE) ? WQUEEN_CHAR : BQUEEN_CHAR;
        viewport_line[i++] = ',';
    }
    if (rooks)
    {
        if (rooks > 1)
        {
            if (rooks >= 10) /*more than 10 is not possible*/
            {
                viewport_line[i++] = '1';
                viewport_line[i++] = '0';
            } else
                viewport_line[i++] = (char) (rooks + '0');
        }
        viewport_line[i++] = (colour == WHITE) ? WROOK_CHAR : BROOK_CHAR;
        viewport_line[i++] = ',';
    }
    if ((lightbishops) || (darkbishops))
    {
        if ((lightbishops) && (darkbishops))
            viewport_line[i++] = (colour == WHITE) ? 'B' : 'b';
        else if (lightbishops)
            viewport_line[i++] = (colour == WHITE) ? 'L' : 'l';
        else
            viewport_line[i++] = (colour == WHITE) ? 'D' : 'd';
        viewport_line[i++] = (colour == WHITE) ? WBISHOP_CHAR : BBISHOP_CHAR;
        viewport_line[i++] = ',';
    }
    if (knights)
    {
        if (knights > 1)
        {
            if (knights >= 10) /*more than 10 is not possible*/
            {
                viewport_line[i++] = '1';
                viewport_line[i++] = '0';
            } else
                viewport_line[i++] = (char) (knights + '0');
        }
        viewport_line[i++] = (colour == WHITE) ? WKNIGHT_CHAR : BKNIGHT_CHAR;
        viewport_line[i++] = ',';
    }
    if (pawns)
    {
        if (pawns > 1)
            viewport_line[i++] = (char) (pawns + '0');
        viewport_line[i++] = (colour == WHITE) ? WPAWN_CHAR : BPAWN_CHAR;
        viewport_line[i++] = ',';
    }
    /*replace the last comma in the list by a dot*/
    viewport_line[--i] = '.';
}

void Hmi_Add_Material_Disp(char *board_viewport, int material)
{
    Util_Pawns_To_String(board_viewport +  7,  material);
    Util_Pawns_To_String(board_viewport + 47, -material);
}

/*displays the current position, upside down or normal.
if white is to move, the display will be normal, and upside down if black is to move.
the user may change between these views.
The function records the time spent in *conf_time.
The calling logic may want this dialogue the restore the screen afterwards, which
is useful if there is only this dialogue to be displayed.
However, if the calling logic intends to display follow-up dialogues, restoring the
screen right after this dialogue and then displaying the next one would cause
annoying screen flickering. In this case, the calling logic must use the
last dialogue in that sequence with automatic restore.
If the user cancels the sequence, the calling logic must Hmi_Restore_Viewport().*/
enum E_HMI_MENU NEVER_INLINE
Hmi_Display_Current_Board(int white_bottom, int32_t* conf_time, enum E_HMI_REST_MODE restore_viewport)
{
    enum E_HMI_USER user_answer;
    int disp_state = 0;
    int32_t conf_start_time;
    char board_viewport[81];

    conf_start_time = Hw_Get_System_Time();

    Hmi_Set_Cursor(0, DISP_CURSOR_OFF);

    /*disp_state is a state machine.
    state 0: display the board.
    state 1: display the flipped board.
    state 2: display the piece list.*/

    do {
        switch (disp_state) /* state machine for the display*/
        {
        case 0:
        default:
            Hmi_Get_Current_Board(board_viewport, white_bottom);
            break;
        case 1:
            Hmi_Get_Current_Board(board_viewport, !white_bottom);
            break;
        case 2:
            Hmi_Get_Current_Piecelist(board_viewport);
            break;
        }

        Hw_Disp_Show_All(board_viewport, HW_DISP_RAW);
        Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_POS, HW_MSG_PARAM_BACK_CONF);

        /*gather user feedback until he gives a valid answer.*/
        user_answer = HMI_USER_INVALID;

        do {
            enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

            if (user_key == KEY_ENT)
                user_answer = HMI_USER_OK;
            if (user_key == KEY_CL)
                user_answer = HMI_USER_CANCEL;
            if (user_key == KEY_POS_DISP)
                user_answer = HMI_USER_FLIP; /*the user wants to flip the board display upside down*/

            /*the MENU key shall only exit the board display if we are here over the menu.
            otherwise, when selecing the POS key during normal play, that isn't a menu
            function, so the menu key shall not exit the board display.*/
            if ((user_key == KEY_MENU) && (restore_viewport == HMI_BOARD_MENU))
                user_answer = HMI_USER_DISP;

        } while (user_answer == HMI_USER_INVALID);

        /*continue the state machine*/
        if (disp_state >= 2) disp_state = 0; else disp_state++;
    } while (user_answer == HMI_USER_FLIP);

    /*restore the screen to what it was before the board display*/
    if (restore_viewport == HMI_RESTORE)
        Hmi_Restore_Viewport();

    *conf_time = Hw_Get_System_Time() - conf_start_time; /*so much time was spent in this display.*/
    if (user_answer == HMI_USER_DISP)
        return (HMI_MENU_LEAVE);
    else
        return(HMI_MENU_OK);
}

/*displays one character c in the move input line.
  chars_entered does not include the current character c.*/
void Hmi_Disp_Move_Char(char c, int chars_entered)
{
    if ((chars_entered >= 0) && (chars_entered <= 5))
    {
        if (c == '\b')
        {
            if (chars_entered == 2)
            {
                /*erase the separating '-'*/
                hmi_buffer[HMI_MOVE_POS + chars_entered] = ' ';
                /*erase source rank*/
                chars_entered--;
                hmi_buffer[HMI_MOVE_POS + chars_entered] = ' ';
                Hw_Disp_Update(hmi_buffer, HMI_MOVE_POS + 1, 2);
                Hmi_Set_Cursor(HMI_MOVE_POS + 1, DISP_CURSOR_ON);
            } else if (chars_entered > 0) /*if there is something to erase*/
            {
                if (chars_entered == 1) /*otherwise, the separating '-' counts*/
                    chars_entered--;
                hmi_buffer[HMI_MOVE_POS + chars_entered] = ' ';
                Hw_Disp_Update(hmi_buffer, HMI_MOVE_POS + chars_entered, 1);
                Hmi_Set_Cursor(HMI_MOVE_POS + chars_entered, DISP_CURSOR_ON);
            }
        } else /*not backspace*/
        {
            if (chars_entered == 1)
            {
                /*put a '-' between from and to square*/
                hmi_buffer[HMI_MOVE_POS + 1] = c;
                hmi_buffer[HMI_MOVE_POS + 2] = '-';
                Hw_Disp_Update(hmi_buffer, HMI_MOVE_POS + 1, 2);
                Hmi_Set_Cursor(HMI_MOVE_POS + 3, DISP_CURSOR_ON);
            } else
            {
                if (chars_entered >= 2)
                    /*the '-' has been appended, so move one display character to the right*/
                    chars_entered++;
                hmi_buffer[HMI_MOVE_POS + chars_entered] = c;
                Hw_Disp_Update(hmi_buffer, HMI_MOVE_POS + chars_entered, 1);
                Hmi_Set_Cursor(HMI_MOVE_POS + chars_entered + 1, DISP_CURSOR_ON);
            }
        }
    }
}

/*displays the whole move as far as it has been entered*/
void Hmi_Disp_Move_Line(char* buffer, int chars_entered)
{
    if ((chars_entered >= 0) && (chars_entered <= 5))
    {
        int i, buf_cnt;
        for (i = 0, buf_cnt = 0; i < chars_entered; i++, buf_cnt++)
        {
            if (i == 2) /*add separating '-'*/
            {
                hmi_buffer[HMI_MOVE_POS + i] = '-';
                i++;
                chars_entered++;
            }
            hmi_buffer[HMI_MOVE_POS + i] = buffer[buf_cnt];
        }
        if (chars_entered == 2)
        {
            hmi_buffer[HMI_MOVE_POS + chars_entered] = '-';
            chars_entered++;
        }
        Hw_Disp_Update(hmi_buffer, HMI_MOVE_POS, chars_entered);
        Hmi_Set_Cursor(HMI_MOVE_POS + chars_entered, DISP_CURSOR_ON);
    }
}

/*clears the display of the input move*/
void Hmi_Clear_Input_Move(void)
{
    Util_Strins(hmi_buffer + HMI_MOVE_POS, "      ");
    Hw_Disp_Update(hmi_buffer, HMI_MOVE_POS, 6);
    Hmi_Set_Cursor(HMI_MOVE_POS, DISP_CURSOR_ON);
}

/*updates the time display in the current viewport, but only transmits
the relevant characters to the display.
the affected viewport area is the last line, last 7 characters.*/
void Hmi_Update_Running_Time(int32_t disp_ms, int32_t raw_ms,
                             int cursorpos, enum E_TIME_DISP disp_toggle)
{
    int disp_update_char_pos, i, no_mate_mode;
    enum E_UT_ROUND rounding_time;

    /*what kind of time control is applicable for rounding?*/
    switch (CFG_GET_OPT(CFG_GAME_MODE))
    {
    case CFG_GAME_MODE_TPM:
    case CFG_GAME_MODE_ANA:
    case CFG_GAME_MODE_MTI:
    default:
        /*round elapsed time downwards*/
        rounding_time = UT_TIME_ROUND_FLOOR;
        break;
    case CFG_GAME_MODE_GMI:
    case CFG_GAME_MODE_TRN:
        /*round remaining time upwards*/
        rounding_time = UT_TIME_ROUND_CEIL;
        break;
    }

    /*in mate search mode, we display also tens of hours.*/
    if (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_MTI))
    {
        no_mate_mode = 0;
        /*with an additional position for tens of hours.*/
        Util_Long_Time_To_String(hmi_buffer + 72, disp_ms, rounding_time);
    } else
    {
        no_mate_mode = 1;

        /*no alternate search display in MTI and ANA mode*/
        if (!(CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_ANA)))
        {
            /*comp_turn got set up when building the game screen*/
            if (comp_turn)
            {
                int root_update;
                MOVE root_move;
                char root_move_buf[6];
                char *viewport = (hmi_search_disp_mode == DISP_NORM) ? hmi_alt_buffer : hmi_buffer;

                /*fetch the current root move of the search*/
                root_move = Search_Get_Current_Root_Move();

                /*convert 4 character moves (non-promotions)
                  to 5 characters with leading space*/
                if (Util_Convert_Moves(root_move, root_move_buf) < 5)
                {
                    for (i = 5; i > 0; i--)
                        root_move_buf[i] = root_move_buf[i - 1];
                    root_move_buf[0] = ' ';
                }

                /*check how much display update is necessary for the root move*/
                for (i = 0, root_update = -1; i < 5; i++)
                {
                    char *buf_ptr = root_move_buf + i;
                    char *view_ptr = viewport + i + 67;

                    if (*buf_ptr != *view_ptr)
                    {
                        Util_Strins(view_ptr, buf_ptr);
                        root_update = i;
                        break;
                    }
                }

                if (disp_toggle != NO_DISP_TOGGLE)
                {
                    /*switch buffers*/
                    char *tmp_buf_ptr = hmi_alt_buffer;
                    hmi_alt_buffer = hmi_buffer;
                    hmi_buffer = tmp_buf_ptr;

                    if (hmi_search_disp_mode == DISP_NORM)
                    {
                        hmi_search_disp_mode = DISP_ALT;
                        /*alternate display always counts up*/
                        disp_ms = raw_ms;
                        /*and rounds downwards*/
                        rounding_time = UT_TIME_ROUND_FLOOR;
                    } else
                        hmi_search_disp_mode = DISP_NORM;

                    Util_Time_To_String(hmi_buffer + 73, disp_ms, UT_NO_LEAD_ZEROS, rounding_time);
                    /*running time cache with offset 1 because 0 is only used
                      for mate-in mode which can count up to 96 hours.*/
                    for (i = 1; i < 8; i ++)
                        running_time_cache[i] = hmi_buffer[73 + i - 1];

                    Hw_Disp_Show_All(hmi_buffer, HW_DISP_RAW);

                    if (cursorpos != DISP_CURSOR_OFF) /*not expected to happen*/
                        Hmi_Set_Cursor(cursorpos, DISP_CURSOR_ON);

                    return;
                }

                if ((root_update >= 0) && (hmi_search_disp_mode != DISP_NORM))
                    Hw_Disp_Update(hmi_buffer, 67 + root_update, 5 - root_update);

                /*alternate display always counts up, but is only active when
                  it's the computer's turn, and in analysis/mate mode, there is no
                  alternate display.*/
                if (hmi_search_disp_mode != DISP_NORM)
                {
                    disp_ms = raw_ms;
                    /*and rounds downwards*/
                    rounding_time = UT_TIME_ROUND_FLOOR;
                }
            }
        }
        /*time display only until 9:59:59 hours.*/
        Util_Time_To_String(hmi_buffer + 73, disp_ms, UT_NO_LEAD_ZEROS, rounding_time);
    }

    /*check what must be updated in the display and copy it to the cache*/
    for (i = 0, disp_update_char_pos = -1; i < (8 - no_mate_mode); i++)
    {
        char hmi_char, cache_char;

        hmi_char = hmi_buffer[72 + (i + no_mate_mode)];
        cache_char = running_time_cache[i + no_mate_mode];
        if ((hmi_char != cache_char) && (disp_update_char_pos < 0))
            /*get the first time char that is different, if any*/
            disp_update_char_pos = i;

        /*and copy to cache for the next time*/
        running_time_cache[i + no_mate_mode] = hmi_char;
    }

    if (disp_update_char_pos >= 0)
    /*there is something to update in the display, so refresh the characters from
    hmi_buffer[73+disp_update_char_pos] to the end (79)*/
    {
#ifndef PC_VERSION
        /*in the PC version, this would make the whole screen scroll without use*/
        Hw_Disp_Update(hmi_buffer, 72 + (no_mate_mode + disp_update_char_pos), 8 - (no_mate_mode + disp_update_char_pos));
        if (cursorpos != DISP_CURSOR_OFF)
            Hmi_Set_Cursor(cursorpos, DISP_CURSOR_ON);
#endif
    }
}

void Hmi_Erase_Second_Player_Time(void)
{
    Util_Strins(hmi_buffer + 53, "       ");
    Hw_Disp_Update(hmi_buffer, 53, 7);
}

void NEVER_INLINE Hmi_Build_Mating_Screen(int targetdepth)
{
    Util_Strcpy(hmi_buffer, "MTI                    mate search         in progress...   depth: -        0:00");
    Hmi_Set_Bat_Display(hmi_buffer);
    /*target depth*/
    hmi_buffer[67] = ((char) targetdepth) + '0';
    Util_Strcpy(running_time_cache, "    0:00");
    Hw_Disp_Show_All(hmi_buffer, HW_DISP_RAW);
}

void NEVER_INLINE Hmi_Update_Analysis_Screen(int32_t time_passed, int eval, int depth, LINE* pv)
{
    char pv_line_1[30], pv_line_2[30], new_pv_start_move[6];
    int i;

    Hmi_Get_Pv_Info(pv_line_1, pv_line_2, pv);

    Hmi_Update_Search_Disp(hmi_buffer, eval, depth, pv_line_1, pv_line_2);

    /*consider the first five characters of the pv start - it might involve promotion.
    otherwise, the space character recognition will finish the move scan on the 4th character.*/
    for (i = 0; ((i < 5) && (pv_line_1[i] != 0) && (pv_line_1[i] != ' ')); i++)
        new_pv_start_move[i] = pv_line_1[i];
    new_pv_start_move[i] = '\0';

    if (Util_Strcmp(new_pv_start_move, pv_start_move) != 0) /*new pv starting move!*/
    {
        Util_Strcpy(pv_start_move, new_pv_start_move); /*save the new starting move*/

        /*time passed. what's being displayed isn't the time-to-depth,
        but the time to the current best move. so only update the displayed
        time when the pv starting move changes.
        note: this time here isn't the normal running time, which is updated
        separately once per second.*/
        time_passed /= MILLISECONDS;

        if (time_passed <= 999) /*seconds*/
        {
            Util_Itoa(hmi_buffer + 67, time_passed);
            hmi_buffer[70] = 's';
        } else /*minutes*/
        {
            time_passed /= 60;
            Util_Itoa(hmi_buffer + 67, time_passed);
            hmi_buffer[70] = 'm';
        }
    }

    Hw_Disp_Show_All(hmi_buffer, HW_DISP_RAW);
}

void NEVER_INLINE Hmi_Update_Alternate_Screen(int eval, int depth, LINE* pv)
{
    char pv_line_1[30], pv_line_2[30];
    int i, mv_offset;
    char *viewport = (hmi_search_disp_mode == DISP_NORM) ? hmi_alt_buffer : hmi_buffer;

    Hmi_Get_Pv_Info(pv_line_1, pv_line_2, pv);

    Hmi_Update_Search_Disp(viewport, eval, depth, pv_line_1, pv_line_2);

    /*set the current computed move to the start of the PV.
      usually, moves have four letters, so display as "M: e2e4", with one space offset.
      however, promotion moves need to be shown as "M:a7a8q" with no space offset.*/

    /*safe init*/
    viewport[68] = viewport[69] = '-';
    viewport[67] = viewport[70] = viewport[71] = ' ';

    for (i = 0; ((i < 5) && (pv_line_1[i] != ' ') && (pv_line_1[i] != 0)); i++) ;

    if (i == 5) mv_offset = 0; else mv_offset = 1;

    for (i = 0; ((i < 5) && (pv_line_1[i] != ' ') && (pv_line_1[i] != 0)); i++)
        viewport[i + 67 + mv_offset] = pv_line_1[i];

    /*if the alternate display is active, show it*/
    if (hmi_search_disp_mode != DISP_NORM)
        Hw_Disp_Show_All(viewport, HW_DISP_RAW);
}

void NEVER_INLINE Hmi_Build_Analysis_Screen(int black_started_game)
{
    int movenumber;
    char viewport[81];

    Util_Strcpy(viewport, "                         preparing          analysis...                     0:00");
    Hmi_Set_Bat_Display(viewport);
    Hw_Disp_Show_All(viewport, HW_DISP_RAW);
    pv_start_move[0] = '\0'; /*delete the starting move so that any calculated PV will trigger the time update*/
    Util_Strcpy(hmi_buffer, "AN    . E:                                                  D: 0 T:  0s     0:00");
    Hmi_Set_Bat_Display(hmi_buffer);
    if (black_started_game != 0)
        black_started_game = 1;
    Util_Strcpy(running_time_cache, "    0:00");
    movenumber  = (mv_stack_p + 2 + black_started_game) / 2;
    Util_Itoa(hmi_buffer + 3, movenumber);
}

void Hmi_Prepare_Pretty_Print(int black_started_game)
{
    char piece_type;
    enum E_HMI_CHECK check_status;
    int is_capture, is_check, is_mate;

    if (black_started_game != 0)
        black_started_game = 1;

    /*let's prepare the "pretty-print"-stuff from the current move.*/

    /*what kind of piece has moved?*/
    if (move_stack[mv_stack_p].special == PROMOT)
        /*if it was a promotion, then the moving piece was a pawn, though it is now something different on the to-square.*/
        piece_type = WPAWN_CHAR;
    else
        piece_type = Hmi_Get_Piece_Char(board[move_stack[mv_stack_p].move.m.to]->type, UPPERCASE);

    /*was the current move a capture?*/
    is_capture = (move_stack[mv_stack_p].captured->type != 0);

    /*who made the current move, black or white?*/
    if (((mv_stack_p + black_started_game) & 1L) != 0) /*white made last move*/
        check_status = Play_Get_Status(BLACK); /*check whether black is in check, or mated*/
    else /*black was moving*/
        check_status = Play_Get_Status(WHITE); /*check whether white is in check, or mated*/
    is_check = (check_status == HMI_CHECK_STATUS_CHECK);
    is_mate  = (check_status == HMI_CHECK_STATUS_MATE);

    /*store the whole stuff*/
    Hmi_Store_Moving_Piece(mv_stack_p, piece_type, is_capture, is_check, is_mate);
}

/*builds and displays the game screen.*/
void NEVER_INLINE Hmi_Build_Game_Screen(int computerside, int black_started_game,
                                        int game_started_from_0,
                                        enum E_HMI_CONF confirm, GAME_INFO *game_info)
{
    int time_to_disp, plynumber, i, is_capture, is_check, is_mate;
    unsigned int upwards;
    char *viewport;
    char buffer[10], mv_piece_char;
    enum E_UT_ROUND rounding_time;

    /*what kind of time control is applicable for rounding?*/
    switch (CFG_GET_OPT(CFG_GAME_MODE))
    {
    case CFG_GAME_MODE_TPM:
    case CFG_GAME_MODE_ANA:
    case CFG_GAME_MODE_MTI:
    default:
        /*round elapsed time downwards*/
        rounding_time = UT_TIME_ROUND_FLOOR;
        break;
    case CFG_GAME_MODE_GMI:
    case CFG_GAME_MODE_TRN:
        /*round remaining time upwards*/
        rounding_time = UT_TIME_ROUND_CEIL;
        break;
    }

    if (black_started_game != 0)
        black_started_game = 1;

    /*is it the computer's turn?*/
    comp_turn = (((computerside == WHITE) && (((mv_stack_p + black_started_game) & 1U) == 0)) ||
                 ((computerside == BLACK) && (((mv_stack_p + black_started_game) & 1U) != 0)));

    if ((hmi_search_disp_mode == DISP_NORM) || (confirm == HMI_CONFIRM) || (!comp_turn))
    {
        if ((confirm != HMI_CONFIRM) && (comp_turn))
            Hmi_Build_Alternate_Screen(black_started_game, hmi_alt_buffer);
        viewport = hmi_buffer;
    } else
    {
        Hmi_Build_Alternate_Screen(black_started_game, hmi_buffer);
        viewport = hmi_alt_buffer;
    }

    if (mv_stack_p > 0)
        Hmi_Prepare_Pretty_Print(black_started_game);

    /*2nd case may happen if a position has been entered where black is to move.*/
    plynumber = (black_started_game == 0) ? mv_stack_p : mv_stack_p + 1;

    if (mv_stack_p < 3)
    {
        if (game_started_from_0)
            Util_Strcpy(viewport, "new game                                                      1.               ");
        else
            /*may happen after entering a position*/
            Util_Strcpy(viewport, "new pos.                                                      1.               ");

        if (mv_stack_p == 0) /*new game!*/
        {
            /*nothing to do*/
        } else if (mv_stack_p == 1)
        {
            if (black_started_game)
                viewport[62] = '2';
            Util_Strins(viewport + 42, "1.");
            /*convert the move to a string*/
            Util_Convert_Moves(move_stack[mv_stack_p].move, buffer);
            /*get all the surrounding data*/
            mv_piece_char = Hmi_Load_Moving_Piece(mv_stack_p, &is_capture, &is_check, &is_mate);
            /*and convert the move string to long algebraic notation*/
            Hmi_Move_Pretty_Print(buffer, mv_piece_char, is_capture, is_check, is_mate);
            for (i = 0; i < 7; i++)
                viewport[45 + i] = buffer[i];
        } else /*mv_stack_p == 2*/
        {
            viewport[62] = '2';
            if (black_started_game) viewport[42] = '2'; else viewport[42] = '1';
            viewport[43] = '.';
            Util_Convert_Moves(move_stack[mv_stack_p].move, buffer);
            mv_piece_char = Hmi_Load_Moving_Piece(mv_stack_p, &is_capture, &is_check, &is_mate);
            Hmi_Move_Pretty_Print(buffer, mv_piece_char, is_capture, is_check, is_mate);
            for (i = 0; i < 7; i++)
                viewport[45 + i] = buffer[i];

            Util_Strins(viewport + 22, "1.");
            Util_Convert_Moves(move_stack[mv_stack_p - 1].move, buffer);
            mv_piece_char = Hmi_Load_Moving_Piece(mv_stack_p - 1, &is_capture, &is_check, &is_mate);
            Hmi_Move_Pretty_Print(buffer, mv_piece_char, is_capture, is_check, is_mate);
            for (i = 0; i < 7; i++)
                viewport[25 + i] = buffer[i];
        }
    } else
    {
        Util_Strcpy(viewport, "                                                                                ");

        Util_Itoa(viewport + 60, plynumber / 2 + 1);
        viewport[63] = '.';

        Util_Itoa(viewport + 40, (plynumber - 1) / 2 + 1);
        viewport[43] = '.';
        Util_Convert_Moves(move_stack[mv_stack_p].move, buffer);
        mv_piece_char = Hmi_Load_Moving_Piece(mv_stack_p, &is_capture, &is_check, &is_mate);
        Hmi_Move_Pretty_Print(buffer, mv_piece_char, is_capture, is_check, is_mate);
        for (i = 0; i < 7; i++)
            viewport[45 + i] = buffer[i];

        Util_Itoa(viewport + 20, (plynumber - 2) / 2 + 1);
        viewport[23] = '.';
        Util_Convert_Moves(move_stack[mv_stack_p - 1].move, buffer);
        mv_piece_char = Hmi_Load_Moving_Piece(mv_stack_p - 1, &is_capture, &is_check, &is_mate);
        Hmi_Move_Pretty_Print(buffer, mv_piece_char, is_capture, is_check, is_mate);
        for (i = 0; i < 7; i++)
            viewport[25 + i] = buffer[i];

        Util_Itoa(viewport, (plynumber - 3) / 2 + 1);
        viewport[3] = '.';
        Util_Convert_Moves(move_stack[mv_stack_p - 2].move, buffer);
        mv_piece_char = Hmi_Load_Moving_Piece(mv_stack_p - 2, &is_capture, &is_check, &is_mate);
        Hmi_Move_Pretty_Print(buffer, mv_piece_char, is_capture, is_check, is_mate);
        for (i = 0; i < 7; i++)
            viewport[5 + i] = buffer[i];
    }

    Hmi_Set_Bat_Display(viewport);

    /*do the short eval display*/

    if (game_info->valid == EVAL_BOOK)
    {
        /*tell that the last computer move came from the opening book.*/
        viewport[14] = 'b';
    } else
    {
        int sign, current_eval = 0;

        if (game_info->valid == EVAL_MOVE)
            current_eval = game_info->eval;
        else if (game_info->last_valid_eval != NO_RESIGN)
            current_eval = game_info->last_valid_eval;

        if (current_eval < 0)
        {
            sign = -1;
            current_eval = -current_eval;
        } else
            sign = 1;

        if (current_eval >= MATE_CUTOFF)
        {
            viewport[14] = (sign > 0) ? '+' : '-';
            viewport[15] = 'm';
        } else if (current_eval >= WINNING_MARGIN)
        {
            if (sign > 0)
                Util_Strins(viewport + 14, "++");
            else
                Util_Strins(viewport + 14, "--");
        } else if (current_eval >= ADVANTAGE_MARGIN)
            viewport[14] = (sign > 0) ? '+' : '-';
    }

    if (mv_stack_p > 0) /*starting does not count.*/
    {
        time_to_disp = Time_Get_Current_Player(mv_stack_p - 1, &upwards);
        if (upwards == 0) /*only display the time of the player before in game-in or tournament mode*/
        {
            Util_Time_To_String(buffer, time_to_disp, UT_NO_LEAD_ZEROS, rounding_time);
            for (i = 0; i < 7; i++)
                viewport[53 + i] = buffer[i];
        }
    }
    if (comp_turn)
    /* if it is the computer's turn, indicate that it is thinking.*/
        Util_Strins(viewport + HMI_MOVE_POS, "...");

    time_to_disp = Time_Get_Current_Player(mv_stack_p, &upwards);
    if (upwards == 0) /*TRN and GMI mode*/
    {
        /*subtract the time needed so far for the current move*/
        if (mv_stack_p > 0) /*first white move doesn't count*/
            time_to_disp -= Time_Passed();
    } else /*TPM mode - analysis has a separate game screen*/
    {
        /*first white move doesn't count*/
        time_to_disp = (mv_stack_p > 0) ? Time_Passed() : 0;
    }

    Util_Time_To_String(viewport + 73, time_to_disp, UT_NO_LEAD_ZEROS, rounding_time);

    /*use running time cache with offset 1 because in any mode except
      mate searcher, the max time is 9:59:59, and the first char of
      the running time cache is not used.*/
    for (i = 1; i < 8; i ++)
        running_time_cache[i] = hmi_buffer[73 + i - 1]; /*from active buffer*/

    if (confirm == HMI_CONFIRM)
        /*the player shall not enter a move, but just confirm. That is needed when the computer has made a
        move that finishes the game - mate, stalemate, draw by repetion or whatever. If we displayed the
        "game over" dialogue right away, the player would miss the last computer move.*/
    {
        Util_Strins(viewport + 60, "     ");
        Util_Strins(viewport + HMI_MOVE_POS, "<OK>");
    }

    /*display the actual HMI buffer, not the viewport.
      the display now depends on which info mode is being active.*/
    Hw_Disp_Show_All(hmi_buffer, HW_DISP_RAW);

    if (confirm == HMI_CONFIRM) /*gather user feedback until he gives a valid answer.*/
        while ((Hw_Getch(SLEEP_ALLOWED) != KEY_ENT)) ;
}

/*This is a dialogue box, used for general purpose dialogues.
It can be a question, answered with EN or CL for yes or no,
or it can be an information, confirmed with EN.
Maximum allowed length for the message is two lines with 18 characters each,
rest will be truncated.

The time spent in this dialogue is returned in *conf_time. The calling logic
has to take care that this conf_time only refers to this single dialogue.
Since there can be more than one dialogue during one ply, the calling logic will
add up every single dialogue confirmation time to track the total time spent
in dialogues. This time will not be subtracted from the time of whoever's turn
it currently is to move.

The return value of the function can be USER_OK or USER_CANCEL for question
dialogues. For info dialogues, USER_OK is returned.

After the dialogue is over, the viewport will be restored if restore_viewport
is set to HMI_RESTORE.

The calling logic may want this dialogue the restore the screen afterwards, which
is useful if there is only this dialogue to be displayed.
However, if the calling logic intends to display follow-up dialogues, restoring the
screen right after this dialogue and then displaying the next one would cause
annoying screen flickering. In this case, the calling logic must use the
last dialogue in that sequence with automatic restore.
If the user cancels the sequence, the calling logic must Hmi_Restore_Viewport().*/
enum E_HMI_USER Hmi_Conf_Dialogue(const char *line1, const char *line2,
                                  int32_t *conf_time, int32_t timeout,
                                  enum E_HMI_DIALOGUE dialogue_mode, enum E_HMI_REST_MODE restore_viewport)
{
    int32_t conf_start_time;
    enum E_HMI_USER user_answer;

    conf_start_time = Hw_Get_System_Time();

    Hmi_Set_Cursor(0, DISP_CURSOR_OFF);

    user_answer = Hmi_No_Restore_Dialogue(line1, line2, timeout, dialogue_mode);

    /*restore the screen to what it was before the dialogue opened.*/
    if (restore_viewport == HMI_RESTORE)
        Hmi_Restore_Viewport();

    *conf_time  = Hw_Get_System_Time() - conf_start_time; /*so much time was spent in this dialogue.*/

    /*and tell the caller what the user decided (only relevant if this dialogue has been a question).*/
    return(user_answer);
}

/*some fatal internal error happened, we must reboot. tell the user what the
problem is so that he can file a bug report, and then restart.*/
void Hmi_Reboot_Dialogue(const char *line1, const char *line2)
{
    int32_t dummy_conf_time;

    Hw_Set_Speed(SYSTEM_SPEED_LOW, SYSTEM_MODE_USER, CLK_FORCE_AUTO); /*activate keyboard for the dialogue*/
    (void) Hmi_Conf_Dialogue(line1, line2, &dummy_conf_time, HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
    Hw_Sys_Reset();
}

/*sets "BAT" in the upper right corner of a menu if the battery is low.*/
void Hmi_Set_Bat_Display(char *viewport)
{
    if (battery_status & BATTERY_LOW)
        Util_Strins(viewport + 17, "BAT");
    else
        Util_Strins(viewport + 17, "   ");
}

/*shuts down the system because the batteries are too low.*/
void Hmi_Battery_Shutdown(enum E_WHOSE_TURN side_to_move)
{
    int32_t dummy_conf_time;

    /*disable the battery shutdown monitoring in Hw_Getch() because
      we are already in shutdown mode - prevent indirect recursion.*/
    Hw_Set_Bat_Mon_Callback(NULL);

    /*disable the display backlight - might still be active if the shutdown
    is caused by the 1.1V-check in main() and not by actual 1.0V shutdown.*/
    Hw_Sig_Send_Msg(HW_MSG_LED_BACK_INHIB, HW_MSG_NO_DURATION, HW_MSG_PARAM_NONE);

    if (side_to_move != USER_TURN) /*activate keyboard for the dialogue.*/
        Hw_Set_Speed(SYSTEM_SPEED_LOW, SYSTEM_MODE_USER, CLK_FORCE_AUTO);

    Hmi_Signal(HMI_MSG_FAILURE);
    /*tell the user what is going on.*/
    (void) Hmi_Conf_Dialogue(BAT_SHUTDOWN_LINE_1, BAT_SHUTDOWN_LINE_2, &dummy_conf_time,
                             BAT_SHUTDOWN_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);

    Hw_Powerdown_System();
}

/*handle battery status changes. what we do depends on whether the computer is thinking or not.
This is important when the battery changes from low to ok during computer thinking time, which
may happen if the user attaches USB power. In this case, we want to ramp up the speed - but not
if it is the user's turn anyway, where we maintain energy saving speed.

Note: The current hardware design does not support power switching during operation. That would
be easy to add, using a big elko capacitor on the power input, but then attaching the hardware to a
computer USB port instead of a USB wall power adapter would be dangerous for the computer because
USB does not allow big inrush capacitors. Anyway, the hardware design might change, and then it
will be fine if the software already is capable of handling that.

returns 1 if there was some kind of change, 0 else.
In *conf_time, the function returns how much time the user confirmation cost in case there
was a user interaction.
The calling function can evaluate this to take care of the time the user needed for confirming this
status change, which will not be subtracted from anyone's thinking time.*/
int Hmi_Battery_Info(enum E_WHOSE_TURN side_to_move, int32_t *conf_time)
{
    uint32_t bat_state = battery_status;

    COMPILER_BARRIER;

    /*the most frequent case first: battery is fine.*/
    if (LIKELY((bat_state & BATTERY_HIGH) && (battery_confirmation & BATTERY_CONF_HIGH)))
        return(0);

    /*check for emergency shutdown*/
    if (UNLIKELY(bat_state & BATTERY_SHUTDOWN))
    /*prevent deep discharge and battery damage!*/
        Hmi_Battery_Shutdown(side_to_move);

    /*battery is somewhat low, but the user has already acknowledged this.*/
    if (LIKELY((bat_state & BATTERY_LOW) && (battery_confirmation & BATTERY_CONF_LOW)))
        return(0);

    /* ok, at this point, there are two possibilities:
    a) the battery went from OK to low, so we will reduce speed.
    b) the battery went from low to high because the user attached USB power, so we may ramp up.

    however, in time control games, the confirmation time shall not count for anyone.*/

    if (bat_state & BATTERY_LOW)
    {
        if (side_to_move != USER_TURN)
            /*activate keyboard for the dialogue*/
            Hw_Set_Speed(SYSTEM_SPEED_LOW, SYSTEM_MODE_USER, CLK_FORCE_AUTO);

        /*if there is no position display or whatever in the main screen, prepare the BAT display*/
        if ((hmi_buffer[17] == ' ') && (hmi_buffer[18] == ' ') && (hmi_buffer[19] == ' '))
            Util_Strins(hmi_buffer + 17, "BAT");

        Hmi_Signal(HMI_MSG_FAILURE);
        /*tell the user what is going on.*/
        (void) Hmi_Conf_Dialogue(BAT_ANNOUNCE_LINE_1, BAT_ANNOUNCE_LINE_2, conf_time,
                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_RESTORE);

        battery_confirmation = BATTERY_CONF_LOW;

        if (side_to_move != USER_TURN)
            /*if it is the computer's move, switch back to comp mode. don't ramp down the speed
            - while this would conserve the batteries somewhat longer, the system would become unusable.*/
            Hw_Set_Speed(SYSTEM_SPEED_HIGH, SYSTEM_MODE_COMP, CLK_FORCE_AUTO);

        return(1);
    }

    if (bat_state & BATTERY_HIGH)
    {
        if (side_to_move != USER_TURN)
            /*activate keyboard for the dialogue*/
            Hw_Set_Speed(SYSTEM_SPEED_LOW, SYSTEM_MODE_USER, CLK_FORCE_AUTO);

        /*if there is no position display or whatever in the main screen, delete the BAT display*/
        if ((hmi_buffer[17] == 'B') && (hmi_buffer[18] == 'A') && (hmi_buffer[19] == 'T'))
            Util_Strins(hmi_buffer + 17, "   ");

        Hmi_Signal(HMI_MSG_ATT);
        /*tell the user what is going on.*/
        (void) Hmi_Conf_Dialogue("batteries are", "fine again.", conf_time,
                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_RESTORE);

        battery_confirmation = BATTERY_CONF_HIGH;

        if (side_to_move != USER_TURN)
            /*if it is the computer's move, switch back to comp mode and ramp up speed now.*/
            Hw_Set_Speed(SYSTEM_SPEED_HIGH, SYSTEM_MODE_COMP, CLK_FORCE_AUTO);

        return(1);
    }

    return(0);
}

void Hmi_Set_Cursor(int viewport_pos, int active)
{
    if (active != DISP_CURSOR_OFF)
    {
        if ((viewport_pos >= 0) && (viewport_pos < 80))
        {
            int line, col;
            line = viewport_pos / 20;
            col = viewport_pos % 20;
            Hw_Disp_Set_Cursor(line, col, active);
        }
    } else
        Hw_Disp_Set_Cursor(0, 0, DISP_CURSOR_OFF);
}

/***********************************************************************/
/***********************       Easter egg       ************************/
/*********************** Conways's Game of Life ************************/
/***********************************************************************/
/*general note: this implementation is not particularly optimised,
  except for some obvious C steps, in decreasing order of impact:
  - counting the "alive neighbours" in a single function and reusing
    the calculated indices instead of doing 8 completely independent
    lookups.
  - moving loop invariant calculations outside the loop.
  - switching the generation buffers instead of copying them.
  - letting the loops count down (loop counter comparison with zero).

  there are four reasons why more complex algorithmic optimisation
  (like using pad cells for the wrap-arounds) does not make sense and
  just would make the code harder to understand:

  1) the display itself is rather slow, so a fast update rate would just
     result in a flickering display. 60 ms per generation is the lower
     limit.

  2) the actual execution time is 9.5 ms per generation at 8 MHz system
     clock (saving energy), and that already includes the display update
     with some busy waiting.

  3) the delays for a full display update sum up to 6.7 ms, and with the
     code execution, it is 7.7 ms. the display update takes 87% of the
     iteration time for a new generation. even making the "Game of Life"
     calculations four times faster would only speed up the whole
     iteration by 17%.

     the applied optimisations are still useful because the generation
     time must be considerably greater than the display update time, or
     else the lower right cells will not have enough time to stabilise on
     the display. at least, the calculations must not be the bottleneck.

     note that just refreshing changed cells on the display does not work
     here because there is no addressable video RAM. changing the current
     write address in the display is costly, and refreshing the complete
     display is already the fastest way.

  4) the logical "board" is 20 colums by 8 lines, i.e. quite small. the
     20x4 text display doesn't allow more, and even mapping the 20x8
     logical board onto the 20x4 display involves some custom character
     tricks. there are only three free characters for playing around
     because the battery monitoring dialogue can kick in at any time.
*/

/*the cell state definitions have to be exactly that way because the
  neighbour counting relies on that for branch-free counting.*/
#define CELL_DEAD               0U
#define CELL_ALIVE              1U

/*when converting the 20x8 board into the 20x4 boards, two board lines are
  put into one display line, with upper and lower half character.*/
#define CELL_NONE               0x00U
#define CELL_DN                 0x01U
#define CELL_UP                 0x02U
#define CELL_FL                 0x03U

/*the game of life board format*/
#define CGL_COLS                20U
#define CGL_LINES               8U
#define CGL_CELLS               (CGL_COLS * CGL_LINES)

/*the display geometry*/
#define DISP_COLS               20U
#define DISP_LINES              4U

/*deviations on the original "game of life" rules:*/

/*below that amount of alive cells, the "breed mode" starts
  (two or three cells can breed).*/
#define CGL_BREED_MODE_LVL      (CGL_CELLS / 8U)

/*below that amount of alive cells, the "cuddle mode" starts
  (no death because of overpopulation, and one living neighbour
  is sufficient for survival).*/
#define CGL_CUDDLE_MODE_LVL     (CGL_CELLS / 10U)

/*the iteration rate speed steps*/
#define STEPS                   9U
#define DEFAULT_STEP            4U
/*the delay steps in milliseconds. evenly distanced in terms of logarithmic
  frequency, by a factor of sqrt(2) in each step.*/
static const int32_t delay_steps[STEPS] = {1000L, 710L, 500L, 350L, 250L, 180L, 130L, 90L, 60L};

/*usually, the undo/redo keys change the speed of the animation
  - except in case the game has ended where all keys can abort the
  game before the 5s timeout.*/
#define HMI_GOL_SPEED_KEEP      0U
#define HMI_GOL_SPEED_CHANGE    1U

/*the timeout (milliseconds) at "game over", before entering the CT800 game
  screen.*/
#define END_PAUSE               5000L

/*waits for timeout (returns 0) or aborts if a key is pressed (returns 1).
note that the Hw_Getch() call also enables the battery monitoring.*/
static uint32_t Hmi_Wait_Keypress(int32_t end_time, uint32_t *delay_index, uint32_t allow_speeds)
{
    while (Hw_Get_System_Time() < end_time)
    {
#ifndef PC_VERSION
        enum E_KEY user_key;

        Hw_Trigger_Watchdog();

        /*pressing keys other than light aborts the waiting. not used in the
          PC version because Hw_Getch() is a blocking call on the PC.*/
        user_key = Hw_Getch(SLEEP_ALLOWED);

        /*but undo/redo can lower or raise the animation speed.*/
        if (user_key == KEY_MENU_MINUS)
        {
            if ((allow_speeds != HMI_GOL_SPEED_KEEP) && ((*delay_index) > 0U))
            {
                /*reduce speed*/
                Hmi_Signal(HMI_MSG_OK);
                (*delay_index)--;
                return(0U);
            }
        } else if (user_key == KEY_MENU_PLUS)
        {
            if ((allow_speeds != HMI_GOL_SPEED_KEEP) && ((*delay_index) < (STEPS-1U)))
            {
                /*raise speed*/
                Hmi_Signal(HMI_MSG_OK);
                (*delay_index)++;
                return(0U);
            }
        } else if ((user_key == KEY_GO) || (user_key == KEY_CL) || (user_key == KEY_ENT))
        {
            /*user keypress finishes the delay*/
            Hmi_Signal(HMI_MSG_OK);
            return(1U);
        }
#endif
    }
    /*no abort*/
    return(0U);
}

/*gets the number of living neighbour cells in a toroid way.*/
static uint32_t Hmi_Game_Of_Life_Neighbours(const uint8_t *cgl_board, uint32_t cell)
{
    uint32_t ret, line_xCOLS, line_xCOLS_minus, line_xCOLS_plus, col, col_minus, col_plus;

    /*convert the cell to line/column*/
    col = cell % CGL_COLS;

    /*we don't need the line itself, just the array index*/
    line_xCOLS = cell - col;

    /*do the wrap-arounds*/
    if (line_xCOLS >= CGL_COLS)
        line_xCOLS_minus = line_xCOLS - CGL_COLS;
    else
        line_xCOLS_minus = line_xCOLS + (CGL_CELLS - CGL_COLS);

    if (line_xCOLS < (CGL_CELLS - CGL_COLS))
        line_xCOLS_plus = line_xCOLS + CGL_COLS;
    else
        line_xCOLS_plus = line_xCOLS - (CGL_CELLS - CGL_COLS);

    if (col > 0U)
        col_minus = col - 1U;
    else
        col_minus = (CGL_COLS - 1U);

    if (col < (CGL_COLS - 1U))
        col_plus = col + 1U;
    else
        col_plus = 0U;

    /*sum up the living neighbours*/
    ret  = cgl_board[line_xCOLS_minus + col_minus];
    ret += cgl_board[line_xCOLS_minus + col      ];
    ret += cgl_board[line_xCOLS_minus + col_plus ];
    ret += cgl_board[line_xCOLS       + col_minus];
    ret += cgl_board[line_xCOLS       + col_plus ];
    ret += cgl_board[line_xCOLS_plus  + col_minus];
    ret += cgl_board[line_xCOLS_plus  + col      ];
    ret += cgl_board[line_xCOLS_plus  + col_plus ];

    return(ret);
}

/*converts the CGL board into 20x4 display viewport.*/
static void Hmi_Disp_Cgl_Board(const uint8_t *cgl_board)
{
    uint32_t line;
    char gamescreen[80];

    /*line and col refer to the display screen, not to the board.
      the board is 20x8, the display 20x4, and two board lines
      are mapped into one display line using upper/lower half
      boxes.*/

    line = DISP_LINES;
    do { /*loop over "line"*/
        uint32_t col, line_xCOLS, line_xCOLS_x2, line_xCOLS_x2_pCOLS;

        line--;

        /*hoist some calculations out of the inner loop*/
        line_xCOLS          = line * CGL_COLS;
        line_xCOLS_x2       = line_xCOLS << 1U; /*times 2*/
        line_xCOLS_x2_pCOLS = line_xCOLS_x2 + CGL_COLS;

        col = DISP_COLS;
        do { /*loop over "col"*/
            uint32_t disp_cell_state;
            char cell_char;

            col--;
            disp_cell_state = CELL_NONE;

            if (cgl_board[line_xCOLS_x2 + col] != CELL_DEAD)
                disp_cell_state |= CELL_UP;
            if (cgl_board[line_xCOLS_x2_pCOLS + col] != CELL_DEAD)
                disp_cell_state |= CELL_DN;

            switch (disp_cell_state)
            {
                case CELL_UP:
#ifndef PC_VERSION
                    cell_char = HD_DISP_BOX_UP;
#else
                    cell_char = 'P';
#endif
                    break;
                case CELL_DN:
#ifndef PC_VERSION
                    cell_char = HD_DISP_BOX_DN;
#else
                    cell_char = 'b';
#endif
                    break;
                case (CELL_UP | CELL_DN):
#ifndef PC_VERSION
                    cell_char = HD_DISP_BOX_FL;
#else
                    cell_char = 'B';
#endif
                    break;
                case CELL_NONE:
                default:
                    cell_char = ' ';
                    break;
            }

            gamescreen[line_xCOLS + col] = cell_char;
        } while (col != 0U);
    } while (line != 0U);

    /*the usual video trick of only updating the cells that have changed
    does not work here because there is no addressable video RAM. refreshing
    the whole display is the fastest way here.*/
    Hw_Disp_Show_All(gamescreen, HW_DISP_RAW);
}

/*the Easter egg, Conway's game of life
  (slightly modified with a "breed mode").*/
void NEVER_INLINE Hmi_Game_Of_Life(void)
{
    uint32_t cell, delay_index;
    int32_t  next_time;
    /*using pointers for buffer switching*/
    uint8_t *old_cgl_board, *new_cgl_board, *tmp_ptr;
    uint8_t  cgl_board1[CGL_CELLS], cgl_board2[CGL_CELLS];

    /*the advantage of waiting for the end time to be reached is that
    the timing will always be fix, no matter how long the game of life
    loop takes, provided that it is less than the delay. The calculations
    are easy, but the display output involves some busy waiting in the
    hardware driver layer.
    if the wait routine were called at the end of the loop, then the
    loop time would add up to the generation time.*/
    next_time = Hw_Get_System_Time() + delay_steps[DEFAULT_STEP];
    delay_index = DEFAULT_STEP;

    /*configure the character set with the half-filled boxes*/
    Hw_Disp_Set_Charset(HW_CHARSET_CGOL);

    /*set up the buffer pointers*/
    old_cgl_board = cgl_board1;
    new_cgl_board = cgl_board2;

    /*init - not time critical*/
    for (cell = 0U; cell < CGL_CELLS; cell++)
    {
        /*set about 33% of the cells to "life"*/
        if (Hw_Rand() % 3U == 0U)
            old_cgl_board[cell] = CELL_ALIVE;
        else
            old_cgl_board[cell] = CELL_DEAD;
    }

    Hmi_Disp_Cgl_Board(old_cgl_board);

    /*the wait routine returns 1 if a key has been pressed*/
    while (Hmi_Wait_Keypress(next_time, &delay_index, HMI_GOL_SPEED_CHANGE) == 0)
    {
        uint32_t game_over, alive_cells, breed_mode, cuddle_mode;

#ifndef PC_VERSION
        /*once per week: reset system time (on ARM only).
          crazy people might try to run this Easter egg for more than 24 days
          when the system clock overflows.*/
        if (Hw_Get_System_Time() >= (MILLISECONDS * 60L * 60L * 24L * 7L))
            Hw_Set_System_Time(0);
#endif

        /*schedule the start of the iteration after the current one*/
        next_time = Hw_Get_System_Time() + delay_steps[delay_index];

        /*determine alive status*/
        alive_cells = 0U;
        cell = CGL_CELLS;
        do {
            cell--;
            alive_cells += old_cgl_board[cell];
        } while (cell != 0U);

        /*modification on the original rules:
          when going extinct, the cells breed more desperately.*/
        if (alive_cells < CGL_BREED_MODE_LVL)
        {
            /*below that level, local overpopulation isn't an issue,
              and one neighbour is sufficient for survival.*/
            cuddle_mode = (alive_cells < CGL_CUDDLE_MODE_LVL) ? 1U : 0U;
            breed_mode = 1U;
        } else
        {
            breed_mode  = 0U;
            cuddle_mode = 0U;
        }

        /*get the next generation status*/
        game_over = 1U;
        cell = CGL_CELLS;
        do { /*loop over "cell"*/
            uint32_t living_neighbours;

            cell--;

            living_neighbours = Hmi_Game_Of_Life_Neighbours(old_cgl_board, cell);

            if (old_cgl_board[cell] == CELL_DEAD) /*cell born?*/
            {
                if ((living_neighbours == 3U) || ((breed_mode) && (living_neighbours >= 2U)))
                {
                    new_cgl_board[cell] = CELL_ALIVE;
                    game_over = 0U; /*a cell state has changed from dead to alive*/
                } else
                    new_cgl_board[cell] = CELL_DEAD;
            } else /*cell survivors?*/
            {
                if ((living_neighbours == 2U) || (living_neighbours == 3U) || ((cuddle_mode) && (living_neighbours > 0U)))
                    new_cgl_board[cell] = CELL_ALIVE;
                else
                {
                    new_cgl_board[cell] = CELL_DEAD;
                    game_over = 0U; /*a cell state has changed from alive to dead*/
                }
            }
        } while (cell != 0U);

        if (game_over) /*no change?*/
        {
            Hmi_Signal(HMI_MSG_OK);
            Hmi_Signal(HMI_MSG_ATT);
            Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_KEY, HW_MSG_PARAM_BACK_CONF);
            /*wait for 5 seconds or until a key is pressed*/
            next_time = Hw_Get_System_Time() + END_PAUSE;
            (void) Hmi_Wait_Keypress(next_time, &delay_index, HMI_GOL_SPEED_KEEP);
            /*leave, but still restore the character set
              at the end of the function.*/
            break;
        }

        /*display the updated screen*/
        Hmi_Disp_Cgl_Board(new_cgl_board);

        /*switching the buffers avoids a buffer copy*/
        tmp_ptr = new_cgl_board;
        new_cgl_board = old_cgl_board;
        old_cgl_board = tmp_ptr;
    }

    /*restore the regular character set*/
    Hw_Disp_Set_Charset(HW_CHARSET_NORM);
}
/***********************************************************************/
/************************** end of Easter egg **************************/
/***********************************************************************/


/*show the start screen:
  tell the user the version ID of the system and whether we're running on
  default or custom config. a very long power outage will destroy the
  configuration, and the user should know whether he has to reconfigure the
  device in case he doesn't like the default config.*/
void NEVER_INLINE Hmi_Show_Start_Screen(void)
{
    int i;
    char start_screen[81];

    Util_Strcpy(running_time_cache, "        ");
    Util_Memzero(moving_piece, sizeof(moving_piece));

    /*clear the viewport*/
    Util_Strcpy(hmi_buffer,"                                                                                ");
    Util_Strcpy(start_screen, "+-------WAIT-------+|"VERSION_INFO_STARTUP"|| settings loaded. |+------------------+");
    if (hw_config == CFG_DEFAULT)
        Util_Strins(start_screen + 41, " defaults loaded. ");

    Hw_Trigger_Watchdog();
    Hw_Disp_Show_All(start_screen, HW_DISP_DIALOGUE);
    Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_KEY, HW_MSG_PARAM_BACK_CONF);
    for (i = 0; i < 8; i++)
    {
        Time_Delay(BOOT_SCREEN_DELAY / 8, SLEEP_FORBIDDEN);
        Hw_Trigger_Watchdog();
    }
}

/*shows a waiting screen with a progress bar. the necessary delay is not
much, but it is distracting enough that there has to be some activity for
the user to know things are fine.*/
void NEVER_INLINE Hmi_Show_Bat_Wait_Screen(int32_t wait_time)
{
    char wait_screen[81];
    int i;

    /*when working with delays, better trigger the watchdog.*/
    Hw_Trigger_Watchdog();

    Util_Strcpy(wait_screen,"+-------WAIT-------+|  battery check:  || |              | |+------------------+");

#ifndef PC_VERSION
    /*fill in a top/bottom box on ARM*/
    wait_screen[42] = HD_DISP_LEFT;
    for (i = 43; i < 57; i++)
       wait_screen[i] = HD_DISP_BOX;
    wait_screen[57] = HD_DISP_RIGHT;
#endif

    Hw_Disp_Show_All(wait_screen, HW_DISP_DIALOGUE);

    /*the progress bar has only 14 entries, but still use 16 time slices:
      the display is not that fast so that the last two squares need slightly
      more time to stabilise.*/
    wait_time /= 16l;

    for (i = 43; i < 57; i++)
    {
        Time_Delay(wait_time, SLEEP_FORBIDDEN); /*no sleep, battery load test*/
#ifndef PC_VERSION
        wait_screen[i] = HD_DISP_BOX_FL; /*a filled rectangle character on ARM*/
        Hw_Disp_Update(wait_screen, i, 1);
#endif
        Hw_Trigger_Watchdog();
    }
    /*wait timeslices 15 and 16*/
    Time_Delay(wait_time * 2, SLEEP_FORBIDDEN); /*no sleep, battery load test*/
    Hw_Trigger_Watchdog();
}

/*enable the battery shutdown in Hw_Getch().*/
void Hmi_Setup_System(void)
{
    Hw_Set_Bat_Mon_Callback(&Hmi_Battery_Shutdown);
}

/*fill in the module details for saving.*/
void Hmi_Save_Status(BACKUP_GAME *ptr)
{
    Util_Memcpy(ptr->moving_piece, moving_piece, sizeof(moving_piece));
    if (hmi_search_disp_mode != DISP_NORM)
        ptr->blackstart_searchdisp |= SAVE_SEARCH_DISP; /*zero init in Hw_Save_Game()*/
}

/*load the module details from saved game.*/
void Hmi_Load_Status(BACKUP_GAME *ptr)
{
    Util_Memcpy(moving_piece, ptr->moving_piece, sizeof(moving_piece));
    hmi_search_disp_mode = (ptr->blackstart_searchdisp & SAVE_SEARCH_DISP) ? DISP_ALT : DISP_NORM;
}

/*signal various frequent logical HMI states.*/
void Hmi_Signal(enum E_HMI_MSG msg_id)
{
    switch (msg_id)
    {
    case HMI_MSG_OK:
        Hw_Sig_Send_Msg(HW_MSG_LED_GREEN_ON, LED_SHORT, HW_MSG_PARAM_NONE);
        break;
    case HMI_MSG_ERROR:
        Hw_Sig_Send_Msg(HW_MSG_LED_RED_ON, LED_SHORT, HW_MSG_PARAM_NONE);
        Hw_Sig_Send_Msg(HW_MSG_BEEP_ON, BEEP_SHORT, HW_MSG_PARAM_ERROR);
        break;
    case HMI_MSG_FAILURE:
        Hw_Sig_Send_Msg(HW_MSG_LED_RED_ON, LED_LONG, HW_MSG_PARAM_NONE);
        Hw_Sig_Send_Msg(HW_MSG_BEEP_ON, BEEP_SHORT, HW_MSG_PARAM_ERROR);
        break;
    case HMI_MSG_ATT:
        Hw_Sig_Send_Msg(HW_MSG_BEEP_ON, BEEP_SHORT, HW_MSG_PARAM_BEEP);
        break;
    case HMI_MSG_MOVE:
        Hw_Sig_Send_Msg(HW_MSG_BEEP_ON, BEEP_MOVE, HW_MSG_PARAM_MOVE);
        break;
    case HMI_MSG_TEST:
        Hw_Sig_Send_Msg(HW_MSG_LED_GREEN_ON, LED_BOOT, HW_MSG_PARAM_NONE);
        Hw_Sig_Send_Msg(HW_MSG_LED_RED_ON, LED_BOOT, HW_MSG_PARAM_NONE);
        Hw_Sig_Send_Msg(HW_MSG_BEEP_ON, BEEP_SHORT, HW_MSG_PARAM_BEEP);
        break;
    default:
        break;
    }
}
