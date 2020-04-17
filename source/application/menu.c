/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2015-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of the CT800 (user menu).
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

/*this is the embedded menu system.*/

#include <stdint.h>
#include <stddef.h>
#include "ctdefs.h"
#include "confdefs.h"
#include "hmi.h"
#include "posedit.h"
#include "timekeeping.h"
#include "hardware.h"
#include "util.h"

/*--------- external variables ------------*/
/*-- READ-ONLY  --*/

/*-- READ-WRITE --*/
/*from hardware_pc/arm.c - for changing the config via the menu*/
extern uint64_t hw_config;

/*--------- module variables ------------*/

/*during the game, changing the time settings is restricted because otherwise,
the whole time allocation would not work anymore.*/
static int restricted_mode;

/*start time of the menu. file global because that must be fetched
via the module's game saving/loading API function*/
static int Menu_Start_Time = 0;

/*--------- module static functions ------------*/

/*the position submenu. view/enter position or leave the submenu or the menu altogether.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Pos(int black_started_game)
{
    char menu_buffer[81];
    enum E_HMI_USER dialogue_answer;
    enum E_HMI_MENU user_answer, pos_answer;
    int32_t dummy_conf_time;

    Util_Strcpy(menu_buffer, "position menu       a: view position    b: enter position   c: view notation    ");
    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_CL:
        case KEY_ENT:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_POS_VIEW:
            if (Hmi_Display_Current_Board(HMI_WHITE_BOTTOM, &dummy_conf_time, HMI_BOARD_MENU) == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_POS_EDIT:
            if ((CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TRN)) ||
                (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_GMI)))
                dialogue_answer = Hmi_Conf_Dialogue("set TPM mode and", "enter position?", &dummy_conf_time,
                                  HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
            else
                dialogue_answer = Hmi_Conf_Dialogue("clear board and", "enter position?", &dummy_conf_time,
                                  HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
            if (dialogue_answer == HMI_USER_OK)
            {
                /*time per move is what will work best here, or analysis if it is active anyway.*/
                if ((CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TRN)) ||
                    (CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_GMI)))
                {
                    CFG_SET_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TPM);
                }
                pos_answer = Pos_Editor();
                if (pos_answer == HMI_MENU_NEW_POS)
                {
                    restricted_mode = 1;
                    user_answer = HMI_MENU_NEW_POS;
                }
                else if (pos_answer == HMI_MENU_LEAVE)
                    user_answer = pos_answer;
                else
                    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            } else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_POS_MOVELIST:
            if (Hmi_Disp_Movelist(black_started_game, HMI_MENU_MODE_MEN) == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        default:
            break;
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}

/*the colour submenu. set initial colour setting or leave the submenu.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Misc_Colour(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer;
    uint64_t cfg_option;

#ifdef PC_VERSION
    Util_Strcpy(menu_buffer, "colour menu         use < > keys.       computer starts:                        ");
#else
    Util_Strcpy(menu_buffer, "colour menu         use \x7f \x7e keys.       computer starts:                        ");
#endif

    if (CFG_GET_OPT(CFG_COMP_SIDE_MODE) == CFG_COMP_SIDE_NONE)
    {
        Util_Strins(menu_buffer + 62, "none");
        cfg_option = CFG_COMP_SIDE_NONE;
    } else if (CFG_GET_OPT(CFG_COMP_SIDE_MODE) == CFG_COMP_SIDE_WHITE)
    {
        Util_Strins(menu_buffer + 62, "white");
        cfg_option = CFG_COMP_SIDE_WHITE;
    } else if (CFG_GET_OPT(CFG_COMP_SIDE_MODE) == CFG_COMP_SIDE_BLACK)
    {
        Util_Strins(menu_buffer + 62, "black");
        cfg_option = CFG_COMP_SIDE_BLACK;
    } else /*random*/
    {
        Util_Strins(menu_buffer + 62, "random");
        cfg_option = CFG_COMP_SIDE_RND;
    }

    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_ENT:
            CFG_SET_OPT(CFG_COMP_SIDE_MODE, cfg_option);
        /*falls through*/
        case KEY_CL:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_PLUS:
            if (cfg_option == CFG_COMP_SIDE_NONE)
                cfg_option = CFG_COMP_SIDE_WHITE;
            else if (cfg_option == CFG_COMP_SIDE_WHITE)
                cfg_option = CFG_COMP_SIDE_BLACK;
            else if (cfg_option == CFG_COMP_SIDE_BLACK)
                cfg_option = CFG_COMP_SIDE_RND;
            else if (cfg_option == CFG_COMP_SIDE_RND)
                cfg_option = CFG_COMP_SIDE_NONE;
            break;
        case KEY_MENU_MINUS:
            if (cfg_option == CFG_COMP_SIDE_NONE)
                cfg_option = CFG_COMP_SIDE_RND;
            else if (cfg_option == CFG_COMP_SIDE_RND)
                cfg_option = CFG_COMP_SIDE_BLACK;
            else if (cfg_option == CFG_COMP_SIDE_BLACK)
                cfg_option = CFG_COMP_SIDE_WHITE;
            else if (cfg_option == CFG_COMP_SIDE_WHITE)
                cfg_option = CFG_COMP_SIDE_NONE;
            break;
        default:
            break;
        }

        if ((user_key == KEY_MENU_PLUS) || (user_key == KEY_MENU_MINUS))
        {
            if (cfg_option == CFG_COMP_SIDE_NONE)
                Util_Strins(menu_buffer + 62, "none  ");
            else if (cfg_option == CFG_COMP_SIDE_WHITE)
                Util_Strins(menu_buffer + 62, "white ");
            else if (cfg_option == CFG_COMP_SIDE_BLACK)
                Util_Strins(menu_buffer + 62, "black ");
            else /*random*/
                Util_Strins(menu_buffer + 62, "random");
            Hw_Disp_Update(menu_buffer, 62, 6);
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}

static void Menu_Set_Clock_String(char *menu_buffer, uint64_t cfg_option)
{
    switch (cfg_option)
    {
    case CFG_CLOCK_145:
        Util_Strins(menu_buffer + 62, "145");
        break;
    case CFG_CLOCK_130:
        Util_Strins(menu_buffer + 62, "130");
        break;
    case CFG_CLOCK_070:
        Util_Strins(menu_buffer + 62, " 70");
        break;
    case CFG_CLOCK_050:
        Util_Strins(menu_buffer + 62, " 50");
        break;
    case CFG_CLOCK_025:
        Util_Strins(menu_buffer + 62, " 25");
        break;
    case CFG_CLOCK_010:
        Util_Strins(menu_buffer + 62, " 10");
        break;
    case CFG_CLOCK_100:
    default:
        Util_Strins(menu_buffer + 62, "100");
        break;
    }
}

