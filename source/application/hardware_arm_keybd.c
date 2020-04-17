/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2015-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of the CT800 (keyboard interface / ARM).
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
/*time keeping reference is needed to enforce computer move if the GO
  key is pressed while the computer is thinking, or toggle the search
  display mode*/
#include "timekeeping.h"
#include "hardware.h"
#include "arm_driver.h"

/*semi-public functions from other hardware modules*/
void Hw_Add_Randomness(void);
void Hw_Wait_Micsecs(uint32_t microseconds);

/*output GPIO*/
#define KEY_GPIO_BSRR        GPIOC_BSRR
#define KEY_GPIO_MODER       GPIOC_MODER
#define KEY_GPIO_PUPDR       GPIOC_PUPDR

/*input GPIO*/
#define KEY_GPIO_IDR         GPIOC_IDR
#define LTREQ_GPIO_IDR       GPIOA_IDR

/*matrix key scan modes*/
enum E_KEY_SCAN {KEY_SCAN_COLS, KEY_SCAN_ROWS};

/*out/in direction of columns/rows*/
enum E_KEY_OUT {KEY_SCAN_COLS_OUT, KEY_SCAN_ROWS_OUT};
enum E_KEY_IN  {KEY_SCAN_COLS_IN,  KEY_SCAN_ROWS_IN };

/*logical key states*/
enum E_KEY_PRESS {KEY_INACTIVE, KEY_ACTIVE};

/*the following are for all keys (except backlight key)*/

/*currently measured bit mask from the IO pins*/
static volatile unsigned int     key_mask;
/*physical counter states*/
static volatile unsigned int     key_counter_state[17];
/*logical activation states*/
static volatile enum E_KEY_PRESS key_logical_state[17];
/*scan colums or rows*/
static volatile enum E_KEY_SCAN  key_scan_mode;

/*user / comp mode*/
static volatile unsigned int     key_user_mode;
/*for skipping the evaluation*/
static volatile enum E_KEY_PRESS key_any_active;

/*special handling keys*/
/*number of enter key GND in shutdown mode*/
static volatile unsigned int en_key_counter;
/*logical activation state of enter key in shutdown mode*/
static volatile enum E_KEY_PRESS en_key_active;
/*number of backlight key GND*/
static volatile unsigned int backlight_key_counter;
/*logical activation state of backlight key*/
static volatile enum E_KEY_PRESS backlight_key_active;

/*a queue as keyboard buffer*/
static volatile unsigned int QU_Read_Index, QU_Write_Index, QU_Data_Ready;
#define KEYPAD_BUFFER_LENGTH 16U /*must be a power of 2*/
static volatile enum E_KEY   QU_Data[KEYPAD_BUFFER_LENGTH];

/*times in units of 2 ms*/
#define KEY_STATE_BLOCKED_TIME 50U
#define KEY_STATE_MAX           8U
#define KEY_STATE_TO_HIGH       6U
#define KEY_STATE_TO_LOW        2U

/*time in units of 10 ms*/
#define KEY_SHUTDOWN_HIGH       3U

/*
A1  = KEY_1  = row 1, col 1 = 0x11 = dec  17
B2  = KEY_2  = row 1, col 2 = 0x12 = dec  18
C3  = KEY_3  = row 1, col 3 = 0x14 = dec  20
D4  = KEY_4  = row 1, col 4 = 0x18 = dec  24

E5  = KEY_5  = row 2, col 1 = 0x21 = dec  33
F6  = KEY_6  = row 2, col 2 = 0x22 = dec  34
G7  = KEY_7  = row 2, col 3 = 0x24 = dec  36
H8  = KEY_8  = row 2, col 4 = 0x28 = dec  40

MNU = KEY_9  = row 3, col 1 = 0x41 = dec  65
INF = KEY_10 = row 3, col 2 = 0x42 = dec  66
POS = KEY_11 = row 3, col 3 = 0x44 = dec  68
GO  = KEY_12 = row 3, col 4 = 0x48 = dec  72

BCK = KEY_13 = row 4, col 1 = 0x81 = dec 129
FWD = KEY_14 = row 4, col 2 = 0x82 = dec 130
CL  = KEY_15 = row 4, col 3 = 0x84 = dec 132
EN  = KEY_16 = row 4, col 4 = 0x88 = dec 136

in the lookup table index, the row is the high nibble, and the colum is
the low nibble. Hw_Keybd_Get_Input() converts the physical line reading
into this format.

if no key is pressed, KEY_NONE is the result. if more than one key is
pressed, KEY_ERROR results.

more than one key is not possible with this approach because e.g. A1 and
F6 pressed at the same time are indistinguishable from B2 and E5 - that
would require a separate line scan. On the other hand, key combinations
are not used, and rapid key pressing with temporary overlap works fine.*/

