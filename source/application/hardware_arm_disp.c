/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2015-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of the CT800 (display interface / ARM).
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
/*configuration reference is needed because the display contrast depends
  on the current device configuration.*/
#include "confdefs.h"
#include "util.h"
#include "hardware.h"
#include "arm_driver.h"

/*semi-public functions from other hardware modules*/
void Hw_Wait_Micsecs(uint32_t microseconds);

/*---------- external variables ----------*/
/*-- READ-ONLY --*/
extern uint64_t hw_config;

#define DISPLAY_PIN_RS       GPIO_Pin_5
#define DISPLAY_PIN_EN       GPIO_Pin_6
#define DISPLAY_PIN_D0       GPIO_Pin_7
#define DISPLAY_PIN_D1       GPIO_Pin_8
#define DISPLAY_PIN_D2       GPIO_Pin_9
#define DISPLAY_PIN_D3       GPIO_Pin_10
#define DISPLAY_PIN_D4       GPIO_Pin_11
#define DISPLAY_PIN_D5       GPIO_Pin_12
#define DISPLAY_PIN_D6       GPIO_Pin_13
#define DISPLAY_PIN_D7       GPIO_Pin_14
/*DISPLAY_PIN_ALL without RS pin; that's nearly always set*/
#define DISPLAY_PIN_ALL      (DISPLAY_PIN_EN | DISPLAY_PIN_D0 | DISPLAY_PIN_D1 |\
                              DISPLAY_PIN_D2 | DISPLAY_PIN_D3 | DISPLAY_PIN_D4 |\
                              DISPLAY_PIN_D5 | DISPLAY_PIN_D6 | DISPLAY_PIN_D7)

/*output GPIO*/
#define DISP_GPIO_BSRR       GPIOB_BSRR

/*the display commands for HD44780.
  the driver is not implemented completely - just what's needed.*/
#define HD_DISP_CLR          0x01U
#define HD_DISP_EMS          0x06U
#define HD_DISP_OFF          0x08U
#define HD_DISP_ON_CURS_OFF  0x0CU
#define HD_DISP_ON_CURS_ON   0x0EU
#define HD_DISP_FUNC         0x38U
#define HD_DISP_CGADR        0x40U

/*custom ARM dispay characters for the dialogue boxes*/
#define HD_DISP_BAR     0x00U
#define HD_DISP_UPLEFT  0x04U
#define HD_DISP_UPRIGHT 0x05U
#define HD_DISP_DNLEFT  0x06U
#define HD_DISP_DNRIGHT 0x07U

#define HD_DISP_CHAR_LEN 8U
/*full box*/
static FLASH_ROM const uint8_t Hd_Disp_Box    [HD_DISP_CHAR_LEN + 1U] =
    {0x1FU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x1FU, HD_DISP_CGADR | (HD_DISP_BOX     << 3)};
/*left bar*/
static FLASH_ROM const uint8_t Hd_Disp_Left   [HD_DISP_CHAR_LEN + 1U] =
    {0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, HD_DISP_CGADR | (HD_DISP_LEFT    << 3)};
/*right bar*/
static FLASH_ROM const uint8_t Hd_Disp_Right  [HD_DISP_CHAR_LEN + 1U] =
    {0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x10U, HD_DISP_CGADR | (HD_DISP_RIGHT   << 3)};
/*box filled, upper half*/
static FLASH_ROM const uint8_t Hd_Disp_Box_Up [HD_DISP_CHAR_LEN + 1U] =
    {0x1FU, 0x1FU, 0x1FU, 0x1FU, 0x00U, 0x00U, 0x00U, 0x00U, HD_DISP_CGADR | (HD_DISP_BOX_UP  << 3)};
/*box filled, lower half*/
static FLASH_ROM const uint8_t Hd_Disp_Box_Dn [HD_DISP_CHAR_LEN + 1U] =
    {0x00U, 0x00U, 0x00U, 0x00U, 0x1FU, 0x1FU, 0x1FU, 0x1FU, HD_DISP_CGADR | (HD_DISP_BOX_DN  << 3)};

/*fancy dialogue characters*/
static FLASH_ROM const uint8_t Hd_Disp_Bar    [HD_DISP_CHAR_LEN + 1U] =
    {0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, HD_DISP_CGADR | (HD_DISP_BAR     << 3)};
