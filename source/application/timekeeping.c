/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2016-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of the CT800 (time control functions).
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
#include "hmi.h"
#include "util.h"
#include "timekeeping.h"
#include "hardware.h"

/* ------------- external variables -------*/
/*-- READ-ONLY  --*/
extern int mv_stack_p;
extern int black_started_game;

/*-- READ-WRITE --*/
extern uint64_t hw_config;
extern volatile enum E_TIMEOUT time_is_up;


/* ------------- module variables -------*/

static TIME_KEEPING time_keeping;
static int32_t start_time, stop_time;
static volatile enum E_TIME_DISP disp_toggle = NO_DISP_TOGGLE;

/* ------------- module static functions -------*/

static int32_t Calculate_Move_Time(int32_t movenumber, int32_t moves_to_go, int32_t total_time, int32_t increment)
{
    int32_t move_time;
    if (moves_to_go > 0)
    {
        /*keep a 100 ms as safeguard buffer*/
        move_time = (total_time - (MILLISECONDS / 10L)) / (moves_to_go);
        if ((movenumber >= 10) && (moves_to_go >= 10)) /*account for time savings*/
        {
            move_time *= 5L;
            move_time /= 4L;
        }
    }
    else /*fixed time per game, or rest of the game*/
    {
        int expected_moves;
        if (movenumber >= 70)
            expected_moves = 20;
        else
            expected_moves = 48 - (movenumber * 2) / 5;
        /*keep a 100 ms as safeguard buffer*/
        move_time = (total_time - (MILLISECONDS / 10L)) / expected_moves;
        if ((movenumber >= 10) && (movenumber <= 30)) /*account for time savings*/
        {
            move_time *= 5L;
            move_time /= 4L;
        }
    }
    if (increment)
    {
        /*keep a 100 ms as safeguard buffer*/
        int32_t max_time = total_time - (MILLISECONDS / 10L);
        /*time >= 2.4 Inc: consume 1.4  Inc
          time >= 1.5 Inc: consume 1.0  Inc
          time >= 1.0 Inc: consume 0.75 Inc
          time <  1.0 Inc: consume 0.5 Inc*/
        if (total_time >= (increment * 12L) / 5L)
            move_time += (increment * 7L) / 5L;
        else if (total_time >= (increment * 3L) / 2L)
            move_time += increment;
        else if (total_time >= increment)
            move_time += (increment * 3L) / 4L;
        else
            move_time += increment / 2L;

        if (move_time > max_time) move_time = max_time;
    }
    if (move_time < 0L) move_time = 0L;

    return(move_time);
}

static int32_t Get_Player_Time_Bonus(void)
{
    if (CFG_HAS_OPT(CFG_PLAYER_BONUS_MODE, CFG_PLAYER_BONUS_ON))
        return (CFG_PLAYER_BONUS_TIME * MILLISECONDS);
    else
        return (0);
}

static int32_t Get_Fischer_Time_Bonus(void)
{
    switch (CFG_GET_OPT(CFG_FISCHER_MODE))
    {
    case CFG_FISCHER_LV0:
        return(CFG_FISCHER_LV0_TIME * MILLISECONDS);
        break;
    case CFG_FISCHER_LV1:
        return(CFG_FISCHER_LV1_TIME * MILLISECONDS);
        break;
    case CFG_FISCHER_LV2:
        return(CFG_FISCHER_LV2_TIME * MILLISECONDS);
        break;
    default:
        return(0);
        break;
    }
}

/*returns the allocated computing time for the upcoming move in tournament time*/
static int32_t Get_Tournament_Move_Time(int32_t plynumber, int32_t increment)
{
    int32_t movenumber, remaining_time;
    /*attention: the plies start from 0, but the moves in chess start from 1!*/
    movenumber = (plynumber / 2) + 1;

    if ((plynumber & 1L) == 0) /*even plies: white*/
        remaining_time = time_keeping.remaining_white_time;
    else
        remaining_time = time_keeping.remaining_black_time;

    if (movenumber <= 40) /*first time control in all tournament modes*/
    {
        int32_t moves_to_go = 41 - movenumber;
        return(Calculate_Move_Time(movenumber, moves_to_go, remaining_time, increment));
    }

    /*so we are beyond move 40, the time control modes differ here.
    mode 3 is special with another time control after move 60.*/
    if ((CFG_HAS_OPT(CFG_TRN_MODE,CFG_TRN_LV3)) && (movenumber <= 60))
    {
        int32_t moves_to_go = 61 - movenumber;
        return(Calculate_Move_Time(movenumber, moves_to_go, remaining_time, increment));
    }

    return(Calculate_Move_Time(movenumber, 0 /*game-in mode*/, remaining_time, increment));
}

