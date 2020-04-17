/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2015-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of the CT800 (hardware interface / PC).
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

/*for interfacing the hardware. this file is for the PC version.
many functions are more or less just stubs to avoid modifying the rest of the c files.*/

#include <stdint.h>
#include <stdio.h>
#include <sys/timeb.h>
#include <stdlib.h>
#include <termios.h>
#include "ctdefs.h"
#include "confdefs.h"
#include "util.h"
#include "timekeeping.h"
#include "hmi.h"
#include "menu.h"
#include "hardware.h"

#ifndef PC_VERSION
#error "in ctdefs.h, you must define PC_VERSION for the PC build. Or use -DPC_VERSION for the compiler."
#endif

/*--------- no external variables ---------*/

void Play_Save_Status(BACKUP_GAME *ptr);
void Play_Load_Status(const BACKUP_GAME *ptr);

#define BAT_VOLTAGE_BUF_LEN_BITS 6U
static volatile uint32_t battery_voltage[(1UL << BAT_VOLTAGE_BUF_LEN_BITS)];
static volatile uint32_t battery_voltage_cnt;
static volatile uint32_t battery_voltage_avg;
volatile uint32_t battery_status; /*might get changed in the timer interrupt*/
int battery_confirmation;
int battery_low_confirmed;
int battery_high_confirmed;
static int user_interaction_passed;

uint64_t hw_config;
static enum E_AUTOSAVE autosave_state;
static int backup_ram_ok;

#define BKPSRAM_SIZE 4096UL
static uint32_t backup_ram[(BKPSRAM_SIZE / sizeof(uint32_t))];

/*the battery buffered backup ram domain is 4k in size. So if the backup
game structure gets larger than 4k, this will not work.*/
BUILD_ASSERT((sizeof(SYS_BACKUP) <= BKPSRAM_SIZE),_build_assert_no_0,"Game save structure too large!");

static void *backup_ram_ptr = backup_ram;
static enum E_SYS_SPEED hw_system_speed = SYSTEM_SPEED_LOW;
static enum E_CLK_FORCE hw_speed_force = CLK_ALLOW_LOW;

/*debug*/
static uint32_t cfg_reg_0 = 0;
static uint32_t cfg_reg_1 = 0;
static uint32_t cfg_reg_2 = 0;

static volatile int32_t sys_time; /*is changed by the timer interrupt*/

static volatile int go_key_seen;
static volatile int go_key_active;

static volatile int hw_hmi_signal_state;

static volatile int32_t led_1_runtime;
static volatile int32_t led_2_runtime;
static volatile int32_t beep_runtime;
static volatile int32_t backlight_runtime;

static uint32_t hw_randomness_state = 0;

/*callback function to the HMI layer*/
static void (*Hw_Bat_Mon_Callback)(enum E_WHOSE_TURN side_to_move);

static struct termios old_term, new_term;
static time_t sys_startup_time_seconds;