static FLASH_ROM const uint8_t Hd_Disp_Upleft [HD_DISP_CHAR_LEN + 1U] =
    {0x00U, 0x00U, 0x00U, 0x07U, 0x04U, 0x04U, 0x04U, 0x04U, HD_DISP_CGADR | (HD_DISP_UPLEFT  << 3)};
static FLASH_ROM const uint8_t Hd_Disp_Upright[HD_DISP_CHAR_LEN + 1U] =
    {0x00U, 0x00U, 0x00U, 0x1CU, 0x04U, 0x04U, 0x04U, 0x04U, HD_DISP_CGADR | (HD_DISP_UPRIGHT << 3)};
static FLASH_ROM const uint8_t Hd_Disp_Dnleft [HD_DISP_CHAR_LEN + 1U] =
    {0x04U, 0x04U, 0x04U, 0x07U, 0x00U, 0x00U, 0x00U, 0x00U, HD_DISP_CGADR | (HD_DISP_DNLEFT  << 3)};
static FLASH_ROM const uint8_t Hd_Disp_Dnright[HD_DISP_CHAR_LEN + 1U] =
    {0x04U, 0x04U, 0x04U, 0x1CU, 0x00U, 0x00U, 0x00U, 0x00U, HD_DISP_CGADR | (HD_DISP_DNRIGHT << 3)};

/*writes a byte to the display - can be command, address, or data.*/
static void Hw_Disp_Write_Byte(uint8_t byte)
{
    uint32_t register_bits = DISPLAY_PIN_EN;

    /*gather the desired state of the data lines*/
    register_bits |= (byte & 0x01U) ? DISPLAY_PIN_D0 : DISPLAY_PIN_D0 << 16;
    register_bits |= (byte & 0x02U) ? DISPLAY_PIN_D1 : DISPLAY_PIN_D1 << 16;
    register_bits |= (byte & 0x04U) ? DISPLAY_PIN_D2 : DISPLAY_PIN_D2 << 16;
    register_bits |= (byte & 0x08U) ? DISPLAY_PIN_D3 : DISPLAY_PIN_D3 << 16;
    register_bits |= (byte & 0x10U) ? DISPLAY_PIN_D4 : DISPLAY_PIN_D4 << 16;
    register_bits |= (byte & 0x20U) ? DISPLAY_PIN_D5 : DISPLAY_PIN_D5 << 16;
    register_bits |= (byte & 0x40U) ? DISPLAY_PIN_D6 : DISPLAY_PIN_D6 << 16;
    register_bits |= (byte & 0x80U) ? DISPLAY_PIN_D7 : DISPLAY_PIN_D7 << 16;

    Drv_SETRESET_BITS(DISP_GPIO_BSRR, register_bits);

    /*enable puls width is minimum 450ns*/
    Hw_Wait_Micsecs(2UL);

    /*data get accepted with the falling flank of ENABLE*/
    Drv_SETRESET_BITS(DISP_GPIO_BSRR, DISPLAY_PIN_EN << 16);
    /*give some execution time - 40 microseconds are required*/
    Hw_Wait_Micsecs(80UL);
}

/*writes a command to the display.*/
static void Hw_Disp_Write_Cmd(uint8_t cmd)
{
    Drv_SETRESET_BITS(DISP_GPIO_BSRR, DISPLAY_PIN_RS << 16);
    Hw_Wait_Micsecs(1UL);
    Hw_Disp_Write_Byte(cmd);
    Drv_SETRESET_BITS(DISP_GPIO_BSRR, DISPLAY_PIN_RS);
    Hw_Wait_Micsecs(1UL);

    /*give time to the display for executing CLEAR as long command.
      1.5ms are required,*/
    if (cmd == HD_DISP_CLR)
        Hw_Wait_Micsecs(3000UL);
}

/*gets the RAM display address for the character position that is
to be written. display RAM addresses as per the display datasheet.*/
static uint8_t Hw_Disp_Get_Dd_Addr(int line, int col)
{
    unsigned int dd_ram_addr;
    switch (line)
    {
    case 0:
    default:
        dd_ram_addr = 0;
        break;
    case 1:
        dd_ram_addr = 0x40U;
        break;
    case 2:
        dd_ram_addr = 0x14U;
        break;
    case 3:
        dd_ram_addr = 0x54U;
        break;
    }
    dd_ram_addr += col;
    return(dd_ram_addr);
}