static FLASH_ROM const enum E_KEY keypad_table[256] =
{
    /*  0*/ KEY_NONE,  KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*  4*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*  8*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 12*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 16*/ KEY_ERROR, KEY_1,     KEY_2,     KEY_ERROR,
    /* 20*/ KEY_3,     KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 24*/ KEY_4,     KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 28*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 32*/ KEY_ERROR, KEY_5,     KEY_6,     KEY_ERROR,
    /* 36*/ KEY_7,     KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 40*/ KEY_8,     KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 44*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 48*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 52*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 56*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 60*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 64*/ KEY_ERROR, KEY_9,     KEY_10,    KEY_ERROR,
    /* 68*/ KEY_11,    KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 72*/ KEY_12,    KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 76*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 80*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 84*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 88*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 92*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /* 96*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*100*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*104*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*108*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*112*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*116*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*120*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*124*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*128*/ KEY_ERROR, KEY_13,    KEY_14,    KEY_ERROR,
    /*132*/ KEY_15,    KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*136*/ KEY_16,    KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*140*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*144*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*148*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*152*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*156*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*160*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*164*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*168*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*172*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*176*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*180*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*184*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*188*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*192*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*196*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*200*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*204*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*208*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*212*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*216*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*220*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*224*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*228*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*232*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*236*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*240*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*244*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*248*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR,
    /*252*/ KEY_ERROR, KEY_ERROR, KEY_ERROR, KEY_ERROR
};

/*sets either the rows or the colums as output and to ground level.
  the other ones are set as input with pullup. needed for the regular
  user input mode where a matrix scan is applied.*/
static void Hw_Keybd_Set_Output(unsigned int set_mode)
{
    if (set_mode == KEY_SCAN_COLS_OUT)
    {
        /*the keypad cols as output and to GND*/
        KEY_GPIO_MODER = KEY_COLS_MODER_OUT;
        KEY_GPIO_PUPDR = KEY_ROWS_PUPDR;
        Drv_SETRESET_BITS(KEY_GPIO_BSRR, KEY_COL_ALL << 16);
    } else
    {
        /*the keypad rows as output and to GND*/
        KEY_GPIO_MODER = KEY_ROWS_MODER_OUT;
        KEY_GPIO_PUPDR = KEY_COLS_PUPDR;
        Drv_SETRESET_BITS(KEY_GPIO_BSRR, KEY_ROW_ALL << 16);
    }
}

/*reads either the rows or the columns and maps them to
  the high or low nibble of the resulting value.*/
static unsigned int Hw_Keybd_Get_Input(unsigned int read_mode)
{
    uint32_t port_bits = KEY_GPIO_IDR;

    if (read_mode == KEY_SCAN_COLS_IN) /*scan cols*/
    {
        unsigned int res;

        /*most frequent situation: no key pressed, only COL scan active.*/
        if ((port_bits & KEY_COL_ALL) == KEY_COL_ALL)
            return(0);

        res = (!(port_bits & KEY_COL_1)) ? 0x01U : 0;

        if (!(port_bits & KEY_COL_2))
            res |= 0x02U;
        if (!(port_bits & KEY_COL_3))
            res |= 0x04U;
        if (!(port_bits & KEY_COL_4))
            res |= 0x08U;

        return(res);
    } else /*scan rows*/
    {
        unsigned int res;

        /*if this branch is eve executed, the previous COL scan must have
          shown active lines so that also active ROW lines will be there.
          => drop an "any row active" check here.*/

        res = (!(port_bits & KEY_ROW_1)) ? 0x10U : 0;

        if (!(port_bits & KEY_ROW_2))
            res |= 0x20U;
        if (!(port_bits & KEY_ROW_3))
            res |= 0x40U;
        if (!(port_bits & KEY_ROW_4))
            res |= 0x80U;

        return(res);
    }
}