/* initialise new terminal i/o settings*/
static void initTermios(int echo)
{
    tcgetattr(0, &old_term); /* grab old terminal i/o settings */
    new_term = old_term; /* make new settings same as old settings */
    new_term.c_lflag &= ~ICANON; /* disable buffered i/o */
    new_term.c_lflag &= echo ? ECHO : ~ECHO; /* set echo mode */
    tcsetattr(0, TCSANOW, &new_term); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
static void resetTermios(void)
{
    tcsetattr(0, TCSANOW, &old_term);
}

/* Read 1 character - echo defines echo mode */
static char getch_(int echo)
{
    char ch;
    initTermios(echo);
    ch = getchar();
    resetTermios();
    return ch;
}
/* Read 1 character with echo */
/*
static char getche(void)
{
return getch_(1);
}*/

/*gets the system startup time in seconds, for x86*/
static void Hw_Get_System_Start_Time(void)
{
    struct timeb timebuffer;
    ftime(&timebuffer);
    sys_startup_time_seconds = timebuffer.time;
}

/*checks whether autosave can be used or whether there is a manually saved game.*/
static enum E_AUTOSAVE Hw_Get_Autosave_State(int *bckp_ram_ok)
{
    uint32_t crc_32;
    BACKUP_GAME *backup_ptr;

    *bckp_ram_ok = 0;

    backup_ptr = &(((SYS_BACKUP *)backup_ram_ptr)->backup_game);

    crc_32 = Util_Crc32((void *) backup_ptr, sizeof (BACKUP_GAME));

    if (crc_32 == ((SYS_BACKUP *) backup_ram_ptr)->crc_32) /*CRC check passed*/
    {
        *bckp_ram_ok = 1;
        if (backup_ptr->autosave == (uint8_t) HW_AUTOSAVE_OFF)
            return (HW_AUTOSAVE_OFF);
    }

    /*if the CRC check failed or autosave has been TRUE anyway, the backup area is free for autosave.*/
    return(HW_AUTOSAVE_ON);
}


/* Read 1 character without echo.
must be non-blocking because of the watchdog triggering.*/
void Hw_Trigger_Watchdog(void);
enum E_KEY Hw_Getch(VAR_UNUSED enum E_WAIT_SLEEP sleep_mode)
{
    enum E_KEY pressed_key;
    char c;

    Hw_Trigger_Watchdog();

    /*the battery shutdown monitoring in the HMI, see Hmi_Battery_Shutdown()*/
    if (Hw_Bat_Mon_Callback != NULL)
    {
        if (battery_status & BATTERY_SHUTDOWN) /*battery very low?*/
            (*Hw_Bat_Mon_Callback)(USER_TURN); /*shut down system*/
    }

    c =  getch_(0);

    if ((c >='A') && (c<='Z'))
        c = c -'A'+'a';
    if ((c >='1') && (c<='8'))
        c = c -'1'+'a';

    switch (c)
    {
    case 'a':
        pressed_key = KEY_A1;
        break;
    case 'b':
        pressed_key = KEY_B2;
        break;
    case 'c':
        pressed_key = KEY_C3;
        break;
    case 'd':
        pressed_key = KEY_D4;
        break;
    case 'e':
        pressed_key = KEY_E5;
        break;
    case 'f':
        pressed_key = KEY_F6;
        break;
    case 'g':
        pressed_key = KEY_G7;
        break;
    case 'h':
        pressed_key = KEY_H8;
        break;
    case 'i':
        pressed_key = KEY_INFO;
        break;
    case 'm':
        pressed_key = KEY_MENU;
        break;
    case 'n':
        pressed_key = KEY_CL;
        break;
    case 'p':
        pressed_key = KEY_POS_DISP;
        break;
    case 'u':
    case '-':
        pressed_key = KEY_UNDO;
        break;
    case 'r':
    case '+':
        pressed_key = KEY_REDO;
        break;
    case 'x':
        pressed_key = KEY_GO;
        break;
    case 'y':
        pressed_key = KEY_ENT;
        break;
    case 'q':
        exit(0);
        break;
    /*virtual key 0*/
    case 'v':
        pressed_key = KEY_V_FCL;
        break;
    default:
        pressed_key = KEY_NONE;
        break;
    }

    return(pressed_key);
}

/*sets the system speed either to low, for saving energy, or to high
for maximum computing power. the overclocking configuration option is
taken care of.
the system mode refers mainly to the keyboard handling:
in user mode, all keys are read and debounced more thoroughly.
in computer mode, only the go-key is scanned so that the user can force
the computer to move. all other keys are ignored.*/
void Hw_Set_Speed(enum E_SYS_SPEED speed, enum E_SYS_MODE mode, enum E_CLK_FORCE force_high)
{
    if (speed == SYSTEM_SPEED_HIGH)
    {
        uint64_t cfg_option = CFG_GET_OPT(CFG_CLOCK_MODE);

        if (force_high == CLK_FORCE_AUTO)
            force_high = hw_speed_force;
        else
            hw_speed_force = force_high;

        if ((cfg_option > CFG_CLOCK_100) &&
            (battery_status & BATTERY_HIGH) &&
            (user_interaction_passed))
            /*don't kick in overclocking when the batteries are already low anyway, or before
            the user has had the opportunity to disable the overclocking (prevent bricking of the device)*/
        {

        } else
        {

        }
        hw_system_speed = SYSTEM_SPEED_HIGH;
    } else /*low speed mode*/
    {
        hw_system_speed = SYSTEM_SPEED_LOW;
    }
}

/*checks whether underclocking is active, and if so, reduces the speed.*/
void Hw_Throttle_Speed(void)
{
    hw_speed_force = CLK_ALLOW_LOW;
}

/*this backup RAM test writes something different to each cell for
being able to detect also address line problems.*/
static unsigned int Hw_Test_Backup_Ram(void)
{
    unsigned int i, ram_test_ok = 0;
    volatile uint32_t *backup_ram_area;
    uint32_t write_value;
    uint32_t read_value;
    const uint32_t test_start_value = 0xF0E32A6UL;

    backup_ram_area = (volatile uint32_t *) backup_ram_ptr;
    write_value = test_start_value;
    for (i = 0; (i < BKPSRAM_SIZE/sizeof(uint32_t) ); i++)
    {
        backup_ram_area[i] = write_value;
        /*rotate right by 19 bits*/
        write_value = (write_value >> 19) | (write_value << (32-19));
    }

    write_value = test_start_value;
    for (i = 0; (i < BKPSRAM_SIZE/sizeof(uint32_t) ); i++)
    {
        /*double read - dirty RAM fault test*/
                     backup_ram_area[i];
        read_value = backup_ram_area[i];
        COMPILER_BARRIER;
        if (read_value != write_value)
            ram_test_ok++;
        write_value = (write_value >> 19) | (write_value << (32-19));
    }

    write_value = ~test_start_value;
    for (i = 0; (i < BKPSRAM_SIZE/sizeof(uint32_t) ); i++)
    {
        backup_ram_area[i] = write_value;
        write_value = (write_value >> 19) | (write_value << (32-19));
    }

    write_value = ~test_start_value;
    for (i = 0; (i < BKPSRAM_SIZE/sizeof(uint32_t) ); i++)
    {
        /*double read - dirty RAM fault test*/
                     backup_ram_area[i];
        read_value = backup_ram_area[i];
        COMPILER_BARRIER;
        backup_ram_area[i] = 0;
        if (read_value != write_value)
            ram_test_ok++;
        write_value = (write_value >> 19) | (write_value << (32-19));
    }

    autosave_state = HW_AUTOSAVE_ON;

    return(!(ram_test_ok));
}

void Hw_Setup_System(void)
{
    unsigned int i;

    Hw_Bat_Mon_Callback = NULL;

    battery_voltage_cnt = 0;
    /*avoid an initial false "battery low" warning*/
    for (i = 0; i < (1U << BAT_VOLTAGE_BUF_LEN_BITS); i++)
        battery_voltage[i] = BATTERY_NORM_LEVEL_20K;
    battery_voltage_avg = (BATTERY_NORM_LEVEL_20K << BAT_VOLTAGE_BUF_LEN_BITS);

    sys_time = 0;
    hw_hmi_signal_state = 0;
    led_1_runtime = 0;
    led_2_runtime = 0;
    beep_runtime = 0;
    backlight_runtime = 0;
    battery_status = BATTERY_HIGH;
    battery_confirmation = BATTERY_CONF_HIGH;
    user_interaction_passed = 0;

    /*ramp up the speed for getting the init stuff done faster.*/
    Hw_Set_Speed(SYSTEM_SPEED_HIGH, SYSTEM_MODE_USER, CLK_FORCE_HIGH);

    Hw_Get_System_Start_Time();

    Hw_Load_Config(&hw_config);

    Hw_Disp_Set_Conf_Contrast();

    autosave_state = Hw_Get_Autosave_State(&backup_ram_ok);
    /*if there is a valid config, then backup_ram_ok is now 1, otherwise 0.*/
    if (backup_ram_ok == 0) /*no valid config - do a dedicated RAM check.*/
        backup_ram_ok = Hw_Test_Backup_Ram();
}

/*set up the battery shutdown callback handler to the HMI layer*/
void Hw_Set_Bat_Mon_Callback(void (*Bat_Mon_Callback)(enum E_WHOSE_TURN))
{
    Hw_Bat_Mon_Callback = Bat_Mon_Callback;
}

void Hw_Powerdown_System(void)
{

}

void Hw_Sleep(void)
{

}

/*gets the system time in milliseconds*/
int32_t Hw_Get_System_Time(void)
{
    struct timeb timebuffer;
    ftime(&timebuffer);
    timebuffer.time -= sys_startup_time_seconds;
    return (timebuffer.time * 1000) + timebuffer.millitm;
}

/*sets the system time in milliseconds*/
void Hw_Set_System_Time(int32_t new_time)
{
    sys_time = new_time;
}

/*returns whether the voltage is OK for starting a new game.*/
int Hw_Battery_Newgame_Ok(void)
{
    return(1);
}

void Hw_Disp_Set_Cursor(int line, int col, int active)
{

}

void Hw_Disp_Show_All(const char *viewport, enum E_HW_DISP mode)
{
    int i;

    if (mode == HW_DISP_RAW)
        printf("\r\n********RAW*********\r\n");
    else
        printf("\r\n******DIALOGUE******\r\n");

    for (i = 0; i < 20; i++)
    {
        if ((viewport[i] >= 32) && (viewport[i] < 127))
            printf("%c", viewport[i]);
        else printf("Z");
    }
    printf("\r\n");
    for (i = 20; i < 40; i++)
    {
        if ((viewport[i] >= 32) && (viewport[i] < 127))
            printf("%c", viewport[i]);
        else printf("Z");
    }
    printf("\r\n");
    for (i = 40; i < 60; i++)
    {
        if ((viewport[i] >= 32) && (viewport[i] < 127))
            printf("%c", viewport[i]);
        else printf("Z");
    }
    printf("\r\n");
    for (i = 60; i < 80; i++)
    {
        if ((viewport[i] >= 32) && (viewport[i] < 127))
            printf("%c", viewport[i]);
        else printf("Z");
    }
    printf("\r\n********************\r\n");

}

/*works only for updates within the same text line!*/
void Hw_Disp_Update(const char *viewport, int pos, int len)
{
    int i;
    int line, col;

    line = pos / 20;
    col = pos % 20;
    Hw_Disp_Set_Cursor(line, col, DISP_CURSOR_OFF);

    for (i=0; i<len; i++)
    {
        /*print out viewport[pos+i] */
    }

    /*debug*/
    printf("\r\n*DIFF UPDATE******** %d:%d\r\n", pos, len);
    for (i = 0; i < 20; i++)
    {
        if ((viewport[i] >= 32) && (viewport[i] < 127))
            printf("%c", viewport[i]);
        else printf("Z");
    }
    printf("\r\n");
    for (i = 20; i < 40; i++)
    {
        if ((viewport[i] >= 32) && (viewport[i] < 127))
            printf("%c", viewport[i]);
        else printf("Z");
    }
    printf("\r\n");
    for (i = 40; i < 60; i++)
    {
        if ((viewport[i] >= 32) && (viewport[i] < 127))
            printf("%c", viewport[i]);
        else printf("Z");
    }
    printf("\r\n");
    for (i = 60; i < 80; i++)
    {
        if ((viewport[i] >= 32) && (viewport[i] < 127))
            printf("%c", viewport[i]);
        else printf("Z");
    }
    printf("\r\n*DIFF UPDATE********\r\n");
}

void Hw_Disp_Set_Charset(enum E_HW_CHARSET charset)
{

}

/*the watchdog will be triggered from two important main points:
a) Hw_Getch(). This should always be the case when the user has to
enter something, i.e. when it is his move.
b) From Time_Check(), which controls whether the computer's thinking
time is up.
Note: The "go!" button forces a computer move when the computer is
thinking, but this is not done via Hw_Getch(). This mechanism is
triggered one level below, from the timer interrupt where the keypad
processing takes place. This basically tells the computer "time is up,
move now".
So during computer thinking, Hw_Getch() isn't called, that's why we need
a second trigger point for the watchdog. Both points are part of the regular
control flow, and a possible endless loop would make the watchdog bite.*/
void Hw_Trigger_Watchdog(void)
{

}

/*wait for the watchdog do bite*/
void Hw_Sys_Reset(void)
{
    for (;;) ;
}

/*save the configuration to one of the backup registers.
the CRC32 of the 4 config bytes goes into another register.*/
void Hw_Save_Config(const uint64_t *config)
{
    uint32_t crc_32;

    crc_32 = Util_Crc32(((void *) config), sizeof(uint64_t));
    cfg_reg_0 = (uint32_t)((*config) >> 32U);
    cfg_reg_1 = (uint32_t)((*config) & 0xFFFFFFFFULL);
    cfg_reg_2 = crc_32;
}

void Hw_Set_Default_Config(uint64_t *config)
{
    *config = CFG_DEFAULT;
    Hw_Save_Config(config);
}

/*load the config from its backup register, load the stored
config CRC32 from another backup register. if the CRC32 matches
the config data, it's a valid config; otherwise, there is some
nonsense stored, e.g. after the supercap has been exhausted.
in this case, use the default configuration.*/
void Hw_Load_Config(uint64_t *config)
{
    uint32_t cfg_0, cfg_1, cfg_2, crc_32, conf_version;
    uint64_t storedconfig;

    cfg_0 = cfg_reg_0;
    cfg_1 = cfg_reg_1;
    cfg_2 = cfg_reg_2;

    storedconfig = cfg_0;
    storedconfig <<= 32;
    storedconfig |= cfg_1;

    crc_32 = Util_Crc32(((void *) &storedconfig), sizeof(uint64_t));
    conf_version = (uint32_t) (storedconfig >> CONF_VERSION_OFFSET);

    if ((cfg_2 != crc_32) || (conf_version != CONF_VERSION))
    {
        (void) Hw_Erase_Game();
        Hw_Set_Default_Config(config);
    }
    else
        *config = storedconfig;
}

/*loads the game including a CRC check.*/
enum E_FILEOP Hw_Load_Game(void)
{
    uint32_t crc_32;
    BACKUP_GAME *backup_ptr;

    backup_ptr = &(((SYS_BACKUP *)backup_ram_ptr)->backup_game);
    crc_32 = Util_Crc32((void *) backup_ptr, sizeof (BACKUP_GAME));

    if (crc_32 == ((SYS_BACKUP *) backup_ram_ptr)->crc_32) /*CRC check passed*/
    {
        int32_t system_time;

        /*setting system time doesn't work on PC*/
        system_time = Hw_Get_System_Time();

        hw_config = backup_ptr->hw_config;
        hw_randomness_state = backup_ptr->randomness_state;

        Time_Load_Status(backup_ptr, system_time); /*get the time management variables*/
        Hmi_Load_Status(backup_ptr);               /*get the pretty print*/
        Menu_Load_Status(backup_ptr,system_time);  /*get the menu start time*/
        Play_Load_Status(backup_ptr);              /*get the game-related stuff*/

        Hw_Disp_Set_Conf_Contrast();

        /*for the adjusted time stamps, doesn't work on PC*/
        Hw_Set_System_Time(0);

        autosave_state = (enum E_AUTOSAVE) backup_ptr->autosave;

        if (autosave_state == HW_AUTOSAVE_ON)
            return (HW_FILEOP_OK_AUTO);
        else
            return (HW_FILEOP_OK_MAN);
    }
    else
        return(HW_FILEOP_FAILED);
}

/*saves the game in the backup RAM. The last 4 bytes
serve as CRC 32 over the saved game structure.

autosave: this routine can either be called by the user, i.e. over the menu,
ich which case it is a manual save (request_autosave is 0). Besides, it is called
automatically after every move (request_autosave is 1).

if there is no manually saved game that needs to be preserved, the autosave will
use the backup memory. If the power fails, the last saved game status can be
loaded after the next power-up.

if there is a manually saved game, this must not be overwritten by the
autosave. if the user wants to re-enable autosave, he has to use the menu
function for erasing the backup storage.

Note that the backup storage is RAM, not flash ROM, so frequent write
cycles will not cause hardware damage in the long run.

Note 2: this function takes 4k on the stack, see below.*/
enum E_FILEOP NEVER_INLINE Hw_Save_Game(enum E_SAVE_TYPE request_autosave)
{
    /*this is declared as a stack-local variable. It takes nearly 4k on the
    stack, but this function is only being called from points outside of the
    recursive search.
    It is better to first stuff everything into a local variable and only then
    copy things to the actual backup ram. The copy loop is optimised, so it
    is faster. That means that the critical time where a power loss would
    destroy the stored backup game is also shorter. That's also why interrupts
    are switched off on ARM.*/

    SYS_BACKUP sys_backup; /*4k stack!*/
    int sys_speed_changed;
    int32_t system_time;

    system_time = Hw_Get_System_Time();

    /*autosave handling: if we are in autosave mode and a manual save comes in,
    disable further autosaving (can be re-enabled using Hw_Erase() ).*/
    if ((autosave_state == HW_AUTOSAVE_ON) && (request_autosave == HW_MANUAL_SAVE))
        autosave_state = HW_AUTOSAVE_OFF;

    /*if autosave is disabled and an autosave comes in, don't execute that.*/
    if ((autosave_state == HW_AUTOSAVE_OFF) && (request_autosave == HW_AUTO_SAVE))
        return(HW_FILEOP_FAILED); /*the caller will ignore the autosave result anyway*/

    /*get the current system speed*/
    if (hw_system_speed == SYSTEM_SPEED_LOW)
    {
        /*saving shall be as fast as possible, so do it at high speed.*/
        Hw_Set_Speed(SYSTEM_SPEED_HIGH, SYSTEM_MODE_KEEP, CLK_FORCE_HIGH);
        sys_speed_changed = 1;
    } else
        sys_speed_changed = 0;

    sys_backup.backup_game.blackstart_searchdisp = 0; /*to be set bitwise*/
    sys_backup.backup_game.hw_config = hw_config;
    sys_backup.backup_game.randomness_state = hw_randomness_state;
    sys_backup.backup_game.autosave = (uint8_t) autosave_state;

    /*save the time management variables*/
    Time_Save_Status(&(sys_backup.backup_game), system_time, request_autosave);
    /*save the pretty print*/
    Hmi_Save_Status (&(sys_backup.backup_game));
    /*save the menu start time*/
    Menu_Save_Status(&(sys_backup.backup_game), system_time);
    /*save the game-related stuff*/
    Play_Save_Status(&(sys_backup.backup_game));

    sys_backup.crc_32 = Util_Crc32((void *) &(sys_backup.backup_game), sizeof (BACKUP_GAME));

    Util_Memcpy(backup_ram_ptr, (void *) &sys_backup, sizeof (SYS_BACKUP));

    /*if the speed has been changed from low to high, change it back.*/
    if (sys_speed_changed)
        Hw_Set_Speed(SYSTEM_SPEED_LOW, SYSTEM_MODE_KEEP, CLK_FORCE_AUTO);

    if (autosave_state == HW_AUTOSAVE_ON)
        return(HW_FILEOP_OK_AUTO);
    else
        return(HW_FILEOP_OK_MAN);
}

/*clears the backup RAM.*/
enum E_FILEOP Hw_Erase_Game(void)
{
    Util_Memzero((void *) backup_ram_ptr, BKPSRAM_SIZE);
    autosave_state = HW_AUTOSAVE_ON; /*re-enable autosave*/
    return(HW_FILEOP_OK_AUTO);
}

/*sets the display contrast.
note that high percent values correspond to a low DAC output voltage as the
contrast grows when the voltage difference between DAC output voltage and
display power supply grows.
0% corresponds to 1.5V output. Since 4095dec is 3.3V output, 1861dec is 1.5V.
100% corresponds to an output 0f 0V.*/
void Hw_Disp_Set_Contrast(unsigned int contrast_percentage)
{
    unsigned int contrast_output;
    if (contrast_percentage >= 100U)
        contrast_output = 0;
    else
    {
        contrast_output = (100UL - contrast_percentage) * 1861UL;
        contrast_output /= 100UL;
    }

    /*now the binary DAC 12 bit value is in contrast_output.
    not used in the PC version, of course.*/
    (void) contrast_output;
}

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


void Hw_Seed(void)
{
    hw_randomness_state = Hw_Get_System_Time();

    hw_randomness_state %= 32051UL;

    hw_randomness_state <<= 8u;
    hw_randomness_state += 71UL;
}

/*returns an integer between 0 and 65535.*/
uint32_t Hw_Rand(void)
{
    hw_randomness_state *= 1103515245UL;
    hw_randomness_state += 12345UL;
    return(hw_randomness_state >> 16);
}

unsigned int Hw_Check_FW_Image(void)
{
    return(HW_SYSTEM_OK);
}

/*on ARM: check the image, RAM and external quarz*/
unsigned int Hw_Check_RAM_ROM_XTAL_Keys(void)
{
    if (backup_ram_ok)
        return(HW_SYSTEM_OK);
    else
        return(HW_RAM_FAIL);
}

/*map the reset register bits to logical system reset causes*/
unsigned int Hw_Get_Reset_Cause(void)
{
    uint32_t result;
    result = 0;
    /*for testing the dialogue system on the PC:*/
    //result |= HW_SYSRESET_POWER;
    //result |= HW_SYSRESET_WDG;
    //result |= HW_SYSRESET_PIN;
    //result |= HW_SYSRESET_SW;
    return(result);
}

/*clear the reset status register - not used on the PC.*/
void Hw_Clear_Reset_Cause(void)
{
    ;
}

/*tell the autosave state for a potential warning on startup
if the autosave is disabled. it would be annoying to rely on
autosave, have a wall power outage and only then discover that
the game has not been saved!*/
enum E_AUTOSAVE Hw_Tell_Autosave_State(void)
{
    return(autosave_state);
}

/*now we may overclock on ARM.*/
void Hw_User_Interaction_Passed(void)
{
    user_interaction_passed = 1;
}

void Hw_Sig_Send_Msg(enum E_HW_MSG message, uint32_t duration, enum E_HW_MSG_PARAM param)
{

}
