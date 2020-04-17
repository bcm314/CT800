/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2015-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of the CT800 (position editor).
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

/*this file implements the embedded position editor in form of a
simple, though lengthy state machine.*/

#include <stdint.h>
#include <stddef.h>
#include "ctdefs.h"
#include "confdefs.h"
#include "hmi.h"
#include "posedit.h"
#include "hardware.h"
#include "util.h"

/*--------- no external variables ---------*/

/*--------- external functions ------------*/

/*external functions - for the position editor*/
enum E_POS_STATE NEVER_INLINE Play_Load_Position(STARTING_POS *load_starting_pos, int black_starts, int pos_wking, int pos_bking);

/*--------- module static functions ------------*/

static int Pos_Square_To_Viewport(int square, int white_bottom)
{
    if ((square >= BP_A1) && (square <= BP_H1))
        return((white_bottom) ? (square - BP_A1 + 60) : (19 + BP_A1 - square));

    if ((square >= BP_A2) && (square <= BP_H2))
        return((white_bottom) ? (square - BP_A2 + 40) : (39 + BP_A2 - square));

    if ((square >= BP_A3) && (square <= BP_H3))
        return((white_bottom) ? (square - BP_A3 + 20) : (59 + BP_A3 - square));

    if ((square >= BP_A4) && (square <= BP_H4))
        return((white_bottom) ? (square - BP_A4 +  0) : (79 + BP_A4 - square));

    if ((square >= BP_A5) && (square <= BP_H5))
        return((white_bottom) ? (square - BP_A5 + 72) : ( 7 + BP_A5 - square));

    if ((square >= BP_A6) && (square <= BP_H6))
        return((white_bottom) ? (square - BP_A6 + 52) : (27 + BP_A6 - square));

    if ((square >= BP_A7) && (square <= BP_H7))
        return((white_bottom) ? (square - BP_A7 + 32) : (47 + BP_A7 - square));

    if ((square >= BP_A8) && (square <= BP_H8))
        return((white_bottom) ? (square - BP_A8 + 12) : (67 + BP_A8 - square));

    return(0); /*should not happen*/
}

static void Pos_Get_Board(char *viewport, int white_bottom, const STARTING_POS *edit_pos)
{
    int i;

    Hmi_Init_Board(viewport, white_bottom); /*get the empty board display*/
    for (i = 0; i < 64; i++)
    {
        if (edit_pos->board[i] != NO_PIECE) /*if the piece is on the board*/
            viewport[Pos_Square_To_Viewport(i, white_bottom)] = Hmi_Get_Piece_Char(edit_pos->board[i], MIXEDCASE);
    }
    /*show possible en passant square as 'e'*/
    if (((edit_pos->epsquare) < BP_NOSQUARE) && (edit_pos->board[edit_pos->epsquare] == NO_PIECE))
        viewport[Pos_Square_To_Viewport(edit_pos->epsquare, white_bottom)] = 'e';
}