static int32_t Get_TPM_Time(void)
{
    switch (CFG_GET_OPT(CFG_TPM_MODE))
    {
    case CFG_TPM_LV0:
        return (CFG_TPM_LV0_TIME*MILLISECONDS);
        break;
    case CFG_TPM_LV1:
        return (CFG_TPM_LV1_TIME*MILLISECONDS);
        break;
    case CFG_TPM_LV2:
        return (CFG_TPM_LV2_TIME*MILLISECONDS);
        break;
    case CFG_TPM_LV3:
        return (CFG_TPM_LV3_TIME*MILLISECONDS);
        break;
    case CFG_TPM_LV4:
        return (CFG_TPM_LV4_TIME*MILLISECONDS);
        break;
    case CFG_TPM_LV5:
        return (CFG_TPM_LV5_TIME*MILLISECONDS);
        break;
    case CFG_TPM_LV6:
        return (CFG_TPM_LV6_TIME*MILLISECONDS);
        break;
    case CFG_TPM_LV7:
        return (CFG_TPM_LV7_TIME*MILLISECONDS);
        break;
    default:
        /*we should not get here*/
        return (CFG_TPM_LV3_TIME*MILLISECONDS);
        break;
    }
}

/* returns the total game time in milliseconds for game-in configurations.*/
static int32_t Get_Game_In_Total_Time(void)
{
    switch (CFG_GET_OPT(CFG_GAME_IN_MODE))
    {
    case CFG_GAME_IN_LV0:
        return(CFG_GAME_IN_LV0_TIME*MILLISECONDS);
        break;
    case CFG_GAME_IN_LV1:
        return(CFG_GAME_IN_LV1_TIME*MILLISECONDS);
        break;
    case CFG_GAME_IN_LV2:
        return(CFG_GAME_IN_LV2_TIME*MILLISECONDS);
        break;
    case CFG_GAME_IN_LV3:
        return(CFG_GAME_IN_LV3_TIME*MILLISECONDS);
        break;
    case CFG_GAME_IN_LV4:
        return(CFG_GAME_IN_LV4_TIME*MILLISECONDS);
        break;
    case CFG_GAME_IN_LV5:
        return(CFG_GAME_IN_LV5_TIME*MILLISECONDS);
        break;
    case CFG_GAME_IN_LV6:
        return(CFG_GAME_IN_LV6_TIME*MILLISECONDS);
        break;
    case CFG_GAME_IN_LV7:
        return(CFG_GAME_IN_LV7_TIME*MILLISECONDS);
        break;
    default:
        /*we should not get here*/
        return(CFG_GAME_IN_LV3_TIME*MILLISECONDS);
        break;
    }
}

/*updates the time display*/
static void Time_Intermediate_Display(enum E_COLOUR colour, int32_t system_time, int cursorpos)
{
    int32_t raw_time, disp_time;
    enum E_TIME_DISP tmp_disp_toggle = disp_toggle;

    /*effective time for the ongoing move*/
    raw_time =  system_time - time_keeping.dialogue_conf_time - start_time;

    switch (CFG_GET_OPT(CFG_GAME_MODE)) /*what kind of time control is applicable?*/
    {
    case CFG_GAME_MODE_TPM:
    case CFG_GAME_MODE_ANA:
    case CFG_GAME_MODE_MTI:
        /*both modes display the effective thinking time for the current ply.*/
        if (UNLIKELY((raw_time >= time_keeping.next_full_second) ||
                     (tmp_disp_toggle != NO_DISP_TOGGLE)))
        /*update the display when a new second is to be displayed*/
        {
            Hmi_Update_Running_Time(raw_time, raw_time, cursorpos, tmp_disp_toggle);
            if (tmp_disp_toggle == NO_DISP_TOGGLE)
                time_keeping.next_full_second += MILLISECONDS;
            else
            {
                /*a full display update takes about 6 ms, so add this*/
                disp_time = Hw_Get_System_Time() - system_time;
                if (disp_time > 1L) /*probably just timer interrupt*/
                    time_keeping.dialogue_conf_time += disp_time + 1L;
                disp_toggle = NO_DISP_TOGGLE;
            }
        }
        break; /*of game-in and analysis mode*/
    case CFG_GAME_MODE_TRN:
    case CFG_GAME_MODE_GMI:
        /*both modes display the remaining time for the current player.*/
        /*convert the amount of time to "remaining time*/
        if (colour == WHITE) /*white's move*/
            disp_time = time_keeping.remaining_white_time - raw_time;
        else if (colour == BLACK)
            disp_time = time_keeping.remaining_black_time - raw_time;
        else /*should not happen*/
            disp_time = 0;

        if (UNLIKELY((disp_time <= time_keeping.next_full_second) ||
                     (tmp_disp_toggle != NO_DISP_TOGGLE)))
        /*update the display when a new second is to be displayed*/
        {
            Hmi_Update_Running_Time(disp_time, raw_time, cursorpos, tmp_disp_toggle);
            if (tmp_disp_toggle == NO_DISP_TOGGLE)
                time_keeping.next_full_second -= MILLISECONDS;
            else
            {
                /*a full display update takes about 6 ms, so add this*/
                disp_time = Hw_Get_System_Time() - system_time;
                if (disp_time > 1L) /*probably just timer interrupt*/
                    time_keeping.dialogue_conf_time += disp_time + 1L;
                disp_toggle = NO_DISP_TOGGLE;
            }
        }
        break; /* of tournament and game-in mode*/
    }
}