/*defines custom display charactes*/
static void Hw_Disp_Define_Char(const uint8_t *character)
{
    unsigned int i;

    /*set up CG RAM address*/
    Hw_Disp_Write_Cmd(character[HD_DISP_CHAR_LEN]);

    /*set up pixel line data*/
    for (i = 0; i < HD_DISP_CHAR_LEN; i++)
        Hw_Disp_Write_Byte(character[i]);
}

/*configures either the normal charset
  or alternate graphics mapping.*/
void Hw_Disp_Set_Charset(enum E_HW_CHARSET charset)
{
    if (charset == HW_CHARSET_CGOL)
    {
        /*characters for graphics*/
        Hw_Disp_Define_Char(Hd_Disp_Box_Up);
        Hw_Disp_Define_Char(Hd_Disp_Box_Dn);
    } else
    {
        /*characters for the progress bar*/
        Hw_Disp_Define_Char(Hd_Disp_Box);
        Hw_Disp_Define_Char(Hd_Disp_Left);
        Hw_Disp_Define_Char(Hd_Disp_Right);
    }
}

/*switches the display off or on*/
void Hw_Disp_Switch(enum E_HW_DISP_ONOFF set_state)
{
    switch (set_state)
    {
    case HW_DISP_OFF:
        Hw_Disp_Write_Cmd(HD_DISP_OFF);
        break;
    case HW_DISP_ON:
        Hw_Disp_Write_Cmd(HD_DISP_ON_CURS_OFF);
        break;
    default:
        break;
    }
}

/*switches the display on*/
void Hw_Disp_Init(void)
{
    /*give powerup time to the display*/
    Hw_Wait_Micsecs(20000UL);

    /*it looks silly to do three times the same command, but this is the
    safest way in case the display is in some strange state after powerup.*/
    Hw_Disp_Write_Cmd(HD_DISP_FUNC);
    Hw_Wait_Micsecs(6000UL);
    Hw_Disp_Write_Cmd(HD_DISP_FUNC);
    Hw_Wait_Micsecs(150UL);
    Hw_Disp_Write_Cmd(HD_DISP_FUNC);

    Hw_Disp_Write_Cmd(HD_DISP_OFF);
    Hw_Disp_Write_Cmd(HD_DISP_CLR);
    Hw_Disp_Write_Cmd(HD_DISP_EMS);
    Hw_Disp_Write_Cmd(HD_DISP_ON_CURS_OFF);

    /*apply standard charset with fancy dialogue boxes*/
    Hw_Disp_Set_Charset(HW_CHARSET_NORM);

    /*characters for the dialogue boxes,
      not selectable by dynamic config*/
    Hw_Disp_Define_Char(Hd_Disp_Bar);
    Hw_Disp_Define_Char(Hd_Disp_Upleft);
    Hw_Disp_Define_Char(Hd_Disp_Upright);
    Hw_Disp_Define_Char(Hd_Disp_Dnleft);
    Hw_Disp_Define_Char(Hd_Disp_Dnright);

    /*per default: RS set because much more data than commands are written.*/
    Drv_SETRESET_BITS(DISP_GPIO_BSRR, DISPLAY_PIN_RS);
}

/*line: 0-3 (0 is topmost line), col: 0-19 (0 is leftmost column)*/
void Hw_Disp_Set_Cursor(int line, int col, int active)
{
    uint8_t dd_ram_addr;

    dd_ram_addr = Hw_Disp_Get_Dd_Addr(line, col);
    dd_ram_addr |= 0x80U; /*that's the command flag for setting the write address*/

    if (active == DISP_CURSOR_ON)
    {
        Hw_Disp_Write_Cmd(dd_ram_addr);
        Hw_Disp_Write_Cmd(HD_DISP_ON_CURS_ON);
    } else /*no blinking cursor*/
    {
        Hw_Disp_Write_Cmd(HD_DISP_ON_CURS_OFF);
        Hw_Disp_Write_Cmd(dd_ram_addr);
    }
    Drv_SETRESET_BITS(DISP_GPIO_BSRR, DISPLAY_PIN_ALL << 16);
}