/*checks whether any key is pressed. used during power-on self test
for detecting "stuck" keys". hardware must already be initialised.*/
unsigned int Hw_Keybd_Check_Keys(void)
{
    unsigned int res;
    Hw_Keybd_Set_Output(KEY_SCAN_COLS_OUT);
    Hw_Wait_Micsecs(100UL);
    res = Hw_Keybd_Get_Input(KEY_SCAN_ROWS_IN);
    /*do the ROW output mode last because that will also be the first
      scan mode in the regular application flow.*/
    Hw_Keybd_Set_Output(KEY_SCAN_ROWS_OUT);
    Hw_Wait_Micsecs(100UL);
    res += Hw_Keybd_Get_Input(KEY_SCAN_COLS_IN);

    /*check light button*/
    if (!(LTREQ_GPIO_IDR & LIGHT_REQ)) /*GND is active*/
        res++;

    return(res);
}

/*prepares they keyboard for the "enter only" mode. needed when the system
  has crashed and the user shall confirm the final dialogue box. this cannot
  be done with regular input because that is in the Sysfault exception so
  that the Systick does not work.*/
static void Hw_Keybd_Set_En_Keymode(void)
{
    /*set ROW 4 to GND, set row 1,2,3 as inputs.
    set col 1, 2, 3 as inputs.
    set col 4 as input with pull-up.*/
    KEY_GPIO_MODER =  KEY_EN_MODER_OUT;
    KEY_GPIO_PUPDR =  KEY_EN_PUPDR_OUT;
    Drv_SETRESET_BITS(KEY_GPIO_BSRR, KEY_ROW_4 << 16);
    /*enforce key going inactive first*/
    en_key_counter   = KEY_SHUTDOWN_HIGH;
    en_key_active = KEY_ACTIVE;
}

/*this does not have a real handler because it is only called for
  system shutdown when the Systick is not active. then the user
  has to press EN for confirmation.*/
unsigned int Hw_Keybd_Get_Enkey(void)
{
    /*read COL 4 state. 0 is ground (pressed). then debounce.*/
    if (!(KEY_GPIO_IDR & KEY_COL_4)) /*pressed*/
    {
        if (en_key_counter < KEY_SHUTDOWN_HIGH)
            en_key_counter++;
    } else /*not pressed*/
    {
        if (en_key_counter > 0)
            en_key_counter--;
    }

    /*pressing is a one time transition because the system will be
      rebooted, so always return "is pressed" after the first detection.
      just a bit more robust in case the event is missed somehow.*/
    if (en_key_active == KEY_INACTIVE)
    {
        /*changed to active?*/
        if (en_key_counter >= KEY_SHUTDOWN_HIGH)
            return(KEY_ACTIVE);
    } else if (en_key_counter == 0) /*initial debouncing*/
        en_key_active = KEY_INACTIVE;

    return(0);
}

/*clear the application inout queue*/
static void Kw_Keybd_QU_Clear(void)
{
    unsigned int i;

    QU_Read_Index  = 0;
    QU_Write_Index = 0;
    QU_Data_Ready  = 0;
    for (i = 0; i < KEYPAD_BUFFER_LENGTH; i++)
        QU_Data[i] = KEY_NONE;
}

/*init the keypad buffer and queue.*/
static void Hw_Keybd_Init(void)
{
    unsigned int i;

    Hw_Keybd_Set_Output(KEY_SCAN_ROWS_OUT);
    key_scan_mode  = KEY_SCAN_COLS;
    key_mask       = 0;
    key_user_mode  = 1U;
    key_any_active = KEY_INACTIVE;

    Kw_Keybd_QU_Clear();

    for (i = 0; i < 17U; i++)
    {
        key_counter_state[i] = 0;
        key_logical_state[i] = KEY_INACTIVE;
    }

    backlight_key_counter = 0;
    backlight_key_active  = KEY_INACTIVE;
}

/*read the keyboard queue. this is called from the application via Hw_Getch().*/
enum E_KEY Hw_Keybd_QU_Read(void)
{
    if (QU_Data_Ready) /*systick ISR put in a character*/
    {
        enum E_KEY result;
        unsigned int tmp_read_index, tmp_write_index;
        /*give some more randomness to the current randomness state. Since
        the user key actions are asynchronous to the system time, this provides
        additional entropy.*/
        Hw_Add_Randomness();

        /*prevent systick from interfering with write*/
        Drv_Pause_Systick();

        tmp_write_index = QU_Write_Index;
        tmp_read_index = QU_Read_Index;
        if (tmp_read_index == tmp_write_index)
        {
            /*queue empty despite QU_Data_Ready, should not happen*/
            QU_Data_Ready = 0;
            Drv_Cont_Systick();
            return(KEY_NONE);
        }
        result = QU_Data[tmp_read_index++];
        tmp_read_index &= (KEYPAD_BUFFER_LENGTH - 1); /*wrap around*/
        /*no more characters in the buffer?*/
        if (tmp_read_index == tmp_write_index)
            QU_Data_Ready = 0;
        /*write back updated read index*/
        QU_Read_Index = tmp_read_index;
        Drv_Cont_Systick();
        return(result);
    } else /*no key presed*/
        return(KEY_NONE);
}