/*returns the computing time for the next move in milliseconds or 0 in case of error.
the answer of course also depends on the game mode.
sets also the dynamic hard time control for the search function.*/
static int32_t Get_Time_For_Move(int plynumber)
{
    int32_t result, remaining_time, tmp, fischer_bonus, movenumber;
    movenumber = (plynumber / 2) + 1;
    switch (CFG_GET_OPT(CFG_GAME_MODE)) /*what kind of time control is applicable?*/
    {
    case CFG_GAME_MODE_TPM:
        result = Get_TPM_Time();
        /*add up 25% to average out opening book,
          quick replies and 55% limit for new iteration.*/
        result *= 5L;
        result /= 4L;
        /*in TPM mode, the clock counts upwards, starting with 0s.*/
        time_keeping.next_full_second = 1L * MILLISECONDS;
        break; /*of TPM mode*/
    case CFG_GAME_MODE_GMI:
        fischer_bonus = Get_Fischer_Time_Bonus();
        if ((plynumber & 1L) == 0) /*white's time*/
            remaining_time = time_keeping.remaining_white_time;
        else
            remaining_time = time_keeping.remaining_black_time;

        result = Calculate_Move_Time(movenumber, 0 /*game in mode*/, remaining_time, fischer_bonus);

        /*for the display update*/
        tmp = remaining_time % MILLISECONDS; /*the fractional part of the time*/
        if (tmp != 0)
            time_keeping.next_full_second = remaining_time - tmp;
        else
            time_keeping.next_full_second = remaining_time - MILLISECONDS;
        break; /*of game-in mode*/
    case CFG_GAME_MODE_TRN:
        fischer_bonus = Get_Fischer_Time_Bonus();
        result = Get_Tournament_Move_Time(plynumber, fischer_bonus);
        if ((plynumber & 1L) == 0) /*white's time*/
            remaining_time = time_keeping.remaining_white_time;
        else
            remaining_time = time_keeping.remaining_black_time;

        /*for the display update*/
        tmp = remaining_time % MILLISECONDS; /*the fractional part of the time*/
        if (tmp != 0)
            time_keeping.next_full_second = remaining_time - tmp;
        else
            time_keeping.next_full_second = remaining_time - MILLISECONDS;
        break; /* of tournament mode*/
    case CFG_GAME_MODE_ANA:
        result = CFG_ANA_TIME * MILLISECONDS;
        /*in analysis mode, the clock counts upwards, starting with 0s.*/
        time_keeping.next_full_second = 1L * MILLISECONDS;
        break;
    case CFG_GAME_MODE_MTI:
        result = CFG_MTI_TIME * MILLISECONDS;
        /*in mate-in mode, the clock counts upwards, starting with 0s.*/
        time_keeping.next_full_second = 1L * MILLISECONDS;
        break;
    default:
        /*we should never get here, just return something legal.*/
        result = 0;
        break;
    }
    /* enforce an amount of milliseconds that is divisible by 10.
    useful for systems with 10ms timer tick resolution.*/
    result -= (result % 10L);
    return (result);
}