/*the CPU speed submenu. set CPU speed or leave the submenu.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Misc_Speed(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer;
    uint64_t cfg_option;

#ifdef PC_VERSION
    Util_Strcpy(menu_buffer, "speed menu          use < > keys.       CPU speed:               %              ");
#else
    Util_Strcpy(menu_buffer, "speed menu          use \x7f \x7e keys.       CPU speed:               %              ");
#endif

    cfg_option = CFG_GET_OPT(CFG_CLOCK_MODE);
    Menu_Set_Clock_String(menu_buffer, cfg_option);
    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_ENT:
            CFG_SET_OPT(CFG_CLOCK_MODE, cfg_option);
            if (cfg_option > CFG_CLOCK_100)
            {
                /*we are overclocked - check the firmware image. The point here is not
                the image, but the calculation.*/
                unsigned int sys_test_result;

                Hw_Set_Speed(SYSTEM_SPEED_HIGH, SYSTEM_MODE_USER, CLK_FORCE_HIGH); /*ramp up to overclocked*/
                sys_test_result = Hw_Check_FW_Image();             /*do some lengthy calculation stuff*/
                Hw_Set_Speed(SYSTEM_SPEED_LOW, SYSTEM_MODE_USER, CLK_ALLOW_LOW);  /*back to idle speed*/
                if (sys_test_result != HW_SYSTEM_OK)               /*ooops - overclocked operation is instable!*/
                {
                    int32_t dummy_conf_time;

                    (void) Hmi_Conf_Dialogue("O/C failed,", "normal speed.", &dummy_conf_time,
                                             HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                    CFG_SET_OPT(CFG_CLOCK_MODE, CFG_CLOCK_100);    /*disable overclocking*/
                }
            }
        /*falls through*/
        case KEY_CL:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_PLUS:
            if (cfg_option < CFG_CLOCK_MAX)
                cfg_option += CFG_CLOCK_STEP;
            break;
        case KEY_MENU_MINUS:
            if (cfg_option > CFG_CLOCK_MIN)
                cfg_option -= CFG_CLOCK_STEP;
            break;
        default:
            break;
        }

        if ((user_key == KEY_MENU_PLUS) || (user_key == KEY_MENU_MINUS))
        {
            Menu_Set_Clock_String(menu_buffer, cfg_option);
            Hw_Disp_Update(menu_buffer, 62, 3);
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}

/*the disp submenu. set display contrast in 10% steps or leave the submenu.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Misc_Disp(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer;
    uint32_t cfg_option32;

#ifdef PC_VERSION
    Util_Strcpy(menu_buffer, "display menu        use < > keys.       display contrast:       0%              ");
#else
    Util_Strcpy(menu_buffer, "display menu        use \x7f \x7e keys.       display contrast:       0%              ");
#endif

    cfg_option32 = (uint32_t) ((CFG_GET_OPT(CFG_DISP_MODE)) >> CFG_DISP_OFFSET);

    if (cfg_option32 > 10U) /*should not happen*/
        cfg_option32 = 10U;

    if (cfg_option32 == 10U) /*100 %*/
        Util_Strins(menu_buffer + 62, "10");
    else if (cfg_option32 > 0)
        menu_buffer[63] = ((char) cfg_option32) + '0';

    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        uint64_t cfg_option;
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_ENT:
            cfg_option = ((uint64_t) cfg_option32) << CFG_DISP_OFFSET;
            CFG_SET_OPT(CFG_DISP_MODE, cfg_option);
            user_answer = HMI_MENU_OK;
            break;
        case KEY_CL:
            /*restore the old contrast in the HW layer and leave the menu*/
            Hw_Disp_Set_Conf_Contrast();
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            /*restore the old contrast in the HW layer and leave the menu*/
            Hw_Disp_Set_Conf_Contrast();
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_PLUS:
            if (cfg_option32 < 10U)
                cfg_option32++;
            break;
        case KEY_MENU_MINUS:
            if (cfg_option32 > 0)
                cfg_option32--;
            break;
        default:
            break;
        }

        if ((user_key == KEY_MENU_PLUS) || (user_key == KEY_MENU_MINUS))
        {
            if (cfg_option32 == 10U) /*100 %*/
                Util_Strins(menu_buffer + 62, "10");
            else if (cfg_option32 > 0)
            {
                menu_buffer[62] = ' ';
                menu_buffer[63] = ((char) cfg_option32) + '0';
            } else /*set to 0%*/
                Util_Strins(menu_buffer + 62, "  ");

            Hw_Disp_Set_Contrast(cfg_option32 * 10UL);
            Hw_Disp_Update(menu_buffer, 62, 2);
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}

static void Menu_Set_Light_String(char *menu_buffer, uint64_t cfg_option)
{
    if (cfg_option == CFG_LIGHT_OFF)
        Util_Strins(menu_buffer + 62, "off ");
    else if (cfg_option == CFG_LIGHT_AUTO)
        Util_Strins(menu_buffer + 62, "auto");
    else
        Util_Strins(menu_buffer + 62, "on  ");
}

/*the backlight submenu. set backlight usage or leave the submenu.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Misc_Light(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer;
    uint64_t cfg_option;

#ifdef PC_VERSION
    Util_Strcpy(menu_buffer, "light menu          use < > keys.       light mode:                             ");
#else
    Util_Strcpy(menu_buffer, "light menu          use \x7f \x7e keys.       light mode:                             ");
#endif

    cfg_option = CFG_GET_OPT(CFG_LIGHT_MODE);
    Menu_Set_Light_String(menu_buffer, cfg_option);
    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_ENT:
            CFG_SET_OPT(CFG_LIGHT_MODE, cfg_option);
            if (cfg_option == CFG_LIGHT_OFF)
                Hw_Sig_Send_Msg(HW_MSG_LED_BACK_FADE, BACKLIGHT_FADE, HW_MSG_PARAM_NONE);
            else
                Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_KEY, HW_MSG_PARAM_BACK_CONF);
        /*falls through*/
        case KEY_CL:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_PLUS:
            if (cfg_option == CFG_LIGHT_OFF)
                cfg_option = CFG_LIGHT_AUTO;
            else if (cfg_option == CFG_LIGHT_AUTO)
                cfg_option = CFG_LIGHT_ON;
            else
                cfg_option = CFG_LIGHT_OFF;
            break;
        case KEY_MENU_MINUS:
            if (cfg_option == CFG_LIGHT_OFF)
                cfg_option = CFG_LIGHT_ON;
            else if (cfg_option == CFG_LIGHT_AUTO)
                cfg_option = CFG_LIGHT_OFF;
            else
                cfg_option = CFG_LIGHT_AUTO;
            break;
        default:
            break;
        }

        if ((user_key == KEY_MENU_PLUS) || (user_key == KEY_MENU_MINUS))
        {
            Menu_Set_Light_String(menu_buffer, cfg_option);
            Hw_Disp_Update(menu_buffer, 62, 4);
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}