/*draws the complete display*/
void Hw_Disp_Output(const char *viewport)
{
    unsigned int i;

    /*set cursor to 0,0*/
    Hw_Disp_Write_Cmd(0x80U | 0x00U);

    for (i = 0; i < 20; i++)
        Hw_Disp_Write_Byte((uint8_t) viewport[i]);

    /*the display has line 0 and line 2 as consecutive*/

    for (i = 40; i < 60; i++)
        Hw_Disp_Write_Byte((uint8_t) viewport[i]);

    /*set cursor to column 0, line 1*/
    Hw_Disp_Write_Cmd(0x80U | 0x40U);

    for (i = 20; i < 40; i++)
        Hw_Disp_Write_Byte((uint8_t) viewport[i]);

    /*the display has line 1 and 3 as consecutive*/

    for (i = 60; i < 80; i++)
        Hw_Disp_Write_Byte((uint8_t) viewport[i]);

    Drv_SETRESET_BITS(DISP_GPIO_BSRR, DISPLAY_PIN_ALL << 16);
}

/*allows the caller to convert the viewport into a fancy dialogue*/
void Hw_Disp_Viewport_Conv(char *viewport)
{
    viewport[0 ] = HD_DISP_UPLEFT;
    viewport[19] = HD_DISP_UPRIGHT;
    viewport[20] = HD_DISP_BAR;
    viewport[39] = HD_DISP_BAR;
    viewport[40] = HD_DISP_BAR;
    viewport[59] = HD_DISP_BAR;
    viewport[60] = HD_DISP_DNLEFT;
    viewport[79] = HD_DISP_DNRIGHT;
}

/*displays the full display content, but optionally with character
  replacement for dialogues, i.e. with nicer edges and vertical borders.*/
void Hw_Disp_Show_All(const char *viewport, enum E_HW_DISP mode)
{
    if (mode == HW_DISP_RAW)
        Hw_Disp_Output(viewport);
    else
    {
        /*if it is a dialogue, convert it to the fancier display.
          could also be done directly on the input buffer, but that
          would fail unexpectedly if a string constant from flash
          were handed over one day.*/
        char dispport[80];
        Util_Memcpy(dispport, viewport, 80);
        Hw_Disp_Viewport_Conv(dispport);
        Hw_Disp_Output(dispport);
    }
}

/*updates a part of the display instead of re-drawing everything.
  works only for updates within the same text line.*/
void Hw_Disp_Update(const char *viewport, int pos, int len)
{
    int i;
    uint8_t dd_ram_addr;

    dd_ram_addr = Hw_Disp_Get_Dd_Addr( pos / 20U, pos % 20U);
    dd_ram_addr |= 0x80U; /*that's the command flag for setting the write address*/
    Hw_Disp_Write_Cmd(dd_ram_addr);

    for (i = 0; i < len; i++)
        Hw_Disp_Write_Byte((uint8_t) viewport[pos+i]);

    Drv_SETRESET_BITS(DISP_GPIO_BSRR, DISPLAY_PIN_ALL << 16);
}

/*sets the display contrast.
note that high percent values correspond to a low DAC output voltage as the
contrast grows when the voltage difference between DAC output voltage and
display power supply grows.
0% corresponds to 1.5V output. Since 4095dec is 3.3V output, 1861dec is 1.5V.
100% corresponds to an output of 0V.*/
void Hw_Disp_Set_Contrast(unsigned int contrast_percentage)
{
    uint32_t contrast_output;
    if (contrast_percentage >= 100U)
        contrast_output = 0;
    else
    {
        contrast_output = (100UL - contrast_percentage) * 1861UL;
        contrast_output /= 100UL;
    }

    DAC_DHR12R1 = contrast_output;
}

/*sets the display contrast as per the current device configuration.*/
void Hw_Disp_Set_Conf_Contrast(void)
{
    unsigned int disp_contrast;

    disp_contrast = (CFG_GET_OPT(CFG_DISP_MODE)) >> CFG_DISP_OFFSET;
    if (disp_contrast > 10U) /*100%*/
        disp_contrast = 100U;
    else
        disp_contrast *= 10U; /*in steps of 10%*/

    Hw_Disp_Set_Contrast(disp_contrast);
}