static void Time_Update_TRN(int plynumber)
/*plynumber is the number of the ply just executed on the board.
If it is even, it was white's move, otherwise black's.*/
{
    int32_t rest_time;
    /*only applicable for tournament modes*/
    if (!(CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TRN)))
        return;
    /*level 3 is tricky because it adds 60 minutes after move 60 and 30 minutes after move 60*/
    if (CFG_HAS_OPT(CFG_TRN_MODE,CFG_TRN_LV3))
    {
        if (plynumber == 78) /*78/2+1 = 40th move from white*/
        {
            time_keeping.remaining_white_time += CFG_TRN_LV3_M60 * MILLISECONDS;
        }
        if (plynumber == 79) /*79/2+1 = 40th move from black*/
        {
            time_keeping.remaining_black_time += CFG_TRN_LV3_M60 * MILLISECONDS;
        }
        if (plynumber == 118) /*118/2+1 = 60th move from white*/
        {
            time_keeping.remaining_white_time += CFG_TRN_LV3_REST * MILLISECONDS;
        }
        if (plynumber == 119) /*119/2+1 = 60th move from black*/
        {
            time_keeping.remaining_black_time += CFG_TRN_LV3_REST * MILLISECONDS;
        }
        return;
    }
    /*at this point, we must be at tournament level 0-2 which all have a rest time addup after move 40.*/
    switch (CFG_GET_OPT(CFG_TRN_MODE))
    {
    case CFG_TRN_LV0:
        rest_time = CFG_TRN_LV0_REST * MILLISECONDS;
        break;
    case CFG_TRN_LV1:
        rest_time = CFG_TRN_LV1_REST * MILLISECONDS;
        break;
    case CFG_TRN_LV2:
        rest_time = CFG_TRN_LV2_REST * MILLISECONDS;
        break;
    default:
        rest_time = 0; /*should not happen - but the error will be apparent*/
        break;
    }
    if (plynumber == 78) /*78/2+1 = 40th move from white*/
    {
        time_keeping.remaining_white_time += rest_time;
    }
    if (plynumber == 79) /*79/2+1 = 40th move from black*/
    {
        time_keeping.remaining_black_time += rest_time;
    }
}



/* ------------- global functions --------------*/

/*the functions
Time_Give_Bonus()
Time_Countdown()
Time_Control()
Time_Update()
are meant to be used in the following way:
before a player moves, Time_Give_Bonus() shall add possible boni to his time (Fischer and Player).
note that Get_Fischer_Bonus() is called both here and in Get_Time_For_Move().
The difference is that Time_Give_Bonus() adds this bonus to the actual time account
while Get_Time_For_Move() tells the computer (before his next move) that he can use the bonus time.
after the move is completed, Time_Countdown() follows to subtract the time just used
and to record the used time for the undo function. Possible confirmation times for dialogues during the
thinking time for that ply are taken care of there.
then, Time_Control() shall check whether he just lost on time. if so, Time_Update() must not
be called - instead, the message "time lost 1-0" or "time lost 0-1" shall be displayed.
otherwise, Time_Update() shall add possible additional time, which is the case in the
tournament modes after the 40th move is completed, and in level 3 also after the 60th move.
    note that also after the 40th move, Time_Control() must happen before Time_Update().*/
void Time_Give_Bonus(enum E_COLOUR colour_to_move, enum E_COLOUR computer_side)
{
    if ((CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TPM)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_ANA)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_MTI)))
        return;
    /*time control only applies for game-in and tournament modes*/
    if (colour_to_move == WHITE)
    {
        time_keeping.remaining_white_time += Get_Fischer_Time_Bonus();
        if (colour_to_move != computer_side)
            time_keeping.remaining_white_time += Get_Player_Time_Bonus();
    } else if (colour_to_move == BLACK)
    {
        time_keeping.remaining_black_time += Get_Fischer_Time_Bonus();
        if (colour_to_move != computer_side)
            time_keeping.remaining_black_time += Get_Player_Time_Bonus();
    }
}

