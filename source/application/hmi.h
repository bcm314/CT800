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

void        Hmi_Restore_Viewport(void);
void        Hmi_Disp_Move_Char(char c, int chars_entered);
void        Hmi_Disp_Move_Line(char* buffer, int chars_entered);
void        Hmi_Clear_Input_Move(void);
void NEVER_INLINE Hmi_Show_Start_Screen(void);
void NEVER_INLINE Hmi_Show_Bat_Wait_Screen(int32_t wait_time);
void        Hmi_Setup_System(void);
enum E_HMI_MENU NEVER_INLINE Hmi_Disp_Movelist(int black_started_game, enum E_HMI_MEN_POS menu_mode);
char        Hmi_Get_Piece_Char(int type, enum E_DISP_CASE case_mode);
void        Hmi_Clear_Pretty_Print(void);
void        Hmi_Init_Board(char *viewport, int white_bottom);
void        Hmi_Print_Piecelist(char *viewport_line, enum E_COLOUR colour, int queens, int rooks,
                                int lightbishops, int darkbishops, int knights, int pawns);
void        Hmi_Add_Material_Disp(char *board_viewport, int material);
enum E_HMI_MENU NEVER_INLINE Hmi_Display_Current_Board(int white_bottom, int32_t *conf_time,
                                                       enum E_HMI_REST_MODE restore_viewport);
void NEVER_INLINE Hmi_Game_Of_Life(void);
void        Hmi_Copy_Time_Cache(void);
void        Hmi_Update_Running_Time(int32_t disp_ms, int32_t raw_ms, int cursorpos, enum E_TIME_DISP disp_toggle);
void        Hmi_Erase_Second_Player_Time(void);
enum E_HMI_USER Hmi_Conf_Dialogue(const char *line1, const char *line2, int32_t *conf_time,
                                  int32_t timeout, enum E_HMI_DIALOGUE dialogue_mode,
                                  enum E_HMI_REST_MODE restore_viewport);
void        Hmi_Reboot_Dialogue(const char *line1, const char *line2);
void        Hmi_Set_Bat_Display(char *viewport);
int         Hmi_Battery_Info(enum E_WHOSE_TURN side_to_move, int32_t *conf_time);
void        Hmi_Battery_Shutdown(enum E_WHOSE_TURN side_to_move);
void NEVER_INLINE Hmi_Build_Mating_Screen(int targetdepth);
void NEVER_INLINE Hmi_Update_Analysis_Screen(int32_t time_passed, int eval, int depth, LINE* pv);
void NEVER_INLINE Hmi_Update_Alternate_Screen(int eval, int depth, LINE* pv);
void NEVER_INLINE Hmi_Build_Analysis_Screen(int black_started_game);
void        Hmi_Prepare_Pretty_Print(int black_started_game);
void NEVER_INLINE Hmi_Build_Game_Screen(int computerside, int black_started_game, int game_started_from_0,
                                        enum E_HMI_CONF confirm, GAME_INFO *game_info);
int NEVER_INLINE Hmi_Menu(int32_t *conf_time, int plynumber);
void        Hmi_Set_Cursor(int viewport_pos, int active);
void        Hmi_Save_Status(BACKUP_GAME *ptr);
void        Hmi_Load_Status(BACKUP_GAME *ptr);
void        Hmi_Signal(enum E_HMI_MSG msg_id);
