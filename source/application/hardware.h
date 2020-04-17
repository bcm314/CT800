/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2015-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of the CT800 (hardware interface functions).
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

/*filled rectangle character from the display font*/
#define HD_DISP_BOX_FL  '\xff'

/*custom ARM display characters for the progress bar*/
#define HD_DISP_BOX     0x01U
#define HD_DISP_LEFT    0x02U
#define HD_DISP_RIGHT   0x03U

/*alternate semi-graphic character mappings. 0x01u still free.*/
#define HD_DISP_BOX_UP  0x02U
#define HD_DISP_BOX_DN  0x03U

enum E_KEY      Hw_Getch(enum E_WAIT_SLEEP sleep_mode);
void            Hw_Sleep(void);
void            Hw_Set_Speed(enum E_SYS_SPEED speed, enum E_SYS_MODE mode, enum E_CLK_FORCE force_high);
void            Hw_Throttle_Speed(void);
void            Hw_Setup_System(void);
void            Hw_Powerdown_System(void);
int32_t         Hw_Get_System_Time(void);
void            Hw_Set_System_Time(int32_t new_time);
int             Hw_Battery_Newgame_Ok(void);
void            Hw_Set_Bat_Mon_Callback(void (*Bat_Mon_Callback)(enum E_WHOSE_TURN side_to_move));
void            Hw_Trigger_Watchdog(void);
void            Hw_Sys_Reset(void);
void            Hw_Save_Config(const uint64_t *config);
void            Hw_Set_Default_Config(uint64_t *config);
void            Hw_Load_Config(uint64_t *config);
enum E_FILEOP   Hw_Load_Game(void);
enum E_FILEOP NEVER_INLINE Hw_Save_Game(enum E_SAVE_TYPE request_autosave);
enum E_FILEOP   Hw_Erase_Game(void);
void            Hw_Seed(void);
uint32_t        Hw_Rand(void);
unsigned int    Hw_Check_FW_Image(void);
unsigned int    Hw_Check_RAM_ROM_XTAL_Keys(void);
unsigned int    Hw_Get_Reset_Cause(void);
void            Hw_Clear_Reset_Cause(void);
enum E_AUTOSAVE Hw_Tell_Autosave_State(void);
void            Hw_User_Interaction_Passed(void);

/*hardware_arm_disp.c*/
void            Hw_Disp_Set_Cursor(int line, int col, int active);
void            Hw_Disp_Update(const char *viewport, int pos, int len);
void            Hw_Disp_Show_All(const char *viewport, enum E_HW_DISP mode);
void            Hw_Disp_Set_Charset(enum E_HW_CHARSET charset);
void            Hw_Disp_Set_Contrast(unsigned int contrast_percentage);
void            Hw_Disp_Set_Conf_Contrast(void);

/*hardware_arm_sig.c*/
void            Hw_Sig_Send_Msg(enum E_HW_MSG, uint32_t duration, enum E_HW_MSG_PARAM param);