/*starttime and endtime in milliseconds*/
void Time_Countdown(enum E_COLOUR colour_moved, int plynumber)
{
    int32_t used_time;

    /*time control only applies for game-in and tournament modes*/
    if ((CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TPM)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_ANA)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_MTI)))
    {
        time_keeping.dialogue_conf_time = 0;
        return;
    }

    /*add up possible time after 40th / 60th move*/
    Time_Update_TRN(plynumber);

    /*stop_time has been set to "now" via Time_Set_Stop()*/
    used_time = stop_time - start_time;

    /* in case there were dialogues during the last ply, don't count the confirmation time*/
    if (time_keeping.dialogue_conf_time != 0)
    {
        used_time -= time_keeping.dialogue_conf_time;
        /*and reset it*/
        time_keeping.dialogue_conf_time = 0;
    }

    if (time_keeping.time_used_index >= MAX_TIME_UNDO)
        /*stack is full, so shift everything down by one. that's a bit ugly, but this isn't a time-critical function.
        *troll* *troll* *trollolol*. */
    {
        int i;
        for (i=0; i < MAX_TIME_UNDO - 1; i++)
        {
            time_keeping.time_used_buffer[i] = time_keeping.time_used_buffer[i+1];
        }
        time_keeping.time_used_index = MAX_TIME_UNDO-1;
    }

    if (colour_moved == WHITE)
    {
        time_keeping.remaining_white_time -= used_time;
        time_keeping.time_used_buffer[time_keeping.time_used_index] = time_keeping.remaining_white_time;
    } else if (colour_moved == BLACK)
    {
        time_keeping.remaining_black_time -= used_time;
        time_keeping.time_used_buffer[time_keeping.time_used_index] = time_keeping.remaining_black_time;
    }

    time_keeping.time_used_index++;
}

/*used during player move input mode, except in the initial position.
  takes care of possible dialogue confirmation time.*/
enum E_TIME_INT_CHECK Time_Player_Intermediate_Check(enum E_COLOUR colour, int cursorpos)
{
    int32_t system_time;

    system_time = Hw_Get_System_Time();

    Time_Intermediate_Display(colour, system_time, cursorpos);

    if ((CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TPM)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_ANA)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_MTI)))
        return(TIME_OK);
    /*time control only applies for game-in and tournament modes*/
    if (colour == WHITE) /*white's move*/
    {
        if (UNLIKELY(time_keeping.remaining_white_time + time_keeping.dialogue_conf_time + start_time - system_time <= 0))
            return (TIME_FAIL);
    } else if (colour == BLACK)
    {
        if (UNLIKELY(time_keeping.remaining_black_time + time_keeping.dialogue_conf_time + start_time - system_time <= 0))
            return (TIME_FAIL);
    }
    return(TIME_OK);
}

/* do the time check*/
enum E_TIME_CONTROL Time_Control(enum E_COLOUR colour)
{
    if ((CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TPM)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_ANA)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_MTI)))
        return(TIME_BOTH_OK);
    /*time control only applies for game-in and tournament modes*/

    if (colour == WHITE) /*white's move*/
    {
        if (time_keeping.remaining_white_time <= 0)
            return(TIME_WHITE_LOST);
        if (time_keeping.remaining_black_time <= 0)
            return(TIME_BLACK_LOST);
    } else if (colour == BLACK)
    {
        if (time_keeping.remaining_black_time <= 0)
            return(TIME_BLACK_LOST);
        if (time_keeping.remaining_white_time <= 0)
            return(TIME_WHITE_LOST);
    }
    return(TIME_BOTH_OK);
}

/*if undo would not imply a time mode change, this returns true.
means, the time stack is fine with taking back another ply.*/
int Time_Undo_OK(void)
{
    if ((CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TPM)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_ANA)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_MTI)))
        return(1);
    if (time_keeping.time_used_index <= 2)
        /*there is nothing more to undo. This will hit in after taking back 20 plies.
        This will NOT hit in at the beginning of the game because that case would
        already fail the undo in the move related aspect, so the time related one
        would not come into action.*/
        return(0);

    return(1);
}

/*this does the time-related part of the ply-undo-function.
if we hit the low limit of the undo stack, this function will change
the current time control mode to "time per move". But it will not
save the configuration so that the next game will start with what the
user actually has configured.
note that the user has already been asked for confirmation at this
point, so we can savely change the time mode.
the ply number is the one of the ply being retracted, so the caller
must decrement the ply counter himself.*/
void Time_Undo(int plynumber)
{
    if ((CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TPM)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_ANA)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_MTI)))
        return;
    /*time control only applies for game-in and tournament modes*/

    if (plynumber < 0)
        /*should not happen, but just be sure not to mess up things here.*/
        return;

    if (time_keeping.time_used_index <= 2)
        /*we hit the low stack limit, change to time per move and exit.
        the confirmation dialogue has already taken place in the UI.*/
    {
        CFG_SET_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TPM);
        return;
    }

    time_keeping.time_used_index--;

    if ((plynumber & 1L) == 0) /*white's time account*/
    {
        /*take the time after white's move BEFORE this one, without boni*/
        time_keeping.remaining_white_time = time_keeping.time_used_buffer[time_keeping.time_used_index - 2];
    } else
    /*black's timeaccount*/
    {
        /*take the time after black's move BEFORE this one, without boni*/
        time_keeping.remaining_black_time = time_keeping.time_used_buffer[time_keeping.time_used_index - 2];
    }
}