/*write into the keyboard queue. this is called from the interrupt.*/
static void Hw_Keybd_QU_Write(enum E_KEY pressed_key)
{
    if (key_user_mode) /*waiting for user input*/
    {
        unsigned int tmp_read_index, tmp_write_index, updated_write_index;

        tmp_read_index = QU_Read_Index;
        tmp_write_index = QU_Write_Index;

        /*is there still space in the queue?*/
        updated_write_index = tmp_write_index;
        updated_write_index++;
        updated_write_index &= (KEYPAD_BUFFER_LENGTH - 1); /*wrap around*/
        /*write index == read index enters the empty condition again*/
        if (updated_write_index == tmp_read_index)
            return; /*just discard. should not happen, the queue is big enough.*/

        /*do the keyboard click (if configured)*/
        Hw_Sig_Send_Msg(HW_MSG_BEEP_ON, BEEP_CLICK, HW_MSG_PARAM_CLICK);
        /*handle the backlight as configured*/
        Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_KEY, HW_MSG_PARAM_BACK_CONF);
        QU_Data[tmp_write_index] = pressed_key;
        QU_Write_Index = updated_write_index;
        QU_Data_Ready = 1U;
    } else /*computer is calculating a move*/
    {
        /*in comp mode, don't use the keyboard queue and react directly to
          pressed keys so that the application doesn't have to poll the queue.*/
        if (pressed_key == KEY_GO)
        {
            /*do the keyboard click (if configured)*/
            Hw_Sig_Send_Msg(HW_MSG_BEEP_ON, BEEP_CLICK, HW_MSG_PARAM_CLICK);
            /*handle the backlight as configured*/
            Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_KEY, HW_MSG_PARAM_BACK_CONF);

            /*it looks like a dirty hack to abuse the "time over" mechanism
              for a user triggered keyboard event, but that avoids a second,
              independent abort mechanism.*/
            Time_Enforce_Comp_Move();
        } else if (pressed_key == KEY_INFO) /*toggle search info display*/
        {
            /*do the keyboard click (if configured)*/
            Hw_Sig_Send_Msg(HW_MSG_BEEP_ON, BEEP_CLICK, HW_MSG_PARAM_CLICK);
            /*handle the backlight as configured*/
            Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_KEY, HW_MSG_PARAM_BACK_CONF);
            /*the actual change goes via the time update*/
            Time_Enforce_Disp_Toggle(DISP_TOGGLE);
        }
    }
}

/*handle the button for the light request, including debouncing.*/
static void NEVER_INLINE Hw_Keybd_Backlight_Handler(void)
{
    uint32_t key_counter = backlight_key_counter;

    /*read the physical state of the light button, active is GND*/
    if (LTREQ_GPIO_IDR & LIGHT_REQ) /*key not pressed*/
    {
        /*key not pressed and not counting down -> nothing to do*/
        if (key_counter == 0)
            return;

        key_counter--;
        /*key released?*/
        if ((backlight_key_active == KEY_ACTIVE) && (key_counter <= KEY_STATE_TO_LOW))
            backlight_key_active = KEY_INACTIVE;
    } else /*key pressed*/
    {
        /*keypress has already been processed, nothing to do*/
        if (key_counter == KEY_STATE_MAX)
            return;

        key_counter++;
        /*key hitting?*/
        if ((backlight_key_active == KEY_INACTIVE) && (key_counter >= KEY_STATE_TO_HIGH))
        {
            /*enforce certain OFF time against user double press*/
            key_counter = KEY_STATE_BLOCKED_TIME;
            backlight_key_active = KEY_ACTIVE;
            Hw_Sig_Send_Msg(HW_MSG_BEEP_ON, BEEP_CLICK, HW_MSG_PARAM_CLICK);
            /*in battery shutdown mode, the HMI layer sends the INHIBIT so that
              the display backlight will not be switched on here.*/
            Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_REQ, HW_MSG_PARAM_BACK_FORCE);
        }
    }
    backlight_key_counter = key_counter;
}