/*the speaker submenu. set speaker usage or leave the submenu.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Misc_Speaker(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer;
    uint64_t cfg_option, old_cfg_option;

#ifdef PC_VERSION
    Util_Strcpy(menu_buffer, "beep menu           use < > keys.       beep mode:                              ");
#else
    Util_Strcpy(menu_buffer, "beep menu           use \x7f \x7e keys.       beep mode:                              ");
#endif

    if (CFG_GET_OPT(CFG_SPEAKER_MODE) == CFG_SPEAKER_OFF)
    {
        Util_Strins(menu_buffer + 62, "off");
        cfg_option = CFG_SPEAKER_OFF;
    } else if (CFG_GET_OPT(CFG_SPEAKER_MODE) == CFG_SPEAKER_CLICK)
    {
        Util_Strins(menu_buffer + 62, "click");
        cfg_option = CFG_SPEAKER_CLICK;
    } else if (CFG_GET_OPT(CFG_SPEAKER_MODE) == CFG_SPEAKER_COMP)
    {
        Util_Strins(menu_buffer + 62, "computer");
        cfg_option = CFG_SPEAKER_COMP;
    } else /*everything on*/
    {
        Util_Strins(menu_buffer + 62, "on");
        cfg_option = CFG_SPEAKER_ON;
    }

    old_cfg_option = cfg_option;
    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_ENT:
            CFG_SET_OPT(CFG_SPEAKER_MODE, cfg_option);
            /*if we go from silent mode to click, fake the keypad click.
            the other way round, when changing to silent mode, this faking doesn't
            work because the keypad click has already been done in the driver layer.*/
            if ( ((old_cfg_option == CFG_SPEAKER_OFF) || (old_cfg_option == CFG_SPEAKER_COMP)) &&
                 ((cfg_option == CFG_SPEAKER_ON)      || (cfg_option == CFG_SPEAKER_CLICK)) )
                Hw_Sig_Send_Msg(HW_MSG_BEEP_ON, BEEP_CLICK, HW_MSG_PARAM_CLICK);
        /*falls through*/
        case KEY_CL:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_PLUS:
            if (cfg_option == CFG_SPEAKER_OFF)
                cfg_option = CFG_SPEAKER_CLICK;
            else if (cfg_option == CFG_SPEAKER_CLICK)
                cfg_option = CFG_SPEAKER_COMP;
            else if (cfg_option == CFG_SPEAKER_COMP)
                cfg_option = CFG_SPEAKER_ON;
            else /*speaker is on*/
                cfg_option = CFG_SPEAKER_OFF;
            break;
        case KEY_MENU_MINUS:
            if (cfg_option == CFG_SPEAKER_OFF)
                cfg_option = CFG_SPEAKER_ON;
            else if (cfg_option == CFG_SPEAKER_CLICK)
                cfg_option = CFG_SPEAKER_OFF;
            else if (cfg_option == CFG_SPEAKER_COMP)
                cfg_option = CFG_SPEAKER_CLICK;
            else /*speaker is on*/
                cfg_option = CFG_SPEAKER_COMP;
            break;
        default:
            break;
        }

        if ((user_key == KEY_MENU_PLUS) || (user_key == KEY_MENU_MINUS))
        {
            if (cfg_option == CFG_SPEAKER_ON)
                Util_Strins(menu_buffer + 62, "on      ");
            else if (cfg_option == CFG_SPEAKER_CLICK)
                Util_Strins(menu_buffer + 62, "click   ");
            else if (cfg_option == CFG_SPEAKER_OFF)
                Util_Strins(menu_buffer + 62, "off     ");
            else /*computer sounds only*/
                Util_Strins(menu_buffer + 62, "computer");

            Hw_Disp_Update(menu_buffer, 62, 8);
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}

/*the evaluation noise submenu.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Misc_Eval_Noise(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer;
    uint32_t cfg_option32;

#ifdef PC_VERSION
    Util_Strcpy(menu_buffer, "eval menu           use < > keys.       evaluation noise:       0%              ");
#else
    Util_Strcpy(menu_buffer, "eval menu           use \x7f \x7e keys.       evaluation noise:       0%              ");
#endif

    cfg_option32 = (uint32_t) ((CFG_GET_OPT(CFG_NOISE_MODE)) >> CFG_NOISE_OFFSET);

    if (cfg_option32 > 10U) /*should not happen*/
        cfg_option32 = 10U;

    if (cfg_option32 == 10U) /*100 %*/
        Util_Strins(menu_buffer + 62, "10");
    else if (cfg_option32 > 0)
        menu_buffer[63] = ((char) cfg_option32) + '0';

    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        uint64_t cfg_option;
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_ENT:
            cfg_option = ((uint64_t) cfg_option32) << CFG_NOISE_OFFSET;
            CFG_SET_OPT(CFG_NOISE_MODE, cfg_option);
        /*falls through*/
        case KEY_CL:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_PLUS:
            if (cfg_option32 < 10U)
                cfg_option32++;
            break;
        case KEY_MENU_MINUS:
            if (cfg_option32 > 0)
                cfg_option32--;
            break;
        default:
            break;
        }

        if ((user_key == KEY_MENU_PLUS) || (user_key == KEY_MENU_MINUS))
        {
            if (cfg_option32 == 10U) /*100 %*/
                Util_Strins(menu_buffer + 62, "10");
            else if (cfg_option32 > 0)
            {
                menu_buffer[62] = ' ';
                menu_buffer[63] = ((char) cfg_option32) + '0';
            } else /*set to 0%*/
                Util_Strins(menu_buffer + 62, "  ");

            Hw_Disp_Update(menu_buffer, 62, 2);
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}

/*the misc submenu. set starting colour, eval noise, CPU speed,
  display contrast, display backlight mode and beeper mode
  or leave the submenu or the menu altogether.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Misc(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer, submenu_answer;

    Util_Strcpy(menu_buffer, "misc menu           a: colour   d:  dispb: noise    e: lightc: speed    f:  beep");
    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;

    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_CL:
        case KEY_ENT:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_MISC_COL:
            submenu_answer = Menu_Misc_Colour();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_MISC_NOISE:
            submenu_answer = Menu_Misc_Eval_Noise();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_MISC_SPEED:
            submenu_answer = Menu_Misc_Speed();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_MISC_DISP:
            submenu_answer = Menu_Misc_Disp();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_MISC_LIGHT:
            submenu_answer = Menu_Misc_Light();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_MISC_SPEAKER:
            submenu_answer = Menu_Misc_Speaker();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
         default:
            break;
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}

/*the player bonus submenu. set player time bonus or leave the submenu.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Boni_Player(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer;
    int32_t dummy_conf_time;
    uint64_t cfg_option;

#ifdef PC_VERSION
    Util_Strcpy(menu_buffer, "player menu         use < > keys.       player bonus:                           ");
#else
    Util_Strcpy(menu_buffer, "player menu         use \x7f \x7e keys.       player bonus:                           ");
#endif

    if (CFG_GET_OPT(CFG_PLAYER_BONUS_MODE) == CFG_PLAYER_BONUS_OFF)
    {
        Util_Strins(menu_buffer + 62, "off");
        cfg_option = CFG_PLAYER_BONUS_OFF;
    } else
    {
        Util_Strins(menu_buffer + 62, "10s");
        cfg_option = CFG_PLAYER_BONUS_ON;
    }

    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_ENT:
            if ((!(CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_GMI))) &&
                    (!(CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TRN))) &&
                    (cfg_option != CFG_PLAYER_BONUS_OFF))
                (void) Hmi_Conf_Dialogue("only active in", "GMI or TRN mode.", &dummy_conf_time,
                                         HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
            CFG_SET_OPT(CFG_PLAYER_BONUS_MODE, cfg_option);
        /*falls through*/
        case KEY_CL:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_PLUS:
        case KEY_MENU_MINUS:
            if (cfg_option == CFG_PLAYER_BONUS_OFF)
            {
                cfg_option = CFG_PLAYER_BONUS_ON;
                Util_Strins(menu_buffer + 62, "10s");
            } else
            {
                cfg_option = CFG_PLAYER_BONUS_OFF;
                Util_Strins(menu_buffer + 62, "off");
            }
            Hw_Disp_Update(menu_buffer, 62, 3);
            break;
        default:
            break;
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}