/*this does the time-related part of the ply-redo-function.
the checking whether there is something to redo is being done elsewhere.*/
void Time_Redo(int plynumber)
{
    if ((CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TPM)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_ANA)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_MTI)))
        return;
    /*time control only applies for game-in and tournament modes*/

    /*take the time that was already there at the end of this ply (before undo), no boni*/
    if ((plynumber & 1L) == 0) /*white's time account*/
    {
        time_keeping.remaining_white_time = time_keeping.time_used_buffer[time_keeping.time_used_index];
    } else
        /*black's timeaccount*/
    {
        time_keeping.remaining_black_time = time_keeping.time_used_buffer[time_keeping.time_used_index];
    }
    time_keeping.time_used_index++;
}

/*init the time budget*/
void Time_Init_Game(enum E_COLOUR computer_side, int plynumber)
{
    int i;

    for (i=0; i < MAX_TIME_UNDO; i++)
        time_keeping.time_used_buffer[i] = 0;

    time_keeping.time_used_index = 0;
    time_keeping.dialogue_conf_time = 0;
    time_keeping.remaining_white_time = 0;
    time_keeping.remaining_black_time = 0;
    if ((CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TPM)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_ANA)) ||
        (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_MTI)))
        return;
    /*time control only applies for game-in and tournament modes*/
    if (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_GMI))
    {
        time_keeping.remaining_white_time = time_keeping.remaining_black_time = Get_Game_In_Total_Time();
        if (computer_side != NONE)
            /*if playing against the computer, use the possibility that the user may want to get more time*/
        {
            int32_t factor;
            switch (CFG_GET_OPT(CFG_USER_TIME_MODE))
            {
            case CFG_USER_TIME_LV1:
                factor = 2L;
                break;
            case CFG_USER_TIME_LV2:
                factor = 3L;
                break;
            case CFG_USER_TIME_LV3:
                factor = 4L;
                break;
            case CFG_USER_TIME_LV0:
            default:
                factor = 1L;
                break;
            }
            if (computer_side == BLACK)
                time_keeping.remaining_white_time *= factor;
            else
                time_keeping.remaining_black_time *= factor;
        }
    }
    /*at this point, it must be the tournament setting*/
    if (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TRN))
    {
        if (CFG_HAS_OPT(CFG_TRN_MODE,CFG_TRN_LV0))
            time_keeping.remaining_white_time = time_keeping.remaining_black_time = CFG_TRN_LV0_M40 * MILLISECONDS;

        if (CFG_HAS_OPT(CFG_TRN_MODE,CFG_TRN_LV1))
            time_keeping.remaining_white_time = time_keeping.remaining_black_time = CFG_TRN_LV1_M40 * MILLISECONDS;

        if (CFG_HAS_OPT(CFG_TRN_MODE,CFG_TRN_LV2))
            time_keeping.remaining_white_time = time_keeping.remaining_black_time = CFG_TRN_LV2_M40 * MILLISECONDS;

        if (CFG_HAS_OPT(CFG_TRN_MODE,CFG_TRN_LV3))
            time_keeping.remaining_white_time = time_keeping.remaining_black_time = CFG_TRN_LV3_M40 * MILLISECONDS;
    }

    /*the time before the first move of each colour, without boni*/
    time_keeping.time_used_buffer[0] = time_keeping.remaining_white_time;
    time_keeping.time_used_buffer[1] = time_keeping.remaining_black_time;

    if (plynumber == 1)
    /*happens in the re-init when switching colours in the initial position. The time shall
      stay with the player instead of the colour to accomodate asymmetric time settings.*/
    {
        time_keeping.time_used_buffer[2] = time_keeping.remaining_white_time;
        time_keeping.time_used_index = 3;
    }
    else
        time_keeping.time_used_index = 2;

}

/*time initialisation before a computer move.*/
int32_t NEVER_INLINE Time_Init(int plynumber)
{
    int32_t move_time;
    move_time = Get_Time_For_Move(plynumber);
    start_time = Hw_Get_System_Time();
    stop_time = start_time + move_time;
    return (move_time);
}