/*the user keyboard handler converts the IO pin states into a key mask, then
performs the debouncing, then writes new pressed keys into the keyboard buffer to
the application.*/
static void NEVER_INLINE Hw_Keybd_Matrix_Handler(void)
{
    if (key_scan_mode == KEY_SCAN_COLS)
    {
        /*measure column state*/
        key_mask = Hw_Keybd_Get_Input(KEY_SCAN_COLS_IN);

        /*set colums as output only if something is there. this reduces power
          consumption and EMI.*/
        if (key_mask)
            Hw_Keybd_Set_Output(KEY_SCAN_COLS_OUT);

        key_scan_mode = KEY_SCAN_ROWS;
    } else /*ROW scan*/
    {
        enum E_KEY pressed_key = KEY_NONE;

        /*measure ROW state. if there has been no COL scan result as active,
          there cannot be a ROW scan result anyway.*/
        if (key_mask)
        {
            key_mask |= Hw_Keybd_Get_Input(KEY_SCAN_ROWS_IN);
            pressed_key = keypad_table[key_mask];

            /*set rows as output*/
            Hw_Keybd_Set_Output(KEY_SCAN_ROWS_OUT);
        }

        if ((pressed_key != KEY_ERROR) &&
            ((pressed_key != KEY_NONE) || (key_any_active != KEY_INACTIVE)))
        /*if several buttons are pressed at the same time, just wait.
          if no key is pressed and no counting activity is going on,
          skip the loop.*/
        {
            enum E_KEY_PRESS counting_key = KEY_INACTIVE;
            enum E_KEY i;
            /*important not to start with 0 as that is KEY_NONE*/
            for (i = KEY_1; i <= KEY_16; i++)
            {
                unsigned int counter_state;
                enum E_KEY_PRESS logical_state;

                /*local copy*/
                logical_state = key_logical_state[i];
                counter_state = key_counter_state[i];
                /*for the keys that are not pressed*/
                if (i != pressed_key)
                {
                    /*still in the release debouncing?*/
                    if (counter_state > 0)
                    {
                        counting_key = KEY_ACTIVE;
                        counter_state--;
                        key_counter_state[i] = counter_state;
                        /*still marked as being active?*/
                        if (logical_state != KEY_INACTIVE)
                            /*key released?*/
                            if (counter_state <= KEY_STATE_TO_LOW)
                                key_logical_state[i] = KEY_INACTIVE;
                    }
                } else /*for the pressed key*/
                {
                    counting_key = KEY_ACTIVE;
                    if (counter_state < KEY_STATE_MAX)
                    {
                        counter_state++;
                        /*not yet marked as pressed?*/
                        if (logical_state == KEY_INACTIVE)
                        {
                            /*key debounced and hitting?*/
                            if (counter_state >= KEY_STATE_TO_HIGH)
                            {
                                /*enforce a certain OFF time against user double press*/
                                counter_state = KEY_STATE_BLOCKED_TIME;
                                key_logical_state[i] = KEY_ACTIVE;
                                /*hand it over to the application*/
                                Hw_Keybd_QU_Write(pressed_key);
                            }
                        }
                        /*write back*/
                        key_counter_state[i] = counter_state;
                    }
                }
            }
            key_any_active = counting_key;
        }
        key_scan_mode = KEY_SCAN_COLS;
    }
}

/*keyboard handler to be called from the systick interrupt*/
void Hw_Keybd_Handler(void)
{
    /*put the backlight handler in slots where the "long" run of the matrix
      key handler will not be executed.*/
    if (key_scan_mode == KEY_SCAN_COLS)
        Hw_Keybd_Backlight_Handler();

    Hw_Keybd_Matrix_Handler();
}

/*sets the keyboard handling mode.*/
void Hw_Keybd_Set_Mode(enum HW_KEYBD_MODE set_mode)
{
    switch (set_mode)
    {
    /*initialisation*/
    case HW_KEYBD_MODE_INIT:
        Hw_Keybd_Init();
        break;
    /*regular user input*/
    case HW_KEYBD_MODE_USER:
        Kw_Keybd_QU_Clear();
        key_user_mode = 1U;
        break;
    /*computer is thinking, only GO and light keys*/
    case HW_KEYBD_MODE_COMP:
        key_user_mode = 0;
        break;
    /*system has crashed, only ENter*/
    case HW_KEYBD_MODE_ENT:
        Hw_Keybd_Set_En_Keymode();
        break;
    default:
        break;
    }
}