/*the Fischer delay submenu. set Fischer delay or leave the submenu.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Boni_Fischer(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer;
    int32_t dummy_conf_time;
    uint64_t cfg_option;

#ifdef PC_VERSION
    Util_Strcpy(menu_buffer, "Fischer menu        use < > keys.       Fischer delay:                          ");
#else
    Util_Strcpy(menu_buffer, "Fischer menu        use \x7f \x7e keys.       Fischer delay:                          ");
#endif

    if (CFG_GET_OPT(CFG_FISCHER_MODE) == CFG_FISCHER_OFF)
    {
        Util_Strins(menu_buffer + 62, "off");
        cfg_option = CFG_FISCHER_OFF;
    } else if (CFG_GET_OPT(CFG_FISCHER_MODE) == CFG_FISCHER_LV0)
    {
        Util_Strins(menu_buffer + 62, "10s");
        cfg_option = CFG_FISCHER_LV0;
    } else if (CFG_GET_OPT(CFG_FISCHER_MODE) == CFG_FISCHER_LV1)
    {
        Util_Strins(menu_buffer + 62, "20s");
        cfg_option = CFG_FISCHER_LV1;
    } else
    {
        Util_Strins(menu_buffer + 62, "30s");
        cfg_option = CFG_FISCHER_LV2;
    }

    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_ENT:
            if ((!(CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_GMI))) &&
                    (!(CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_TRN))) &&
                    (cfg_option != CFG_FISCHER_OFF))
                (void) Hmi_Conf_Dialogue("only active in", "GMI or TRN mode.", &dummy_conf_time,
                                         HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
            CFG_SET_OPT(CFG_FISCHER_MODE, cfg_option);
        /*falls through*/
        case KEY_CL:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_PLUS:
            if (cfg_option == CFG_FISCHER_OFF)
                cfg_option = CFG_FISCHER_LV0;
            else if (cfg_option == CFG_FISCHER_LV0)
                cfg_option = CFG_FISCHER_LV1;
            else if (cfg_option == CFG_FISCHER_LV1)
                cfg_option = CFG_FISCHER_LV2;
            else if (cfg_option == CFG_FISCHER_LV2)
                cfg_option = CFG_FISCHER_OFF;
            break;
        case KEY_MENU_MINUS:
            if (cfg_option == CFG_FISCHER_OFF)
                cfg_option = CFG_FISCHER_LV2;
            else if (cfg_option == CFG_FISCHER_LV2)
                cfg_option = CFG_FISCHER_LV1;
            else if (cfg_option == CFG_FISCHER_LV1)
                cfg_option = CFG_FISCHER_LV0;
            else if (cfg_option == CFG_FISCHER_LV0)
                cfg_option = CFG_FISCHER_OFF;
            break;
        default:
            break;
        }

        if ((user_key == KEY_MENU_PLUS) || (user_key == KEY_MENU_MINUS))
        {
            if (cfg_option == CFG_FISCHER_OFF)
                Util_Strins(menu_buffer + 62, "off");
            else if (cfg_option == CFG_FISCHER_LV0)
                Util_Strins(menu_buffer + 62, "10s");
            else if (cfg_option == CFG_FISCHER_LV1)
                Util_Strins(menu_buffer + 62, "20s");
            else
                Util_Strins(menu_buffer + 62, "30s");

            Hw_Disp_Update(menu_buffer, 62, 3);
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}


