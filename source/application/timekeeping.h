/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2015-2020, Rasmus Althoff <althoff@ct800.net>
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

int32_t NEVER_INLINE Time_Init(int plynumber);
void        Time_Override_Stop_Time(int32_t new_move_time);
void        Time_Set_Start(int plynumber);
void NEVER_INLINE Time_Set_Stop(void);
void NEVER_INLINE Time_Set_Current(void);
void NEVER_INLINE Time_Cancel_Used_Time(void);
int32_t     Time_Passed(void);
int         Time_Check(enum E_COLOUR colour);
void NEVER_INLINE Time_Add_Conf_Delay(int32_t conf_time);
void NEVER_INLINE Time_Clear_Conf_Delay(void);
void NEVER_INLINE Time_Delay(int32_t milliseconds, enum E_WAIT_SLEEP sleep_mode);
void        Time_Enforce_Comp_Move(void);
void        Time_Enforce_Disp_Toggle(enum E_TIME_DISP new_state);
void        Time_Give_Bonus(enum E_COLOUR colour_to_move, enum E_COLOUR computer_side);
void        Time_Countdown(enum E_COLOUR colour_moved, int plynumber);
enum E_TIME_CONTROL Time_Control(enum E_COLOUR colour);
enum E_TIME_INT_CHECK Time_Player_Intermediate_Check(enum E_COLOUR colour, int cursorpos);
int         Time_Undo_OK(void);
void        Time_Undo(int plynumber);
void        Time_Redo(int plynumber);
void        Time_Init_Game(enum E_COLOUR computer_side, int plynumber);
void        Time_Change_To_TPM_After_Loss(void);
int32_t     Time_Get_Current_Player(int plynumber, unsigned int *upwards);
void        Time_Save_Status(BACKUP_GAME *ptr, int32_t system_time, enum E_SAVE_TYPE request_autosave);
void        Time_Load_Status(const BACKUP_GAME *ptr, int32_t system_time);