/*for the in-check time reduction request.*/
void Time_Override_Stop_Time(int32_t new_move_time)
{
    stop_time = start_time + new_move_time + time_keeping.dialogue_conf_time;
}


/*time initialisation before the player moves.*/
void Time_Set_Start(int plynumber)
{
    int32_t remaining_time, tmp;

    switch (CFG_GET_OPT(CFG_GAME_MODE)) /*what kind of time control is applicable?*/
    {
    case CFG_GAME_MODE_TPM:
    case CFG_GAME_MODE_ANA:
    case CFG_GAME_MODE_MTI:
        /*in TPM and analysis mode, the clock counts upwards, starting with 0s.*/
        time_keeping.next_full_second = 1L * MILLISECONDS;
        break; /*of TPM / ANA /MTI mode*/
    case CFG_GAME_MODE_GMI:
    case CFG_GAME_MODE_TRN:
        if ((plynumber & 1L) == 0) /*white's time*/
            remaining_time = time_keeping.remaining_white_time;
        else
            remaining_time = time_keeping.remaining_black_time;
        tmp = remaining_time % MILLISECONDS; /*the fractional part of the time*/
        if (tmp != 0)
            time_keeping.next_full_second = remaining_time - tmp;
        else
            time_keeping.next_full_second = remaining_time - MILLISECONDS;
        break; /*of game-in / TRN mode*/
    default:
        /*we should never get here*/
        break;
    }

    start_time = Hw_Get_System_Time();
}

/*set used time to zero, used for the game screen buildup.*/
void NEVER_INLINE Time_Set_Current(void)
{
    start_time = Hw_Get_System_Time();
    time_keeping.dialogue_conf_time = 0;
}

/*stop time accounting after either side has made a move. Note that the computer
might need less time than foreseen e.g. in case he only has exactly one legal
move, or when a mate cutoff during search hits in.*/
void NEVER_INLINE Time_Set_Stop(void)
{
    stop_time = Hw_Get_System_Time();
}

/*used after white's first move (if the player has white) to make sure
that this move does not count for the timekeeping.*/
void NEVER_INLINE Time_Cancel_Used_Time(void)
{
    stop_time = start_time;
    /*dialogue confirmations do not add up if the time used is disregarded anyway.*/
    time_keeping.dialogue_conf_time = 0;
}

/*checks how much time was effectively needed for this move until now.*/
int32_t Time_Passed(void)
{
    return(Hw_Get_System_Time() - start_time - time_keeping.dialogue_conf_time);
}

/*adds some confirmation time from whereever to the overall one for this move*/
void NEVER_INLINE Time_Add_Conf_Delay(int32_t conf_time)
{
    time_keeping.dialogue_conf_time += conf_time;
}

/*clears the confirmation time. That is needed for switching sides when a dialogue was open.*/
void NEVER_INLINE Time_Clear_Conf_Delay(void)
{
    time_keeping.dialogue_conf_time = 0;
}

/*checks whether the computer's move time is up.*/
int Time_Check(enum E_COLOUR colour)
{
    int32_t conf_time, system_time;

    /*inform the user when the battery status changes. Since Time_Check() is called during
    computer thinking which draws maximum current, this is one of the important points to catch this.*/

    if (UNLIKELY(Hmi_Battery_Info(COMP_TURN, &conf_time )))
    {
        /*add the time delay. This will be taken care of in Time_Countdown().*/
        stop_time += conf_time;
        Time_Add_Conf_Delay(conf_time);
    }

    Hw_Trigger_Watchdog();
    system_time = Hw_Get_System_Time();
    Time_Intermediate_Display(colour, system_time, DISP_CURSOR_OFF);
    if (UNLIKELY(system_time >= stop_time)) {
        return(1);
    }

    return(0);
}

/*a waiting routine using the system timer.
  can use CPU sleep to save energy or run busy for battery load test.
  does not trigger the watchdog.*/
volatile uint32_t time_spin;
void NEVER_INLINE Time_Delay(int32_t milliseconds, enum E_WAIT_SLEEP sleep_mode)
{
    int32_t delay_end = Hw_Get_System_Time() + milliseconds;

    if (delay_end < MAX_SYS_TIME)
    {
        do {
            if (sleep_mode == SLEEP_ALLOWED)
                Hw_Sleep();
            else
            {
                uint32_t i;

                /*do increment as dummy work*/
                for (i = 8U; i > 0; i--)
                {
                    time_spin++; time_spin++;
                    time_spin++; time_spin++;
                }
            }
        } while (Hw_Get_System_Time() < delay_end);
    }
}