/*the player factor submenu. set game-in time factor for the player or leave the submenu.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Boni_Factor(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer;
    int32_t dummy_conf_time;
    uint64_t cfg_option, old_cfg_option;

#ifdef PC_VERSION
    Util_Strcpy(menu_buffer, "factor menu         use < > keys.       GMI player time:      x                 ");
#else
    Util_Strcpy(menu_buffer, "factor menu         use \x7f \x7e keys.       GMI player time:      x                 ");
#endif

    if (CFG_GET_OPT(CFG_USER_TIME_MODE) == CFG_USER_TIME_LV0)
    {
        menu_buffer[63] = '1';
        cfg_option = CFG_USER_TIME_LV0;
    } else if (CFG_GET_OPT(CFG_USER_TIME_MODE) == CFG_USER_TIME_LV1)
    {
        menu_buffer[63] = '2';
        cfg_option = CFG_USER_TIME_LV1;
    } else if (CFG_GET_OPT(CFG_USER_TIME_MODE) == CFG_USER_TIME_LV2)
    {
        menu_buffer[63] = '3';
        cfg_option = CFG_USER_TIME_LV2;
    } else
    {
        menu_buffer[63] = '4';
        cfg_option = CFG_USER_TIME_LV3;
    }

    /*save the old setting.*/
    old_cfg_option = cfg_option;
    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_ENT:
            if ((restricted_mode == 0) || (old_cfg_option == cfg_option) ||
                    (!(CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_GMI))))
            {
                if (!(CFG_HAS_OPT(CFG_GAME_MODE, CFG_GAME_MODE_GMI)) &&
                        (cfg_option != CFG_USER_TIME_LV0))
                    (void) Hmi_Conf_Dialogue("only active in", "GMI mode.", &dummy_conf_time,
                                             HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                CFG_SET_OPT(CFG_USER_TIME_MODE, cfg_option);
            }
            else
                (void) Hmi_Conf_Dialogue("no initial pos.", "read-only setting.", &dummy_conf_time,
                                         HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
        /*falls through*/
        case KEY_CL:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_PLUS:
            if (cfg_option == CFG_USER_TIME_LV0)
                cfg_option = CFG_USER_TIME_LV1;
            else if (cfg_option == CFG_USER_TIME_LV1)
                cfg_option = CFG_USER_TIME_LV2;
            else if (cfg_option == CFG_USER_TIME_LV2)
                cfg_option = CFG_USER_TIME_LV3;
            else if (cfg_option == CFG_USER_TIME_LV3)
                cfg_option = CFG_USER_TIME_LV0;
            break;
        case KEY_MENU_MINUS:
            if (cfg_option == CFG_USER_TIME_LV0)
                cfg_option = CFG_USER_TIME_LV3;
            else if (cfg_option == CFG_USER_TIME_LV1)
                cfg_option = CFG_USER_TIME_LV0;
            else if (cfg_option == CFG_USER_TIME_LV2)
                cfg_option = CFG_USER_TIME_LV1;
            else if (cfg_option == CFG_USER_TIME_LV3)
                cfg_option = CFG_USER_TIME_LV2;
            break;
        default:
            break;
        }

        if ((user_key == KEY_MENU_PLUS) || (user_key == KEY_MENU_MINUS))
        {
            if (cfg_option == CFG_USER_TIME_LV0)
                menu_buffer[63] = '1';
            else if (cfg_option == CFG_USER_TIME_LV1)
                menu_buffer[63] = '2';
            else if (cfg_option == CFG_USER_TIME_LV2)
                menu_buffer[63] = '3';
            else
                menu_buffer[63] = '4';
            Hw_Disp_Update(menu_buffer, 63, 1);
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}

/*the book submenu. set book usage or leave the submenu.*/
static enum E_HMI_MENU NEVER_INLINE Menu_File_Book(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer;
    uint64_t cfg_option;

#ifdef PC_VERSION
    Util_Strcpy(menu_buffer, "book menu           use < > keys.       book usage:                             ");
#else
    Util_Strcpy(menu_buffer, "book menu           use \x7f \x7e keys.       book usage:                             ");
#endif

    if (CFG_GET_OPT(CFG_BOOK_MODE) == CFG_BOOK_OFF)
    {
        Util_Strins(menu_buffer + 62, "off");
        cfg_option = CFG_BOOK_OFF;
    } else
    {
        Util_Strins(menu_buffer + 62, "on");
        cfg_option = CFG_BOOK_ON;
    }

    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_ENT:
            CFG_SET_OPT(CFG_BOOK_MODE, cfg_option);
        /*falls through*/
        case KEY_CL:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_PLUS:
        case KEY_MENU_MINUS:
            if (cfg_option == CFG_BOOK_OFF)
            {
                cfg_option = CFG_BOOK_ON;
                Util_Strins(menu_buffer + 62, "on ");
            } else
            {
                cfg_option = CFG_BOOK_OFF;
                Util_Strins(menu_buffer + 62, "off");
            }
            Hw_Disp_Update(menu_buffer, 62, 3);
            break;
        default:
            break;
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}

/*the file submenu. load/save or leave the submenu or the menu altogether.*/
static enum E_HMI_MENU NEVER_INLINE Menu_File(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer;
    int32_t dummy_conf_time;

    Util_Strcpy(menu_buffer, "file menu           a: load     d:  bookb: save     e: resetc: erase            ");
    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_FILEOP file_result;
        enum E_HMI_USER dialogue_answer;
        enum E_HMI_MENU submenu_answer;
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_CL:
        case KEY_ENT:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_FILE_LOAD:
            file_result = Hw_Load_Game();
            if (file_result != HW_FILEOP_FAILED)
            {
                if (file_result == HW_FILEOP_OK_AUTO)
                    (void) Hmi_Conf_Dialogue("loading ok,", "auto-save on.", &dummy_conf_time,
                                             HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                else
                    (void) Hmi_Conf_Dialogue("loading ok,", "auto-save off.", &dummy_conf_time,
                                             HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                user_answer = HMI_MENU_NEW_POS;
            }
            else
            {
                (void) Hmi_Conf_Dialogue("loading failed:", "bad checksum.", &dummy_conf_time,
                                         HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            }
            break;
        case KEY_MENU_FILE_SAVE:
            file_result = Hw_Save_Game(HW_MANUAL_SAVE);
            if (file_result != HW_FILEOP_FAILED)
            {
                if (file_result == HW_FILEOP_OK_AUTO)
                    (void) Hmi_Conf_Dialogue("saving ok,", "auto-save on.", &dummy_conf_time,
                                             HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                else
                    (void) Hmi_Conf_Dialogue("saving ok,", "auto-save off.", &dummy_conf_time,
                                             HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                user_answer = HMI_MENU_LEAVE; /*just be consistent with successful load behaviour.*/
            }
            else
            {
                (void) Hmi_Conf_Dialogue("saving", "failed.", &dummy_conf_time,
                                         HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            }
            break;
        case KEY_MENU_FILE_ERASE:
            file_result = Hw_Erase_Game();
            if (file_result != HW_FILEOP_FAILED)
            {
                if (file_result == HW_FILEOP_OK_AUTO)
                    (void) Hmi_Conf_Dialogue("erasing ok,", "auto-save on.", &dummy_conf_time,
                                             HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                else
                    (void) Hmi_Conf_Dialogue("erasing ok,", "auto-save off.", &dummy_conf_time,
                                             HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                user_answer = HMI_MENU_LEAVE; /*just be consistent with successful load behaviour.*/
            }
            else
            {
                (void) Hmi_Conf_Dialogue("erasing", "failed.", &dummy_conf_time,
                                         HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            }
            break;
        case KEY_MENU_FILE_BOOK:
            submenu_answer = Menu_File_Book();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_FILE_RESET:
            dialogue_answer = Hmi_Conf_Dialogue("set defaults", "and restart?", &dummy_conf_time,
                                                HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
            if (dialogue_answer == HMI_USER_OK)
            {
                uint64_t dummy_config;

                Util_Strcpy(menu_buffer, "+-------WAIT-------+|     about to     ||     restart.     |+------------------+");
                Hw_Disp_Show_All(menu_buffer, HW_DISP_DIALOGUE);
                /*set default config*/
                Hw_Set_Default_Config(&dummy_config);
                /*erase a possibly saved game*/
                (void) Hw_Erase_Game();

                Hw_Sys_Reset();
            }
            Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        default:
            break;
        }
    } while (user_answer == HMI_MENU_INVALID);
    return (user_answer);
}

/*the game mode submenu. set the time control mode or leave the submenu.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Time_Game_Mode(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer;
    int32_t dummy_conf_time;
    uint64_t cfg_option, old_cfg_option, game_mode;

#ifdef PC_VERSION
    Util_Strcpy(menu_buffer, "game mode menu      use < > keys.       game time mode:                         ");
#else
    Util_Strcpy(menu_buffer, "game mode menu      use \x7f \x7e keys.       game time mode:                         ");
#endif

    game_mode = CFG_GET_OPT(CFG_GAME_MODE);

    if (game_mode == CFG_GAME_MODE_TPM)
    {
        Util_Strins(menu_buffer + 62, "time per move: TPM");
        cfg_option = CFG_GAME_MODE_TPM;
    } else if (game_mode == CFG_GAME_MODE_GMI)
    {
        Util_Strins(menu_buffer + 62, "game in: GMI");
        cfg_option = CFG_GAME_MODE_GMI;
    } else if (game_mode == CFG_GAME_MODE_TRN)
    {
        Util_Strins(menu_buffer + 62, "tournament: TRN");
        cfg_option = CFG_GAME_MODE_TRN;
    } else if (game_mode == CFG_GAME_MODE_MTI)
    {
        Util_Strins(menu_buffer + 62, "mate in: MTI");
        cfg_option = CFG_GAME_MODE_MTI;
    } else /*analysis mode*/
    {
        Util_Strins(menu_buffer + 62, "analysis: AN");
        cfg_option = CFG_GAME_MODE_ANA;
    }

    /*save old setting*/
    old_cfg_option = cfg_option;
    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_ENT:
            /*during the game, changing to game-in or tournament mode is not allowed,
                but changing to analysis, time per move mode or mate-in is OK.*/
            if ((restricted_mode == 0) || (old_cfg_option == cfg_option)
                    || (cfg_option == CFG_GAME_MODE_TPM) || (cfg_option == CFG_GAME_MODE_ANA) || (cfg_option == CFG_GAME_MODE_MTI))
            {
                CFG_SET_OPT(CFG_GAME_MODE, cfg_option);
            }
            else
                (void) Hmi_Conf_Dialogue("no initial pos.", "read-only setting.", &dummy_conf_time,
                                         HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
        /*falls through*/
        case KEY_CL:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_PLUS:
            if (cfg_option == CFG_GAME_MODE_TPM)
                cfg_option = CFG_GAME_MODE_GMI;
            else if (cfg_option == CFG_GAME_MODE_GMI)
                cfg_option = CFG_GAME_MODE_TRN;
            else if (cfg_option == CFG_GAME_MODE_TRN)
                cfg_option = CFG_GAME_MODE_ANA;
            else if (cfg_option == CFG_GAME_MODE_ANA)
                cfg_option = CFG_GAME_MODE_MTI;
            else if (cfg_option == CFG_GAME_MODE_MTI)
                cfg_option = CFG_GAME_MODE_TPM;
            break;
        case KEY_MENU_MINUS:
            if (cfg_option == CFG_GAME_MODE_TPM)
                cfg_option = CFG_GAME_MODE_MTI;
            else if (cfg_option == CFG_GAME_MODE_MTI)
                cfg_option = CFG_GAME_MODE_ANA;
            else if (cfg_option == CFG_GAME_MODE_ANA)
                cfg_option = CFG_GAME_MODE_TRN;
            else if (cfg_option == CFG_GAME_MODE_TRN)
                cfg_option = CFG_GAME_MODE_GMI;
            else if (cfg_option == CFG_GAME_MODE_GMI)
                cfg_option = CFG_GAME_MODE_TPM;
            break;
        default:
            break;
        }

        if ((user_key == KEY_MENU_PLUS) || (user_key == KEY_MENU_MINUS))
        {
            if (cfg_option == CFG_GAME_MODE_TPM)
                Util_Strins(menu_buffer + 62, "time per move: TPM");
            else if (cfg_option == CFG_GAME_MODE_GMI)
                Util_Strins(menu_buffer + 62, "game in: GMI      ");
            else if (cfg_option == CFG_GAME_MODE_TRN)
                Util_Strins(menu_buffer + 62, "tournament: TRN   ");
            else if (cfg_option == CFG_GAME_MODE_MTI)
                Util_Strins(menu_buffer + 62, "mate in: MTI      ");
            else /*analysis mode*/
                Util_Strins(menu_buffer + 62, "analysis: AN      ");

            Hw_Disp_Update(menu_buffer, 62, 18);
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}

/*what we display in the time details depends also on the game mode, that's a bit tricky.*/
static void Menu_Time_Details_String(char *menu_buffer, uint64_t game_mode, uint64_t detail_mode)
{
    if (game_mode == CFG_GAME_MODE_TPM)
    {
        if (detail_mode == CFG_TPM_LV0)
            Util_Strins(menu_buffer + 62, "minimal");
        else if (detail_mode == CFG_TPM_LV1)
            Util_Strins(menu_buffer + 62, "  5s   ");
        else if (detail_mode == CFG_TPM_LV2)
            Util_Strins(menu_buffer + 62, " 10s   ");
        else if (detail_mode == CFG_TPM_LV3)
            Util_Strins(menu_buffer + 62, " 20s   ");
        else if (detail_mode == CFG_TPM_LV4)
            Util_Strins(menu_buffer + 62, " 30s   ");
        else if (detail_mode == CFG_TPM_LV5)
            Util_Strins(menu_buffer + 62, " 60s   ");
        else if (detail_mode == CFG_TPM_LV6)
            Util_Strins(menu_buffer + 62, "120s   ");
        else /*LV 7*/
            Util_Strins(menu_buffer + 62, "180s   ");
    } else if (game_mode == CFG_GAME_MODE_GMI)
    {
        if (detail_mode == CFG_GAME_IN_LV0)
            Util_Strins(menu_buffer + 62, " 5min");
        else if (detail_mode == CFG_GAME_IN_LV1)
            Util_Strins(menu_buffer + 62, "10min");
        else if (detail_mode == CFG_GAME_IN_LV2)
            Util_Strins(menu_buffer + 62, "15min");
        else if (detail_mode == CFG_GAME_IN_LV3)
            Util_Strins(menu_buffer + 62, "20min");
        else if (detail_mode == CFG_GAME_IN_LV4)
            Util_Strins(menu_buffer + 62, "30min");
        else if (detail_mode == CFG_GAME_IN_LV5)
            Util_Strins(menu_buffer + 62, "45min");
        else if (detail_mode == CFG_GAME_IN_LV6)
            Util_Strins(menu_buffer + 62, "60min");
        else /*LV 7*/
            Util_Strins(menu_buffer + 62, "90min");
    } else if (game_mode == CFG_GAME_MODE_TRN)
    {
        if (detail_mode == CFG_TRN_LV0)
            Util_Strins(menu_buffer + 62, "40/90+30       ");
        else if (detail_mode == CFG_TRN_LV1)
            Util_Strins(menu_buffer + 62, "40/120+30      ");
        else if (detail_mode == CFG_TRN_LV2)
            Util_Strins(menu_buffer + 62, "40/120+60");
        else /*LV 3*/
            Util_Strins(menu_buffer + 62, "40/120,20/60+30");
    } else if (game_mode == CFG_GAME_MODE_MTI)
    {
        unsigned int mate_depth;

        mate_depth = (unsigned int) ((detail_mode) >> CFG_MTI_OFFSET);
        menu_buffer[62] = ((char) (mate_depth)) + '1';

        if (mate_depth == 0) /*mate in 1 is "move", not "moves".*/
            Util_Strins(menu_buffer + 63, " move ");
        else
            Util_Strins(menu_buffer + 63, " moves");
    } else /*analysis mode*/
        Util_Strins(menu_buffer + 62, "9:00h");
}

/*subtract/add modes, that depends also on the game mode.*/
static void Menu_Time_Details_Logic(uint64_t game_mode, uint64_t *detail_mode, int increment)
{
    if (game_mode == CFG_GAME_MODE_TPM)
    {
        if (*detail_mode == CFG_TPM_LV0)
            *detail_mode = (increment) ? CFG_TPM_LV1 : CFG_TPM_LV7;
        else if (*detail_mode == CFG_TPM_LV1)
            *detail_mode = (increment) ? CFG_TPM_LV2 : CFG_TPM_LV0;
        else if (*detail_mode == CFG_TPM_LV2)
            *detail_mode = (increment) ? CFG_TPM_LV3 : CFG_TPM_LV1;
        else if (*detail_mode == CFG_TPM_LV3)
            *detail_mode = (increment) ? CFG_TPM_LV4 : CFG_TPM_LV2;
        else if (*detail_mode == CFG_TPM_LV4)
            *detail_mode = (increment) ? CFG_TPM_LV5 : CFG_TPM_LV3;
        else if (*detail_mode == CFG_TPM_LV5)
            *detail_mode = (increment) ? CFG_TPM_LV6 : CFG_TPM_LV4;
        else if (*detail_mode == CFG_TPM_LV6)
            *detail_mode = (increment) ? CFG_TPM_LV7 : CFG_TPM_LV5;
        else /*LV 7*/
            *detail_mode = (increment) ? CFG_TPM_LV0 : CFG_TPM_LV6;
    } else if (game_mode == CFG_GAME_MODE_GMI)
    {
        if (*detail_mode == CFG_GAME_IN_LV0)
            *detail_mode = (increment) ? CFG_GAME_IN_LV1 : CFG_GAME_IN_LV7;
        else if (*detail_mode == CFG_GAME_IN_LV1)
            *detail_mode = (increment) ? CFG_GAME_IN_LV2 : CFG_GAME_IN_LV0;
        else if (*detail_mode == CFG_GAME_IN_LV2)
            *detail_mode = (increment) ? CFG_GAME_IN_LV3 : CFG_GAME_IN_LV1;
        else if (*detail_mode == CFG_GAME_IN_LV3)
            *detail_mode = (increment) ? CFG_GAME_IN_LV4 : CFG_GAME_IN_LV2;
        else if (*detail_mode == CFG_GAME_IN_LV4)
            *detail_mode = (increment) ? CFG_GAME_IN_LV5 : CFG_GAME_IN_LV3;
        else if (*detail_mode == CFG_GAME_IN_LV5)
            *detail_mode = (increment) ? CFG_GAME_IN_LV6 : CFG_GAME_IN_LV4;
        else if (*detail_mode == CFG_GAME_IN_LV6)
            *detail_mode = (increment) ? CFG_GAME_IN_LV7 : CFG_GAME_IN_LV5;
        else /*LV 7*/
            *detail_mode = (increment) ? CFG_GAME_IN_LV0 : CFG_GAME_IN_LV6;
    } else if (game_mode == CFG_GAME_MODE_TRN)
    {
        if (*detail_mode == CFG_TRN_LV0)
            *detail_mode = (increment) ? CFG_TRN_LV1 : CFG_TRN_LV3;
        else if (*detail_mode == CFG_TRN_LV1)
            *detail_mode = (increment) ? CFG_TRN_LV2 : CFG_TRN_LV0;
        else if (*detail_mode == CFG_TRN_LV2)
            *detail_mode = (increment) ? CFG_TRN_LV3 : CFG_TRN_LV1;
        else /*LV 3*/
            *detail_mode = (increment) ? CFG_TRN_LV0 : CFG_TRN_LV2;
    } else if (game_mode == CFG_GAME_MODE_MTI)
    {
        int mate_depth = (int) ((*detail_mode) >> CFG_MTI_OFFSET);

        if (increment)
            mate_depth++;
        else
            mate_depth--;
        /*do the wrap-around*/
        if (mate_depth < 0)
            mate_depth = CFG_MTI_MAX;
        if (mate_depth > CFG_MTI_MAX)
            mate_depth = 0;
        *detail_mode = ((uint64_t) mate_depth) << CFG_MTI_OFFSET;
    } else
    {
        /*analysis mode - no changes possible.*/
    }
}

/*the game mode submenu. set the time control mode or leave the submenu.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Time_Details(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer;
    int32_t dummy_conf_time;
    uint64_t cfg_option, old_cfg_option, cfg_mode;
    unsigned int option_length = 0;

    /*what kind of time control are we going to configure here?*/
    cfg_mode = CFG_GET_OPT(CFG_GAME_MODE);

    if (cfg_mode != CFG_GAME_MODE_ANA) /*analysis is not configurable*/
    {
#ifdef PC_VERSION
        Util_Strcpy(menu_buffer, "time details menu   use < > keys.                                               ");
#else
        Util_Strcpy(menu_buffer, "time details menu   use \x7f \x7e keys.                                               ");
#endif
    } else
    {
        Util_Strcpy(menu_buffer, "time details menu   fixed AN time.                                              ");
    }

    switch (cfg_mode)
    {
    case CFG_GAME_MODE_TPM:
        cfg_option = CFG_GET_OPT(CFG_TPM_MODE);
        Util_Strins(menu_buffer + 40, "time per move:");
        option_length = 7; /* 'minimal' , that's 7 characters*/
        break;
    case CFG_GAME_MODE_GMI:
        cfg_option = CFG_GET_OPT(CFG_GAME_IN_MODE);
        Util_Strins(menu_buffer + 40, "game in:");
        option_length = 5; /*e.g. '90min', that's 5 characters*/
        break;
    case CFG_GAME_MODE_TRN:
        cfg_option = CFG_GET_OPT(CFG_TRN_MODE);
        Util_Strins(menu_buffer + 40, "tournament mode:");
        option_length = 15;
        break;
    case CFG_GAME_MODE_MTI:
        cfg_option = CFG_GET_OPT(CFG_MTI_MODE);
        Util_Strins(menu_buffer + 40, "mate in:");
        option_length = 7; /*e.g. '3 moves', that's 7 characters*/
        break;
    case CFG_GAME_MODE_ANA:
    default:
        Util_Strins(menu_buffer + 40, "analysis mode:");
        option_length = 5;
        cfg_option = 0; /*analysis is not configurable*/
        break;
    }

    /*save the initial setting*/
    old_cfg_option = cfg_option;
    Menu_Time_Details_String(menu_buffer, cfg_mode, cfg_option);
    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_ENT:
            if (cfg_mode != CFG_GAME_MODE_ANA) /*analysis mode has no options*/
            {
                switch (cfg_mode)
                {
                case CFG_GAME_MODE_TPM:
                    /*time per move setting may be changed freely during the game.*/
                    CFG_SET_OPT(CFG_TPM_MODE, cfg_option);
                    break;
                case CFG_GAME_MODE_GMI:
                    /*changing the game-in time is only allowed in the initial position.*/
                    if ((restricted_mode == 0) || (old_cfg_option == cfg_option))
                        CFG_SET_OPT(CFG_GAME_IN_MODE, cfg_option);
                    else
                        (void) Hmi_Conf_Dialogue("no initial pos.", "read-only setting.", &dummy_conf_time,
                                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                    break;
                case CFG_GAME_MODE_TRN:
                    /*changing the tournament time is only allowed in the initial position.*/
                    if ((restricted_mode == 0) || (old_cfg_option == cfg_option))
                        CFG_SET_OPT(CFG_TRN_MODE, cfg_option);
                    else
                        (void) Hmi_Conf_Dialogue("no initial pos.", "read-only setting.", &dummy_conf_time,
                                                 HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
                    break;
                case CFG_GAME_MODE_MTI:
                    CFG_SET_OPT(CFG_MTI_MODE, cfg_option);
                    break;
                default:
                    break;
                }
            }
        /*falls through*/
        case KEY_CL:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_PLUS:
            if (cfg_mode != CFG_GAME_MODE_ANA) /*analysis mode has no options*/
                Menu_Time_Details_Logic(cfg_mode, &cfg_option, 1);
            break;
        case KEY_MENU_MINUS:
            if (cfg_mode != CFG_GAME_MODE_ANA) /*analysis mode has no options*/
                Menu_Time_Details_Logic(cfg_mode, &cfg_option, 0);
            break;
        default:
            break;
        }

        if (((user_key == KEY_MENU_PLUS) || (user_key == KEY_MENU_MINUS)) && (cfg_mode != CFG_GAME_MODE_ANA))
        {
            Menu_Time_Details_String(menu_buffer, cfg_mode, cfg_option);
            Hw_Disp_Update(menu_buffer, 62, option_length);
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}

/*the boni submenu. set Fischer and player bonus usage or leave the submenu or the menu altogether.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Time_Boni(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer, submenu_answer;

    Util_Strcpy(menu_buffer, "add-on menu         a: Fischer delay    b: player bonus     c: player factor    ");
    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_CL:
        case KEY_ENT:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_BONI_FISCHER:
            submenu_answer = Menu_Boni_Fischer();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_BONI_PLAYER:
            submenu_answer = Menu_Boni_Player();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_BONI_FACTOR:
            submenu_answer = Menu_Boni_Factor();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        default:
            break;
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}

/*the time submenu. set game mode, time usage and soft time per move or leave the submenu or the menu altogether.*/
static enum E_HMI_MENU NEVER_INLINE Menu_Time(void)
{
    char menu_buffer[81];
    enum E_HMI_MENU user_answer, submenu_answer;

    Util_Strcpy(menu_buffer, "time menu           a: mode             b: details          c: add-ons          ");
    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);

    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_CL:
        case KEY_ENT:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU:
            user_answer = HMI_MENU_LEAVE;
            break;
        case KEY_MENU_GAME_MODE:
            submenu_answer = Menu_Time_Game_Mode();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_TIME_DETAILS:
            submenu_answer = Menu_Time_Details();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_TIME_BONI:
            submenu_answer = Menu_Time_Boni();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_LEAVE;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        default:
            break;
        }
    } while (user_answer == HMI_MENU_INVALID);

    return (user_answer);
}


/*--------- global functions ------------*/



/*displays the main menu. returns whether a new game shall be started, or whether a new
position was entered. this main menu calls the different submenus as separate functions.

Here the menu tree:

a: new game
b: file
    a: load game
    b: save game
    c: erase storage
    d: opening book
    e: reset device
c: position
    a: view position
    b: enter position
    c: view notation
d: time
    a: mode
    b: details
    c: add-ons
        a: Fischer delay
        b: player bonus
        c: player factor
e: misc
    a: player colour
    b: evaluation noise
    c: CPU speed
    d: display contrast
    e: display backlight
    f: beeper
f: info

*/
enum E_HMI_MENU NEVER_INLINE
Menu_Main(int32_t *conf_time, int plynumber, int black_started_game, int game_started_from_0)
{
    char menu_buffer[81];
    enum E_HMI_USER dialogue_answer;
    enum E_HMI_MENU user_answer, submenu_answer;
    int32_t dummy_conf_time;
    uint64_t old_config;

    Menu_Start_Time = Hw_Get_System_Time();
    Hmi_Set_Cursor(0, DISP_CURSOR_OFF);
    old_config = hw_config; /*save the current config*/

    /*allow the full configuration of the time settings only in the initial position, or after
    the first ply by white since white's first ply doesn't count time-wise anyway.*/
    if ((plynumber < 2) && (game_started_from_0))
        restricted_mode = 0;
    else
        restricted_mode = 1;

    Util_Strcpy(menu_buffer, "main menu           a: new game  d: timeb: file      e: miscc: position  f: info");
    Hmi_Set_Bat_Display(menu_buffer);
    Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);


    /*gather user feedback until he gives a valid answer.*/
    user_answer = HMI_MENU_INVALID;
    do
    {
        enum E_KEY user_key = Hw_Getch(SLEEP_ALLOWED);

        switch (user_key)
        {
        case KEY_CL:
        case KEY_MENU:
        case KEY_ENT:
            /*just leave the menu, do nothing*/
            user_answer = HMI_MENU_OK;
            break;
        case KEY_MENU_NEW_GAME:
            dialogue_answer = Hmi_Conf_Dialogue("start", "new game?", &dummy_conf_time,
                                                HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
            if (dialogue_answer == HMI_USER_OK)
            {
                /*avoid restoring the display.*/
                Hmi_Clear_Pretty_Print();
                user_answer = HMI_MENU_NEW_GAME;
            }
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_FILE:
            submenu_answer = Menu_File();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_OK;
            else if (submenu_answer == HMI_MENU_NEW_POS)
                user_answer = HMI_MENU_NEW_POS;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_POS:
            submenu_answer = Menu_Pos(black_started_game);
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_OK;
            else if (submenu_answer == HMI_MENU_NEW_POS)
                user_answer = HMI_MENU_NEW_POS;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_TIME:
            submenu_answer = Menu_Time();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_OK;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_MISC:
            submenu_answer = Menu_Misc();
            if (submenu_answer == HMI_MENU_LEAVE)
                user_answer = HMI_MENU_OK;
            else
                Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        case KEY_MENU_INFO:
            (void) Hmi_Conf_Dialogue(VERSION_INFO_DIALOGUE_LINE_1, VERSION_INFO_DIALOGUE_LINE_2, &dummy_conf_time,
                                     HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
            (void) Hmi_Conf_Dialogue(VERSION_INFO_DIALOGUE_LINE_3, VERSION_INFO_DIALOGUE_LINE_4, &dummy_conf_time,
                                     HMI_NO_TIMEOUT, HMI_INFO, HMI_NO_RESTORE);
            Hw_Disp_Show_All(menu_buffer, HW_DISP_RAW);
            break;
        default:
            break;
        }
    } while (user_answer == HMI_MENU_INVALID);

    if (hw_config != old_config) /*ask whether the new config shall be stored permanently.*/
    {
        if (user_answer != HMI_MENU_NEW_GAME)
            /*if the config was changed AND a new game selected, saving the config is the only way
            to have the modified settings take action in the new game.*/
            dialogue_answer = Hmi_Conf_Dialogue("save new", "configuration?", &dummy_conf_time,
                                                HMI_NO_TIMEOUT, HMI_QUESTION, HMI_NO_RESTORE);
        else
            dialogue_answer = HMI_USER_OK;

        if (dialogue_answer == HMI_USER_OK)
            /*save new config*/
            Hw_Save_Config(&hw_config);
    }

    /*the screen is not restored.*/

    *conf_time  = Hw_Get_System_Time() - Menu_Start_Time; /*so much time was spent in the menu.*/

    return(user_answer);
}

/*fill in the module details for saving*/
void Menu_Save_Status(BACKUP_GAME *ptr, int32_t system_time)
{
    ptr->menu_start_time = Menu_Start_Time - system_time;
}

/*load the module details from saved game*/
void Menu_Load_Status(const BACKUP_GAME *ptr, int32_t system_time)
{
    Menu_Start_Time = ptr->menu_start_time + system_time;
}