static void Pos_Get_Piecelist(char *board_viewport, const STARTING_POS *edit_pos)
{
    int queens, rooks, lightbishops, darkbishops, knights, pawns, square, material;

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
    for (square = 0; square < 64; square++)
    {
        int file, rank;

        switch (edit_pos->board[square]) /*king is ignored, that's already filled in*/
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
            file = square % 8;
            rank = square / 8;
            if (((file+rank) % 2) != 0)
                lightbishops++;
            else
                darkbishops++;
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
    for (square = 0; square < 64; square++)
    {
        int file, rank;

        switch (edit_pos->board[square]) /*king is ignored, that's already filled in*/
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
            file = square % 8;
            rank = square / 8;
            if (((file+rank) % 2) != 0)
                lightbishops++;
            else
                darkbishops++;
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

static enum E_HMI_MENU Pos_Disp_Edit_Board(const STARTING_POS *edit_pos)
{
    enum E_HMI_USER user_answer;
    int disp_state = 0, white_bottom = 1;
    char board_viewport[81];

    Hmi_Set_Cursor(0,DISP_CURSOR_OFF);

    /*disp_state is a state machine.
    state 0: display the board.
    state 1: display the board in reverse.
    state 2: display the piece list.*/

    do {
        switch (disp_state) /* state machine for the display*/
        {
        case 0:
        default:
            Pos_Get_Board(board_viewport, white_bottom, edit_pos);
            break;
        case 1:
            Pos_Get_Board(board_viewport, !white_bottom, edit_pos);
            break;
        case 2:
            Pos_Get_Piecelist(board_viewport, edit_pos);
            break;
        }

        Hw_Disp_Show_All(board_viewport, HW_DISP_RAW);

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
            if (user_key == KEY_MENU)
                user_answer = HMI_USER_DISP;

        } while (user_answer == HMI_USER_INVALID);

        /*continue the state machine*/
        if (disp_state >= 2)
            disp_state = 0;
        else
            disp_state++;
    } while (user_answer == HMI_USER_FLIP);

    if (user_answer == HMI_USER_DISP)
        return (HMI_MENU_LEAVE);
    else
        return(HMI_MENU_OK);
}

static int Pos_Get_Piece_Type_Line(char *buffer, int piece_type, int allowed_items, const STARTING_POS *edit_pos)
{
    int square, file, rank, cp, treated_item, entered_items = 0;

    if (piece_type == POSEDIT_EPSQ_TYPE)
    /*the en passant square isn't actally a piece, but it is entered like one*/
    {
        square = edit_pos->epsquare;
        if ((square < BP_NOSQUARE) && (edit_pos->board[square] == NO_PIECE))
        {
            buffer[0] = (square % 8) + 'a';
            buffer[1] = (square / 8) + '1';
            buffer[2] = '.'; /*maximum en passant square is one anyway.*/
            return(1);
        } else
            return(0);
    }

    /*how many pieces of that type are already there?*/
    for (square = 0; square < 64; square++)
        if (edit_pos->board[square] == piece_type)
            entered_items++;

    /*if there are no pieces of that type, we're done.*/
    if (entered_items == 0)
        return(0);

    treated_item = 0;
    cp = 0;

    for (file = 0; file < 8; file++)
    {
        for (rank = 0; rank < 8; rank++)
        {
            square = file + (rank << 3);
            if (edit_pos->board[square] == piece_type)
            {
                char print_char;

                /*get the algebraic coordinates.*/
                print_char = square % 8;
                print_char += 'a';
                buffer[cp++] = print_char;
                print_char = square / 8;
                print_char += '1';
                buffer[cp++] = print_char;

                treated_item++;

                if ((treated_item < entered_items) || (entered_items < allowed_items))
                {
                    buffer[cp++] = ',';
                    buffer[cp++] = ' ';
                } else
                {
                    /*if the last piece of that type is already the maximum that may be entered,
                    taking promotion into account, then display the final '.'.*/
                    buffer[cp] = '.';
                }
            }
        }
    }

    return(entered_items);
}

static int Pos_Count_Promoted_Pieces(enum E_COLOUR colour, const STARTING_POS *edit_pos)
{
    int queens = 0, rooks = 0, bishops = 0, knights= 0, prompieces = 0, square;

    if (colour == WHITE)
    {
        /*count the total white promotable  pieces.*/
        for (square = 0; square < 64; square++)
        {
            switch (edit_pos->board[square])
            {
            case WQUEEN:
                queens++;
                break;
            case WROOK:
                rooks++;
                break;
            case WBISHOP:
                bishops++;
                break;
            case WKNIGHT:
                knights++;
                break;
            default:
                break;
            }
        }
    } else if (colour == BLACK)
    {
        /*count the total black promotable  pieces.*/
        for (square = 0; square < 64; square++)
        {
            switch (edit_pos->board[square])
            {
            case BQUEEN:
                queens++;
                break;
            case BROOK:
                rooks++;
                break;
            case BBISHOP:
                bishops++;
                break;
            case BKNIGHT:
                knights++;
                break;
            default:
                break;
            }
        }
    }

    /*add up the number of pieces that surpasses the one from the initial position.*/
    if (queens > 1)
        prompieces += queens - 1;
    if (rooks > 2)
        prompieces += rooks - 2;
    if (bishops > 2)
        prompieces += bishops - 2;
    if (knights > 2)
        prompieces += knights - 2;

    return(prompieces);
}

static int Pos_Count_Pawns(enum E_COLOUR colour, const STARTING_POS *edit_pos)
{
    int pawns = 0, square;

    if (colour == WHITE)
    {
        /*count the total white pawns.*/
        for (square = 0; square < 64; square++)
        {
            if (edit_pos->board[square] == WPAWN)
                pawns++;
        }
    } else if (colour == BLACK)
    {
        /*count the total black pawns.*/
        for (square = 0; square < 64; square++)
        {
            if (edit_pos->board[square] == BPAWN)
                pawns++;
        }
    }

    return(pawns);
}

static int Pos_Black_King_Absent(const STARTING_POS *edit_pos)
{
    int square;

    for (square = 0; square < 64; square++)
        if (edit_pos->board[square] == BKING)
            return(0);

    return(1);
}

static void Pos_Add_Line(const char *menu_buffer, int piece_type, int piece_number, STARTING_POS *edit_pos)
{
    int i, square;

    if (piece_type == POSEDIT_EPSQ_TYPE)
    /*this isn't an actual piece, it is the en passant square.*/
    {
        if (piece_number > 0)
        /*any EP square entered?*/
        {
            square =    (menu_buffer[40] - 'a');
            square += 8*(menu_buffer[41] - '1');
            edit_pos->epsquare = square;
        } else
            edit_pos->epsquare = BP_NOSQUARE;
        return;
    }
    /*first delete lingering pieces of that type (from undo/redo)*/
    for (square = 0; square < 64; square++)
    {
        if (edit_pos->board[square] == piece_type)
            edit_pos->board[square] = NO_PIECE;
    }
    /*then enter the pieces*/
    for (i=0; i<piece_number; i++)
    {
        square =    (menu_buffer[40 + 4*i] - 'a');
        square += 8*(menu_buffer[40 + 4*i + 1] - '1');
        edit_pos->board[square] = piece_type;
    }
}

/*is the supplied en passant square valid?
- must be 0-63.
- must be free.
- must be on the 3rd/6th rank, depending on the side to move.
- must have a pawn before it.
- must have a free square behind it.
- must touch an enemy pawn diagonally.*/
static int Pos_EPSQ_Valid(int black_starts, uint32_t epsq, const STARTING_POS *edit_pos)
{
    int rank;

    /*square out of range*/
    if (epsq >= BP_NOSQUARE)
        return(0);

    /*square not free*/
    if (edit_pos->board[epsq] != NO_PIECE)
        return(0);

    rank = (epsq / 8) + 1;

    /*basic sanity check, must be on the 3rd / 6th rank.*/
    if (((!black_starts) && (rank != 6)) ||
        ((black_starts) && (rank != 3)))
        return(0);

    if (black_starts)
    {
        int file;

        /*past white pawn double step possible?*/
        if (edit_pos->board[epsq-8] != NO_PIECE)
            return(0);
        if (edit_pos->board[epsq+8] != WPAWN)
            return(0);
        file = (epsq % 8) + 1;
        if (file != 8) /*H file has no right neighbour*/
            if (edit_pos->board[epsq+9] == BPAWN)
                return(1);
        if (file != 1) /*A file has no left neighbour*/
            if (edit_pos->board[epsq+7] == BPAWN)
                return(1);
    } else /*white to move*/
    {
        int file;

        /*past black pawn double step possible?*/
        if (edit_pos->board[epsq+8] != NO_PIECE)
            return(0);
        if (edit_pos->board[epsq-8] != BPAWN)
            return(0);
        file = (epsq % 8) + 1;
        if (file != 8) /*H file has no right neighbour*/
            if (edit_pos->board[epsq-7] == WPAWN)
                return(1);
        if (file != 1) /*A file has no left neighbour*/
            if (edit_pos->board[epsq-9] == WPAWN)
                return(1);
    }
    return(0);
}

/*does any potential en passant square exist for the user to select?*/
static int Pos_EPSQ_Exist(int black_starts, const STARTING_POS *edit_pos)
{
    unsigned int i;
    int res = 0;

    if (black_starts)
    {
        for (i = BP_A3; i <= BP_H3; i++)
            res += Pos_EPSQ_Valid(black_starts, i, edit_pos);
    } else
    {
        for (i = BP_A6; i <= BP_H6; i++)
            res += Pos_EPSQ_Valid(black_starts, i, edit_pos);
    }

    return(res);
}

static int Pos_Readline(char *menu_buffer, int allowed_items, int requested_items, int piece_type, const STARTING_POS *edit_pos)
{
    int entered_items;
    int input_finished = 0;
    int input_file_enter = 1; /*coordinates start with the file letter*/

    /*how many items are already there?*/
    entered_items = Pos_Get_Piece_Type_Line(menu_buffer + 40, piece_type, allowed_items, edit_pos);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*can more items be entered?*/
    if (entered_items < allowed_items)
        Hmi_Set_Cursor(40 + entered_items*4, DISP_CURSOR_ON);
    else
        Hmi_Set_Cursor(0, DISP_CURSOR_OFF);

    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        /*virtual key for cancelling a started item only*/
        if ((user_key == KEY_V_FCL) && (!input_file_enter))
            user_key = KEY_CL;

        if ((user_key >= KEY_A1) && (user_key <= KEY_H8))
        {
            /*allow only additional entries when the maximum number has not yet been reached.*/
            if (entered_items < allowed_items)
            {
                if (input_file_enter)
                {
                    /*the entered character is a file a-h*/
                    menu_buffer[40 + entered_items*4] = Util_Key_To_Char(user_key);
                    Hw_Disp_Update(menu_buffer, 40 + entered_items*4, 1);
                    Hmi_Set_Cursor(40 + entered_items*4 + 1, DISP_CURSOR_ON);
                    input_file_enter = 0;
                } else if (((piece_type != WPAWN) && (piece_type != BPAWN)) ||
                           ((user_key >= KEY_B2) && (user_key <= KEY_G7))) /*pawns can only be on rank 2-7*/
                {
                    /*the entered character is a rank 1-8*/
                    int i, is_present;
                    char file_char, rank_char;

                    rank_char = Util_Key_To_Digit(user_key);
                    file_char = menu_buffer[40 + entered_items*4];

                    /*has this item already been entered at this square?*/
                    for (i = 0, is_present = 0; i < entered_items; i++)
                    {
                        if ((menu_buffer[40 + i*4    ] == file_char) &&
                            (menu_buffer[40 + i*4 + 1] == rank_char))
                        {
                            /*found same entry before*/
                            is_present = 1;
                            break;
                        }
                    }

                    if (is_present) /*double entry*/
                    {
                        /*remove new item entry*/
                        menu_buffer[40 + entered_items*4] = ' ';
                        Hw_Disp_Update(menu_buffer, 40 + entered_items*4, 1);
                        Hmi_Set_Cursor(40 + entered_items*4, DISP_CURSOR_ON);
                    } else /*item is new*/
                    {
                        /*add item*/
                        menu_buffer[40 + entered_items*4 + 1] = rank_char;
                        if (entered_items < allowed_items - 1) /*can more items follow?*/
                        {
                            menu_buffer[40 + entered_items*4 + 2] = ',';
                            menu_buffer[40 + entered_items*4 + 3] = ' ';
                            Hw_Disp_Update(menu_buffer, 40 + entered_items*4 + 1, 3);
                            Hmi_Set_Cursor(40 + entered_items*4 + 4, DISP_CURSOR_ON);
                        } else /*last allowed item*/
                        {
                            menu_buffer[40 + entered_items*4 + 2] = '.';
                            Hw_Disp_Update(menu_buffer, 40 + entered_items*4 + 1, 2);
                            Hmi_Set_Cursor(0, DISP_CURSOR_OFF);
                        }
                        entered_items++;
                    }

                    /*expect file letter as next input*/
                    input_file_enter = 1;
                }
            }
        } else if (user_key == KEY_CL)  /*cancel last entered item*/
        {
            if (input_file_enter == 1) /*user was about to enter a new item*/
            {
                if (entered_items > 0) /*so cancel the last item if there is one*/
                {
                    entered_items--;
                    menu_buffer[40 + entered_items*4    ] = ' ';
                    menu_buffer[40 + entered_items*4 + 1] = ' ';
                    menu_buffer[40 + entered_items*4 + 2] = ' ';
                    menu_buffer[40 + entered_items*4 + 3] = ' ';
                    Hw_Disp_Update(menu_buffer, 40 + entered_items*4, 4);
                    Hmi_Set_Cursor(40 + entered_items*4, DISP_CURSOR_ON);
                }
            } else /*user was entering an item, so cancel this item*/
            {
                input_file_enter = 1;
                menu_buffer[40 + entered_items*4] = ' ';
                Hw_Disp_Update(menu_buffer, 40 + entered_items*4, 1);
                Hmi_Set_Cursor(40 + entered_items*4, DISP_CURSOR_ON);
            }
        } else if (user_key == KEY_UNDO)
        {
            /*jump back to the previous piece type - except if the user is entering
            the white king position, which is the first piece type.*/
            if (piece_type != WKING)
            {
                entered_items = POS_PCS_UNDO;
                input_finished = 1;
            }
        } else if (user_key == KEY_REDO)
        {
            /*jump forward to the next piece type - except if the user is entering
            the black pawn position, which is the last piece type.*/
            if (piece_type != BPAWN)
            {
                entered_items = POS_PCS_REDO;
                input_finished = 1;
            }
        } else if (user_key == KEY_ENT)
        {
            if ((input_file_enter == 1) /*don't allow finishing when still entering, would be a user typo anyway*/
                    && (entered_items >= requested_items))
                input_finished = 1;
        } else if (user_key == KEY_MENU)
        {
            entered_items = POS_PCS_LEAVE;
            input_finished = 1;
        }
    } while (input_finished == 0);

    Hmi_Set_Cursor(0, DISP_CURSOR_OFF);
    return (entered_items);
}


/*--------- global functions ------------*/


/*the position editor is a state machine inside a big loop
and usually proceeds to the next state, but may go back to the previous one.*/
enum E_HMI_MENU NEVER_INLINE Pos_Editor(void)
{
    STARTING_POS edit_pos;
    char menu_buffer[81], reject_reason[20];
    int32_t dummy_conf_time;
    int pos_wking, pos_bking, piece_number, promoted_pieces, black_starts;
    int pos_wking_valid, pos_bking_valid;
    enum E_HMI_USER dialogue_answer;
    enum E_POS_STATE pos_answer;
    enum E_POS_EDIT_STATE editor_state;


    Util_Memzero(&edit_pos, sizeof(STARTING_POS));
    edit_pos.epsquare = BP_NOSQUARE;
    pos_wking = BP_E1; /*avoid compiler warning*/
    pos_bking = BP_E8; /*avoid compiler warning*/
    pos_wking_valid = 0;
    pos_bking_valid = 0;
    black_starts = 0; /*avoid compiler warning*/

    editor_state = POS_ENTER_WKING;
    while ((editor_state != POS_ENTER_FINISHED) && (editor_state != POS_ENTER_CANCELLED))
    /*loop though this state machine until the user leaves the position editor.*/
    {
        /*get the white pieces*/

        /* *** white king *** */
        if (editor_state == POS_ENTER_WKING)
        {
            Util_Strcpy(menu_buffer, "position editor     white king:                                                 ");
            Hmi_Set_Bat_Display(menu_buffer);
            piece_number = Pos_Readline(menu_buffer, 1, 1, WKING, &edit_pos);
            if (piece_number == POS_PCS_LEAVE)
                editor_state = POS_ENTER_CANCELLED;
            else if ((piece_number == POS_PCS_REDO) || (piece_number == POS_PCS_UNDO))
            {
                if (pos_wking_valid)
                    editor_state = POS_ENTER_WQUEENS;
            } else
            {
                pos_wking_valid = 1;
                Pos_Add_Line(menu_buffer, WKING, piece_number, &edit_pos);
                /*save the position of the white king in case it gets overwritten by later entries*/
                pos_wking =    (menu_buffer[40] - 'a');
                pos_wking += 8*(menu_buffer[41] - '1');
                editor_state = POS_ENTER_WQUEENS;
            }
        }

        /* *** white queen *** */
        if (editor_state == POS_ENTER_WQUEENS)
        {
            edit_pos.board[pos_wking] = WKING;
            promoted_pieces = Pos_Count_Promoted_Pieces(WHITE, &edit_pos);
            promoted_pieces += Pos_Count_Pawns(WHITE, &edit_pos);
            Util_Strcpy(menu_buffer, "position editor     white queens:                                               ");
            Hmi_Set_Bat_Display(menu_buffer);
            piece_number = Pos_Readline(menu_buffer, 1 + 8 - promoted_pieces, 0, WQUEEN, &edit_pos);
            if (piece_number == POS_PCS_LEAVE)
                editor_state = POS_ENTER_CANCELLED;
            else if (piece_number == POS_PCS_REDO)
                editor_state = POS_ENTER_WROOKS;
            else if (piece_number == POS_PCS_UNDO)
                editor_state = POS_ENTER_WKING;
            else
            {
                Pos_Add_Line(menu_buffer, WQUEEN, piece_number, &edit_pos);
                editor_state = POS_ENTER_WROOKS;
            }
        }

        /* *** white rooks *** */
        if (editor_state == POS_ENTER_WROOKS)
        {
            edit_pos.board[pos_wking] = WKING;
            promoted_pieces = Pos_Count_Promoted_Pieces(WHITE, &edit_pos);
            promoted_pieces += Pos_Count_Pawns(WHITE, &edit_pos);
            Util_Strcpy(menu_buffer, "position editor     white rooks:                                                ");
            Hmi_Set_Bat_Display(menu_buffer);
            piece_number = Pos_Readline(menu_buffer, 2 + 8 - promoted_pieces, 0, WROOK, &edit_pos);
            if (piece_number == POS_PCS_LEAVE)
                editor_state = POS_ENTER_CANCELLED;
            else if (piece_number == POS_PCS_REDO)
                editor_state = POS_ENTER_WBISHOPS;
            else if (piece_number == POS_PCS_UNDO)
                editor_state = POS_ENTER_WQUEENS;
            else
            {
                Pos_Add_Line(menu_buffer, WROOK, piece_number, &edit_pos);
                editor_state = POS_ENTER_WBISHOPS;
            }
        }

        /* *** white bishops *** */
        if (editor_state == POS_ENTER_WBISHOPS)
        {
            edit_pos.board[pos_wking] = WKING;
            promoted_pieces = Pos_Count_Promoted_Pieces(WHITE, &edit_pos);
            promoted_pieces += Pos_Count_Pawns(WHITE, &edit_pos);
            Util_Strcpy(menu_buffer, "position editor     white bishops:                                              ");
            Hmi_Set_Bat_Display(menu_buffer);
            piece_number = Pos_Readline(menu_buffer, 2 + 8 - promoted_pieces, 0, WBISHOP, &edit_pos);
            if (piece_number == POS_PCS_LEAVE)
                editor_state = POS_ENTER_CANCELLED;
            else if (piece_number == POS_PCS_REDO)
                editor_state = POS_ENTER_WKNIGHTS;
            else if (piece_number == POS_PCS_UNDO)
                editor_state = POS_ENTER_WROOKS;
            else
            {
                Pos_Add_Line(menu_buffer, WBISHOP, piece_number, &edit_pos);
                editor_state = POS_ENTER_WKNIGHTS;
            }

        }

         /* *** white knights *** */
       if (editor_state == POS_ENTER_WKNIGHTS)
        {
            edit_pos.board[pos_wking] = WKING;
            promoted_pieces = Pos_Count_Promoted_Pieces(WHITE, &edit_pos);
            promoted_pieces += Pos_Count_Pawns(WHITE, &edit_pos);
            Util_Strcpy(menu_buffer, "position editor     white knights:                                              ");
            Hmi_Set_Bat_Display(menu_buffer);
            piece_number = Pos_Readline(menu_buffer, 2 + 8 - promoted_pieces, 0, WKNIGHT, &edit_pos);
            if (piece_number == POS_PCS_LEAVE)
                editor_state = POS_ENTER_CANCELLED;
            else if (piece_number == POS_PCS_REDO)
                editor_state = POS_ENTER_WPAWNS;
            else if (piece_number == POS_PCS_UNDO)
                editor_state = POS_ENTER_WBISHOPS;
            else
            {
                Pos_Add_Line(menu_buffer, WKNIGHT, piece_number, &edit_pos);
                editor_state = POS_ENTER_WPAWNS;
            }
        }

         /* *** white pawns *** */
       if (editor_state == POS_ENTER_WPAWNS)
        {
            edit_pos.board[pos_wking] = WKING;
            promoted_pieces = Pos_Count_Promoted_Pieces(WHITE, &edit_pos);
            if (promoted_pieces < 8)
            {
                Util_Strcpy(menu_buffer, "position editor     white pawns:                                                ");
                Hmi_Set_Bat_Display(menu_buffer);
                piece_number = Pos_Readline(menu_buffer, 8 - promoted_pieces, 0, WPAWN, &edit_pos);
                if (piece_number == POS_PCS_LEAVE)
                    editor_state = POS_ENTER_CANCELLED;
                else if (piece_number == POS_PCS_REDO)
                    editor_state = POS_ENTER_BKING;
                else if (piece_number == POS_PCS_UNDO)
                    editor_state = POS_ENTER_WKNIGHTS;
                else
                {
                    Pos_Add_Line(menu_buffer, WPAWN, piece_number, &edit_pos);
                    editor_state = POS_ENTER_BKING;
                }
            } else
                editor_state = POS_ENTER_BKING;
        }

        /*get the black pieces*/

        /* *** black king *** */
        if (editor_state == POS_ENTER_BKING)
        {
            edit_pos.board[pos_wking] = WKING;
            Util_Strcpy(menu_buffer, "position editor     black king:                                                 ");
            Hmi_Set_Bat_Display(menu_buffer);
            piece_number = Pos_Readline(menu_buffer, 1, 1, BKING, &edit_pos);
            if (Pos_Black_King_Absent(&edit_pos))
                pos_bking_valid = 0;
            if (piece_number == POS_PCS_LEAVE)
                editor_state = POS_ENTER_CANCELLED;
            else if (piece_number == POS_PCS_REDO)
            {
                if (pos_bking_valid)
                    editor_state = POS_ENTER_BQUEENS;
            } else if (piece_number == POS_PCS_UNDO)
            {
                int wh_promoted_pieces = Pos_Count_Promoted_Pieces(WHITE, &edit_pos);

                /*the previous state now depends on whether there are less than 8 white
                promoted pieces. If so, going back means entering white pawns. Otherwise,
                it means entering white knights.*/
                editor_state = (wh_promoted_pieces < 8) ? POS_ENTER_WPAWNS : POS_ENTER_WKNIGHTS;
            } else
            {
                /*save the position of the black king in case it gets overwritten by later entries*/
                pos_bking =    (menu_buffer[40] - 'a');
                pos_bking += 8*(menu_buffer[41] - '1');

                if (pos_bking != pos_wking)
                    /*overwriting the white king's position is not allowed.*/
                {
                    pos_bking_valid = 1;
                    Pos_Add_Line(menu_buffer, BKING, piece_number, &edit_pos);

                    editor_state = POS_ENTER_BQUEENS;
                } else
                {
                    (void)            Hmi_Conf_Dialogue("square is taken", "by white king.", &dummy_conf_time,
                                                        HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                    dialogue_answer = Hmi_Conf_Dialogue("go back to", "white king?", &dummy_conf_time,
                                                        HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
                    if (dialogue_answer == HMI_USER_OK)
                        editor_state = POS_ENTER_WKING;
                    /*if "cancel", the editor state will stay POS_ENTER_BKING, and in the next
                    loop run, the user can enter the black king's position again.*/
                }
            }
        }

        /* *** black queens *** */
        if (editor_state == POS_ENTER_BQUEENS)
        {
            edit_pos.board[pos_wking] = WKING;
            edit_pos.board[pos_bking] = BKING;
            promoted_pieces = Pos_Count_Promoted_Pieces(BLACK, &edit_pos);
            promoted_pieces += Pos_Count_Pawns(BLACK, &edit_pos);
            Util_Strcpy(menu_buffer, "position editor     black queens:                                               ");
            Hmi_Set_Bat_Display(menu_buffer);
            piece_number = Pos_Readline(menu_buffer, 1 + 8 - promoted_pieces, 0, BQUEEN, &edit_pos);
            if (piece_number == POS_PCS_LEAVE)
                editor_state = POS_ENTER_CANCELLED;
            else if (piece_number == POS_PCS_REDO)
                editor_state = POS_ENTER_BROOKS;
            else if (piece_number == POS_PCS_UNDO)
                editor_state = POS_ENTER_BKING;
            else
            {
                Pos_Add_Line(menu_buffer, BQUEEN, piece_number, &edit_pos);
                editor_state = POS_ENTER_BROOKS;
            }
        }

        /* *** black rooks *** */
        if (editor_state == POS_ENTER_BROOKS)
        {
            edit_pos.board[pos_wking] = WKING;
            edit_pos.board[pos_bking] = BKING;
            promoted_pieces = Pos_Count_Promoted_Pieces(BLACK, &edit_pos);
            promoted_pieces += Pos_Count_Pawns(BLACK, &edit_pos);
            Util_Strcpy(menu_buffer, "position editor     black rooks:                                                ");
            Hmi_Set_Bat_Display(menu_buffer);
            piece_number = Pos_Readline(menu_buffer, 2 + 8 - promoted_pieces, 0, BROOK, &edit_pos);
            if (piece_number == POS_PCS_LEAVE)
                editor_state = POS_ENTER_CANCELLED;
            else if (piece_number == POS_PCS_REDO)
                editor_state = POS_ENTER_BBISHOPS;
            else if (piece_number == POS_PCS_UNDO)
                editor_state = POS_ENTER_BQUEENS;
            else
            {
                Pos_Add_Line(menu_buffer, BROOK, piece_number, &edit_pos);
                editor_state = POS_ENTER_BBISHOPS;
            }
        }

        /* *** black bishops *** */
        if (editor_state == POS_ENTER_BBISHOPS)
        {
            edit_pos.board[pos_wking] = WKING;
            edit_pos.board[pos_bking] = BKING;
            promoted_pieces = Pos_Count_Promoted_Pieces(BLACK, &edit_pos);
            promoted_pieces += Pos_Count_Pawns(BLACK, &edit_pos);
            Util_Strcpy(menu_buffer, "position editor     black bishops:                                              ");
            Hmi_Set_Bat_Display(menu_buffer);
            piece_number = Pos_Readline(menu_buffer, 2 + 8 - promoted_pieces, 0, BBISHOP, &edit_pos);
            if (piece_number == POS_PCS_LEAVE)
                editor_state = POS_ENTER_CANCELLED;
            else if (piece_number == POS_PCS_REDO)
                editor_state = POS_ENTER_BKNIGHTS;
            else if (piece_number == POS_PCS_UNDO)
                editor_state = POS_ENTER_BROOKS;
            else
            {
                Pos_Add_Line(menu_buffer, BBISHOP, piece_number, &edit_pos);
                editor_state = POS_ENTER_BKNIGHTS;
            }
        }

        /* *** black knights *** */
        if (editor_state == POS_ENTER_BKNIGHTS)
        {
            edit_pos.board[pos_wking] = WKING;
            edit_pos.board[pos_bking] = BKING;
            promoted_pieces = Pos_Count_Promoted_Pieces(BLACK, &edit_pos);
            promoted_pieces += Pos_Count_Pawns(BLACK, &edit_pos);
            Util_Strcpy(menu_buffer, "position editor     black knights:                                              ");
            Hmi_Set_Bat_Display(menu_buffer);
            piece_number = Pos_Readline(menu_buffer, 2 + 8 - promoted_pieces, 0, BKNIGHT, &edit_pos);
            if (piece_number == POS_PCS_LEAVE)
                editor_state = POS_ENTER_CANCELLED;
            else if (piece_number == POS_PCS_REDO)
                editor_state = POS_ENTER_BPAWNS;
            else if (piece_number == POS_PCS_UNDO)
                editor_state = POS_ENTER_BBISHOPS;
            else
            {
                Pos_Add_Line(menu_buffer, BKNIGHT, piece_number, &edit_pos);
                editor_state = POS_ENTER_BPAWNS;
            }
        }

        /* *** black pawns *** */
        if (editor_state == POS_ENTER_BPAWNS)
        {
            edit_pos.board[pos_wking] = WKING;
            edit_pos.board[pos_bking] = BKING;
            promoted_pieces = Pos_Count_Promoted_Pieces(BLACK, &edit_pos);
            if (promoted_pieces < 8)
            {
                Util_Strcpy(menu_buffer, "position editor     black pawns:                                                ");
                Hmi_Set_Bat_Display(menu_buffer);
                piece_number = Pos_Readline(menu_buffer, 8 - promoted_pieces, 0, BPAWN, &edit_pos);
                if (piece_number == POS_PCS_LEAVE)
                    editor_state = POS_ENTER_CANCELLED;
                else if (piece_number == POS_PCS_UNDO)
                    editor_state = POS_ENTER_BKNIGHTS;
                else
                {
                    Pos_Add_Line(menu_buffer, BPAWN, piece_number, &edit_pos);
                    editor_state = POS_PIECES_ENTERED;
                }
            } else
                editor_state = POS_PIECES_ENTERED;
        }

        /* all pieces entered. */
        if (editor_state == POS_PIECES_ENTERED)
        {
            edit_pos.board[pos_wking] = WKING;
            edit_pos.board[pos_bking] = BKING;
            dialogue_answer = Hmi_Conf_Dialogue("finished", "entering pieces?", &dummy_conf_time,
                                                HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
            if (dialogue_answer != HMI_USER_OK)
            {
                int bl_promoted_pieces;

                bl_promoted_pieces = Pos_Count_Promoted_Pieces(BLACK, &edit_pos);
                /*the previous state now depends on whether there are less than 8 black
                promoted pieces. If so, going back means entering black pawns. Otherwise,
                it means entering black knights.*/
                editor_state = (bl_promoted_pieces < 8) ? POS_ENTER_BPAWNS : POS_ENTER_BKNIGHTS;
            } else
                editor_state = POS_ENTER_TURN;
        }

        /* *** side to move *** */
        if (editor_state == POS_ENTER_TURN)
        {
            /*which side is to move?*/
            edit_pos.gflags = FLAGRESET;

            dialogue_answer = Hmi_Conf_Dialogue("white to move: OK", "black to move: CL", &dummy_conf_time,
                                                HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
            black_starts = (dialogue_answer == HMI_USER_OK) ? 0 : 1;

            editor_state = POS_ENTER_EP;
        }

        /* *** en passant squares *** */
        if (editor_state == POS_ENTER_EP)
        {
            /*any en passant square possible?*/
            if (Pos_EPSQ_Exist(black_starts, &edit_pos))
            {
                Util_Strcpy(menu_buffer, "position editor     en passant square:                                          ");
                Hmi_Set_Bat_Display(menu_buffer);
                if (Pos_EPSQ_Valid(black_starts, edit_pos.epsquare, &edit_pos) == 0)
                    edit_pos.epsquare = BP_NOSQUARE;
                piece_number = Pos_Readline(menu_buffer, 1, 0, POSEDIT_EPSQ_TYPE, &edit_pos);
                if (piece_number == POS_PCS_LEAVE)
                    editor_state = POS_ENTER_CANCELLED;
                else if (piece_number == POS_PCS_REDO)
                    editor_state = POS_ENTER_CASTL;
                else if (piece_number == POS_PCS_UNDO)
                    editor_state = POS_PIECES_ENTERED;
                else
                {
                    if (piece_number > 0)
                    /*EP square given?*/
                    {
                        Pos_Add_Line(menu_buffer, POSEDIT_EPSQ_TYPE, piece_number, &edit_pos);
                        if (Pos_EPSQ_Valid(black_starts, edit_pos.epsquare, &edit_pos))
                            editor_state = POS_ENTER_CASTL;
                        else
                        /*invalid ep square given. beep and start over with EP entering.*/
                        {
                            edit_pos.epsquare = BP_NOSQUARE;
                            Hmi_Signal(HMI_MSG_ERROR);
                        }
                    } else
                    /*fine, the user doesn't want an EP square.*/
                    {
                        edit_pos.epsquare = BP_NOSQUARE;
                        editor_state = POS_ENTER_CASTL;
                    }
                }
            } else /*no EP squares possible, so clean up the variable.*/
            {
                edit_pos.epsquare = BP_NOSQUARE;
                editor_state = POS_ENTER_CASTL;
            }
        }

        /* *** castling rights *** */
        if (editor_state == POS_ENTER_CASTL)
        {
            unsigned int castling_rights = CST_NONE, castling_opts = 0;
            char white_castling_str[20], black_castling_str[20];

            /*check where castling is possible*/
            if (pos_wking == BP_E1)
            {
                if (edit_pos.board[BP_H1] == WROOK)
                {
                    castling_rights |= CST_WH_KING;
                    castling_opts++;
                }
                if (edit_pos.board[BP_A1] == WROOK)
                {
                    castling_rights |= CST_WH_QUEEN;
                    castling_opts++;
                }
            }
            if (pos_bking == BP_E8)
            {
                if (edit_pos.board[BP_H8] == BROOK)
                {
                    castling_rights |= CST_BL_KING;
                    castling_opts++;
                }
                if (edit_pos.board[BP_A8] == BROOK)
                {
                    castling_rights |= CST_BL_QUEEN;
                    castling_opts++;
                }
            }

            /*any castling possible?*/
            if (castling_opts > 0)
            {
                /*show the aggregate dialogue only if more than one castling
                  possibility exists. otherwise, the user would need to go
                  through two dialogues to cancel a single possibility.*/
                if (castling_opts > 1)
                    dialogue_answer = Hmi_Conf_Dialogue("allow all", "castling?", &dummy_conf_time,
                                                        HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
                else
                    dialogue_answer = HMI_USER_CANCEL;

                if (dialogue_answer != HMI_USER_OK)
                {
                    /*checking individually whether to reset the derived
                      castling rights*/
                    if (castling_rights & CST_WH_KING)
                    {
                        dialogue_answer = Hmi_Conf_Dialogue("can white castle", "kingside?", &dummy_conf_time,
                                                        HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
                        if (dialogue_answer != HMI_USER_OK)
                            castling_rights &= ~CST_WH_KING;
                    }
                    if (castling_rights & CST_WH_QUEEN)
                    {
                        dialogue_answer = Hmi_Conf_Dialogue("can white castle", "queenside?", &dummy_conf_time,
                                                        HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
                        if (dialogue_answer != HMI_USER_OK)
                            castling_rights &= ~CST_WH_QUEEN;
                    }
                    if (castling_rights & CST_BL_KING)
                    {
                        dialogue_answer = Hmi_Conf_Dialogue("can black castle", "kingside?", &dummy_conf_time,
                                                        HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
                        if (dialogue_answer != HMI_USER_OK)
                            castling_rights &= ~CST_BL_KING;
                    }
                    if (castling_rights & CST_BL_QUEEN)
                    {
                        dialogue_answer = Hmi_Conf_Dialogue("can black castle", "queenside?", &dummy_conf_time,
                                                        HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
                        if (dialogue_answer != HMI_USER_OK)
                            castling_rights &= ~CST_BL_QUEEN;
                    }
                }
            }

            /*now we have the validated castling info,
              prepare the castling info display*/
            Util_Strcpy(white_castling_str, "wh   cstl: ");
            Util_Strcpy(black_castling_str, "bl   cstl: ");
            if (!black_starts)
                white_castling_str[3] = '*';
            else
                black_castling_str[3] = '*';

            /*white castling right evaluation*/
            switch (castling_rights & (CST_WH_KING | CST_WH_QUEEN))
            {
            case CST_WH_KING | CST_WH_QUEEN:
                Util_Strcat(white_castling_str, "both");
                break;
            case CST_WH_QUEEN:
                edit_pos.gflags |= WRH1MOVED;
                Util_Strcat(white_castling_str, "queen");
                break;
            case CST_WH_KING:
                edit_pos.gflags |= WRA1MOVED;
                Util_Strcat(white_castling_str, "king");
                break;
            case CST_NONE:
            default:
                edit_pos.gflags |= WRA1MOVED | WRH1MOVED | WKMOVED;
                Util_Strcat(white_castling_str, "none");
                break;
            }

            /*black castling right evaluation*/
            switch (castling_rights & (CST_BL_KING | CST_BL_QUEEN))
            {
            case CST_BL_KING | CST_BL_QUEEN:
                Util_Strcat(black_castling_str, "both");
                break;
            case CST_BL_QUEEN:
                edit_pos.gflags |= BRH8MOVED;
                Util_Strcat(black_castling_str, "queen");
                break;
            case CST_BL_KING:
                edit_pos.gflags |= BRA8MOVED;
                Util_Strcat(black_castling_str, "king");
                break;
            case CST_NONE:
            default:
                edit_pos.gflags |= BRA8MOVED | BRH8MOVED | BKMOVED;
                Util_Strcat(black_castling_str, "none");
                break;
            }

            dialogue_answer = Hmi_Conf_Dialogue(white_castling_str, black_castling_str, &dummy_conf_time,
                                                HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
            editor_state = (dialogue_answer == HMI_USER_OK) ? POS_ENTER_VIEW : POS_PIECES_ENTERED;
        } /*end of castling entering*/

        /* *** user can view and verify the position *** */
        if (editor_state == POS_ENTER_VIEW)
        {
            if (Pos_Disp_Edit_Board(&edit_pos) == HMI_MENU_LEAVE)
                editor_state = POS_ENTER_CANCELLED;
            else
            {
                dialogue_answer = Hmi_Conf_Dialogue("position", " ok?", &dummy_conf_time,
                                                    HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
                editor_state = (dialogue_answer == HMI_USER_OK) ? POS_ENTER_FINISHED : POS_ENTER_WKING;
            }
        }

    } /*end of the position editor state machine*/

    if (editor_state == POS_ENTER_CANCELLED)
        return (HMI_MENU_LEAVE);

    /*make sure that the kings' positions have not been overwritten by other pieces,
    in which case the position check might run into serious issues*/
    edit_pos.board[pos_wking] = WKING;
    edit_pos.board[pos_bking] = BKING;

    pos_answer = Play_Load_Position(&edit_pos, black_starts, pos_wking, pos_bking);

    /*new position, so clear the stored status information for the pretty game notation*/
    Hmi_Clear_Pretty_Print();

    if (pos_answer == POS_OK) /*conversion was OK, blink with the green LED*/
    {
        Hmi_Signal(HMI_MSG_OK);
        return(HMI_MENU_NEW_POS);
    }

    switch (pos_answer)
    {
        case POS_TOO_MANY_PIECES:
            Util_Strcpy(reject_reason, "too many pieces,");
            break;
        case POS_KING_INVALID:
            Util_Strcpy(reject_reason, "invalid king pos,");
            break;
        case POS_CHECKS_INVALID:
            Util_Strcpy(reject_reason, "invalid checks,");
            break;
        case POS_TOO_MANY_MOVES:
            Util_Strcpy(reject_reason, "too many moves,");
            break;
        case POS_TOO_MANY_CAPTS:
            Util_Strcpy(reject_reason, "too many captures,");
            break;
        case POS_TOO_MANY_CHECKS:
            Util_Strcpy(reject_reason, "too many checks,");
            break;
        default:
            Util_Strcpy(reject_reason, "general error,");
            break;
    }

    Hmi_Signal(HMI_MSG_ERROR); /*signal an error beep and red LED to the user*/
    (void) Hmi_Conf_Dialogue(reject_reason, "pos rejected.", &dummy_conf_time,
                             HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);

    return(HMI_MENU_OK);
}