/*to be called for user triggered interrupting an ongoing move
calculation - to be called from an interrupt.
it looks like a dirty hack to abuse the "time over" mechanism for a user
triggered keyboard event on ARM, but the design rationale is to avoid a
second, independent abort mechanism that cannot even be tested in the PC
version.*/
void Time_Enforce_Comp_Move(void)
{
    time_is_up = TM_USER_CANCEL;
}

/*called from the interrupt for switching between search displays,
  and reset via the main play loop, just in case.*/
void Time_Enforce_Disp_Toggle(enum E_TIME_DISP new_state)
{
    disp_toggle = new_state;
}

/*the game was lost due to time. Change to TPM and manage the display.*/
void Time_Change_To_TPM_After_Loss(void)
{
    int32_t time_amount;

    CFG_SET_OPT(CFG_GAME_MODE,CFG_GAME_MODE_TPM);

    /*effective time for the ongoing move*/
    time_amount =  Time_Passed();

    /*set the next full second for the update*/
    time_keeping.next_full_second = (time_amount / 1000L) * 1000L + MILLISECONDS;

    /*erase the time of the other player which isn't needed in TPM mode*/
    Hmi_Erase_Second_Player_Time();

    /*display the current time*/
    Hmi_Update_Running_Time(time_amount, time_amount, DISP_CURSOR_OFF, NO_DISP_TOGGLE);
}

int32_t Time_Get_Current_Player(int plynumber, unsigned int *upwards)
{
    int32_t ret;

    switch (CFG_GET_OPT(CFG_GAME_MODE)) /*what kind of time control is applicable?*/
    {
    case CFG_GAME_MODE_TPM:
    case CFG_GAME_MODE_ANA:
    case CFG_GAME_MODE_MTI:
        /*in TPM and analysis mode, the clock counts upwards, starting with 0s.*/
        *upwards = 1U;
        ret = 0;
        break; /*of TPM / ANA / MTI mode*/
    case CFG_GAME_MODE_GMI:
    case CFG_GAME_MODE_TRN:
        *upwards = 0;
        if ((plynumber & 1L) == 0) /*white's time*/
            ret = time_keeping.remaining_white_time;
        else
            ret = time_keeping.remaining_black_time;
        break; /*of game-in / TRN mode*/
    default:
        *upwards = 0;
        ret = 0;
        /*we should never get here*/
        break;
    }
    return(ret);
}

/*fill in our module details for saving*/
void Time_Save_Status(BACKUP_GAME *ptr, int32_t system_time, enum E_SAVE_TYPE request_autosave)
{
    if ( (request_autosave == HW_MANUAL_SAVE) &&
         ((CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_GMI)) ||
          (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TRN))) )
    {
        /*if the game has been saved manually, then this is AFTER the player
          has received player bonus and Fischer delay. Upon loading, however,
          he will receive it again. So subtract the boni from the current
          player's time.
          these boni only are used with time modes "game-in" and "tournament".*/
        int white_to_move;
        int32_t subtract_time;

        /*black_started_game==1 is not possible with game-in and tournament
          modes as of V1.30, but in case this should change later, there won't
          be a strange time save bug to hunt down.*/
        white_to_move = (((mv_stack_p+black_started_game) & 1L) == 0);

        /*that's the add-on time the player will receive again after loading.*/
        subtract_time = Get_Player_Time_Bonus() + Get_Fischer_Time_Bonus();

        /*subtract time for the saved game.*/
        if (white_to_move)
            time_keeping.remaining_white_time -= subtract_time;
        else
            time_keeping.remaining_black_time -= subtract_time;

        Util_Memcpy(&(ptr->time_keeping), &(time_keeping), sizeof(TIME_KEEPING));

        /*add back time for the ongoing game.*/
        if (white_to_move)
            time_keeping.remaining_white_time += subtract_time;
        else
            time_keeping.remaining_black_time += subtract_time;
    } else
        Util_Memcpy(&(ptr->time_keeping), &(time_keeping), sizeof(TIME_KEEPING));

    ptr->start_time = start_time - system_time;
    ptr->stop_time  = stop_time  - system_time;
}

/*load our module details from saved game, already adjusted to the current system time*/
void Time_Load_Status(const BACKUP_GAME *ptr, int32_t system_time)
{
    start_time = ptr->start_time + system_time;
    stop_time  = ptr->stop_time  + system_time;
    Util_Memcpy(&(time_keeping), &(ptr->time_keeping), sizeof(TIME_KEEPING));
}
