/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2015-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of the CT800 (hardware interface / ARM).
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
#include "util.h"
#include "timekeeping.h"
#include "hmi.h"
#include "menu.h"
#include "hardware.h"
#include "arm_driver.h"

/*semi-public functions from other hardware modules*/

/*display*/
void         Hw_Disp_Init(void);
void         Hw_Disp_Switch(enum E_HW_DISP_ONOFF set_state);
void         Hw_Disp_Output(const char *viewport);
void         Hw_Disp_Viewport_Conv(char *viewport);

/*keyboard*/
void         Hw_Keybd_Handler(void);
unsigned int Hw_Keybd_Check_Keys(void);
enum E_KEY   Hw_Keybd_QU_Read(void);
void         Hw_Keybd_Set_Mode(enum HW_KEYBD_MODE set_mode);
unsigned int Hw_Keybd_Get_Enkey(void);

/*signal handler*/
void         Hw_Sig_Handler(uint32_t timer_period);
void         Hw_Sig_Beep_Handler(uint32_t timer_period);


/*the battery buffered backup ram domain is 4k in size. So if the backup
game structure gets larger than 4k, this will not work.*/
BUILD_ASSERT((sizeof(SYS_BACKUP) <= BKPSRAM_SIZE),_build_assert_no_0,"Game save structure too large!");

/*--------- external variables ----------*/

/*from the boot_stm32f405.c startup file: the results of the RAM test and
of the system speed rampup are stored at the bottom of the stack. they are
read, evaluated and cleared.
ct_stack[0] contains the RAM test result,
ct_stack[1] contains the XTAL (quartz/clock) test result.*/
extern uint32_t ct_stack[];

/*enable the following define for generating a build with the watchdog
disabled. that is useful for debugging on the target platform as it prevents
the watchdog from resetting the system once you pause the program in the
debugger.*/
//#define HW_CT_DEBUG

#ifdef PC_VERSION
#error "in ctdefs.h, you must undefine PC_VERSION for the ARM build."
#endif

#define RAM_TEST_OK_RESULT   0x00000042UL
#define XTAL_TEST_OK_RESULT  0x00000021UL
#define IMAGE_LENGTH         (384UL * 1024UL)
#define WDG_SYS_RESET_OK     0x1C0FFEE0UL

/*output GPIO*/
#define LIGHT_GPIO_BSRR      GPIOA_BSRR
#define LED_GPIO_BSRR        GPIOB_BSRR
#define BUZ_GPIO_BSRR        GPIOB_BSRR
#define ONBOARDLED_BSRR      GPIOC_BSRR

#define ONBOARDLED_PIN_ON    (GPIO_Pin_12 << 16)
#define ONBOARDLED_PIN_OFF   (GPIO_Pin_12)

/*there are a lot of volatile variables here. their point is that they usually get
set from the interrupt, but they can also get initialised by the application.
so they are volatile to ensure that no crazy compiler optimisation kicks them
out.

Some smaller variables are pushed into CCM-RAM because there is still some space left,
and this way, we get the remaining total free RAM in the SRAM as one block, for
future use.*/

void Play_Save_Status(BACKUP_GAME *ptr);
void Play_Load_Status(const BACKUP_GAME *ptr);

#define BAT_VOLTAGE_BUF_LEN_BITS 6U
static volatile uint32_t battery_voltage[1UL << BAT_VOLTAGE_BUF_LEN_BITS];
static volatile uint32_t battery_voltage_cnt;
static volatile uint32_t battery_voltage_avg;
volatile uint32_t battery_status; /*might get changed in the timer interrupt*/
int battery_confirmation;

/*measurement levels for PCB with 10k/20k voltage divisor*/
static uint32_t FLASH_ROM const battery_levels_20k[BATTERY_LEVEL_ENTRIES] =
{
    BATTERY_SHUTDOWN_LEVEL_20K << BAT_VOLTAGE_BUF_LEN_BITS,
    BATTERY_HI_TO_LO_LEVEL_20K << BAT_VOLTAGE_BUF_LEN_BITS,
    BATTERY_STARTUP_VOLTAGE_20K,
    BATTERY_LO_TO_HI_LEVEL_20K << BAT_VOLTAGE_BUF_LEN_BITS,
    BATTERY_NORM_LEVEL_20K,
    BATTERY_LOW_VALID_LEVEL_20K,
    BATTERY_HIGH_VALID_LEVEL_20K
};

/*measurement levels for PCB with 10k/47k voltage divisor*/
static uint32_t FLASH_ROM const battery_levels_47k[BATTERY_LEVEL_ENTRIES] =
{
    BATTERY_SHUTDOWN_LEVEL_47K << BAT_VOLTAGE_BUF_LEN_BITS,
    BATTERY_HI_TO_LO_LEVEL_47K << BAT_VOLTAGE_BUF_LEN_BITS,
    BATTERY_STARTUP_VOLTAGE_47K,
    BATTERY_LO_TO_HI_LEVEL_47K << BAT_VOLTAGE_BUF_LEN_BITS,
    BATTERY_NORM_LEVEL_47K,
    BATTERY_LOW_VALID_LEVEL_47K,
    BATTERY_HIGH_VALID_LEVEL_47K
};

/*on system startup, this pointer is set to the correct measurement table
  above as per the PCB pin programming on ports PA5-PA7.*/
static const uint32_t *battery_levels;

static int user_interaction_passed;

uint64_t hw_config;
static enum E_AUTOSAVE autosave_state;
static int backup_ram_ok;
static int poweron_keys_pressed;

static void* const FLASH_ROM backup_ram_ptr = (void *) BKPSRAM_BASE;

static volatile unsigned int hw_system_stopped;
static enum E_SYS_SPEED hw_system_speed;
static enum E_CLK_FORCE hw_speed_force;

static volatile int32_t sys_time; /*is changed by the timer interrupt*/
static volatile uint32_t system_core_clock_div_1M; /*spares that division in the delay routine*/
static uint32_t system_core_clock;

static uint32_t hw_randomness_state;
static volatile uint32_t hw_randomness_seed;

/*callback function to the HMI layer*/
static void (*Hw_Bat_Mon_Callback)(enum E_WHOSE_TURN side_to_move);

/*prototypes for the timer interrupt.*/
static void Hw_Battery_Handler(void);

#define NORM_TIMER_PERIOD      1L
#define SIGBAT_TIMER_PERIOD   10L
#define SHUTDOWN_TIMER_PERIOD 10L

/*microsecond busy waiting using the core debug timer*/
void Hw_Wait_Micsecs(uint32_t microseconds)
{
    uint32_t cycles;

    DWT_CYCCNT = 0;
    __DMB();
    cycles = system_core_clock_div_1M * microseconds;
    do  {  } while (DWT_CYCCNT < cycles);
}

/*sleeps until next interrupt or systick*/
void Hw_Sleep(void)
{
    Drv_CPU_Sleep();
}

/*handles keyboard including light key, signals and battery monitoring.*/
void FUNC_USED NEVER_INLINE Hw_SysTick_Handler(void)
{
    /*clear "Systick counter has reached zero" COUNTFLAG*/
    Drv_Clear_Systick();

    if (!hw_system_stopped)
    {
        static int32_t bat_timer, led_timer;

        /*needs to be called every 1 ms because of keyboard click accuracy
          with 10 ms duration and 5ms on / 5 ms off error beep*/
        Hw_Sig_Beep_Handler((uint32_t) NORM_TIMER_PERIOD);

        /*battery and signal handler every 10 ms, i.e. at even systicks
          because the longer keyboard handler run happens at odd ticks.
          resetting the system time to 0 does not influence the key scan
          sequence, so use a dedicated bat_timer here that is not touched
          upon system time reset either.*/
        if (bat_timer >= SIGBAT_TIMER_PERIOD - NORM_TIMER_PERIOD)
        {
            Hw_Battery_Handler();
            Hw_Sig_Handler((uint32_t) SIGBAT_TIMER_PERIOD);
            bat_timer = 0;
        } else
            bat_timer += NORM_TIMER_PERIOD;

        if (sys_time < MAX_SYS_TIME) /*prevent overflow*/
            sys_time += NORM_TIMER_PERIOD;

        Hw_Keybd_Handler();

        /*toggle the onboard LED on the Olimex board, this makes
         potential troubleshooting easier. cycle: 100 ms on, 900 ms off.*/
        if (led_timer == 0)
            Drv_SETRESET_BITS(ONBOARDLED_BSRR, ONBOARDLED_PIN_ON);
        else if (led_timer == 100L)
            Drv_SETRESET_BITS(ONBOARDLED_BSRR, ONBOARDLED_PIN_OFF);

        if (led_timer < 1000L - NORM_TIMER_PERIOD)
            led_timer += NORM_TIMER_PERIOD;
        else
            led_timer = 0;
    } else /*system is stopped because of battery failure*/
        sys_time += SHUTDOWN_TIMER_PERIOD;

    __DSB();
}

/*this function is called from the inline assembly in the Exception Handler, which
is not visible to the C compiler. two consequences:
a) it must be labeled "used" so that it is not optimised away, leading to a linker error.
b) it must not be inlined at the FLTO stage or so because it is using the stack, which
might have to get repaired by the Exception Handler first.*/
static void FUNC_USED NEVER_INLINE Hw_Fault_Exception_Processing(uint32_t address)
{
    uint32_t fault_register, time_cnt;
    static char hw_viewport[81];

    fault_register = SCB_CFSR;

    Drv_IWDG_TRG_WD();

    /*switch off beeper*/
    Drv_SETRESET_BITS(BUZ_GPIO_BSRR, BUZZ_ODR_OUT << 16);

    /*Systick cannot happen in a fault exception, but just disable it.*/
    Drv_Stop_Systick();

    /*ramp down system speed with disregard to USART and USB so that the
      system does not burn through energy at maximum rate just for the
      fault display.*/
    (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_OSC, &system_core_clock);
    /*for the microsecond waiting routine*/
    system_core_clock_div_1M = system_core_clock / 1000000UL;

    /*switch off green LED, switch on red LED*/
    Drv_SETRESET_BITS(LED_GPIO_BSRR, (LED_GR_ODR_OUT << 16) | LED_RD_ODR_OUT);
    /*activate display backlight if configured*/
    Hw_Sig_Send_Msg(HW_MSG_LED_BACK_ON, BACKLIGHT_KEY, HW_MSG_PARAM_BACK_CONF);

    /*fake a dialogue box*/
    Util_Strcpy(hw_viewport, "+-----SYSFAULT-----+| addr: 0x12345678 || code: 0x12345678 |+-------<OK>-------+");

    Util_Long_Int_To_Hex(address, hw_viewport + 30);
    Util_Long_Int_To_Hex(fault_register, hw_viewport + 50);
    /*bypass the usual Hw_Disp_Show_All() because it adds 96 bytes of stack usage*/
    Hw_Disp_Viewport_Conv(hw_viewport); /*manual fancy dialogue conversion*/
    Hw_Disp_Output(hw_viewport);        /*and direct output*/
    Hw_Disp_Switch(HW_DISP_ON);

    /*the regular keypad feedback doesn't work here because that is set up
    using the SysTick exception for the handling. But here, we ARE already in
    an exception.*/

    Hw_Keybd_Set_Mode(HW_KEYBD_MODE_ENT);

    time_cnt = 0;

    do {
        Hw_Wait_Micsecs(10000UL); /*wait 10ms - not via system timer because we are in an interrupt*/

        time_cnt++;
        if (time_cnt >= 100UL) /*1 second*/
        {
            time_cnt = 0;
            Drv_SETRESET_BITS(LED_GPIO_BSRR, LED_RD_ODR_OUT); /*switch on red LED*/
        } else if (time_cnt == 10UL) /*100ms*/
            Drv_SETRESET_BITS(LED_GPIO_BSRR, LED_RD_ODR_OUT << 16); /*switch off red LED*/

        Drv_IWDG_TRG_WD();

    } while (!Hw_Keybd_Get_Enkey());

    /*do keyboard click if configured*/
    Hw_Sig_Send_Msg(HW_MSG_BEEP_ON, BEEP_CLICK, HW_MSG_PARAM_CLICK);
    /*the beep will not get set off in the regular beep signal handler because that
    is being called from the SysTick handler, which is switched off at this point.*/
    Hw_Wait_Micsecs(BEEP_CLICK * 1000UL);
    /*switch off beeper*/
    Drv_SETRESET_BITS(BUZ_GPIO_BSRR, BUZZ_ODR_OUT << 16);

    Drv_DWT_Stop();
    Drv_IWDG_Setup_Watchdog(100UL /*milliseconds*/);

    Hw_Sys_Reset();
}

/*we need to get the faulting program address, and that is only possible
in assembler.

Note that we can't detect stack overflows here. That would require
to have this function "naked", but then the faulting program
address is NOT pushed to the stack. Faults other than a stack
overflow would be hard to track.

So the stack is dimensioned to be OK plus some reserve, and we
can neglect stack overflow.*/
void FUNC_USED NEVER_INLINE Hw_Fault_Exception_Handler(void)
{
    __asm volatile (
        ".thumb_func\n"
        "CPSID i\n"                    /*switch off all interrupts*/
        "TST LR,#4\n"
        "ITE EQ\n"
        "MRSEQ R1,MSP\n"
        "MRSNE R1,PSP\n"
        "LDR R0,[R1,#24]\n"
        "B Hw_Fault_Exception_Processing\n"
        "\n" : : : "memory");
}

/*to be called from the Systick handler every 10 ms*/
static void Hw_Battery_Handler(void)
{
    uint32_t bat_reading, bat_cnt, bat_avg;

    /*read the value*/
    bat_reading = ADC1_DR;

    /*start the next conversion - that's why the usual while-loop for waiting
    for the conversion to complete is not necessary here. The reading will be
    in the next timer interrupt, i.e. in 10ms. That's so far more than enough
    that checking would be pointless.
    the data will be outdated by 10ms, but that is irrelevant for monitoring
    the battery level.*/
    ADC1_CR2 = ADC1_CR2_ADON | ADC1_CR2_SWSTART;

    /*preserve the LSB for the random seeding*/
    hw_randomness_seed <<= 1;
    hw_randomness_seed |= bat_reading & 1UL;

    /*measurement value clipping*/
    if (bat_reading > battery_levels[BATTERY_HIGH_VALID_LEVEL]) /*clip too high values*/
        bat_reading = battery_levels[BATTERY_HIGH_VALID_LEVEL];
    else if (bat_reading < battery_levels[BATTERY_LOW_VALID_LEVEL]) /*clip too low values*/
        bat_reading = battery_levels[BATTERY_LOW_VALID_LEVEL];

    bat_cnt = battery_voltage_cnt;
    /*subtract the oldest bat reading entry (that is to be overwritten
      with the current measurement value)*/
    bat_avg  = battery_voltage_avg;
    bat_avg -= battery_voltage[bat_cnt];
    /*add up the the clipped current reading to the average*/
    bat_avg += bat_reading;

    battery_voltage_avg = bat_avg;

    battery_voltage[bat_cnt++] = bat_reading;
    bat_cnt &= (1UL << BAT_VOLTAGE_BUF_LEN_BITS) - 1UL; /*wrap-around*/
    battery_voltage_cnt = bat_cnt;

    /*the bat average contains the pure sum of the measurement values.*/
    if (bat_avg >= battery_levels[BATTERY_LO_TO_HI_LEVEL])
        battery_status = BATTERY_HIGH;
    else if (bat_avg <= battery_levels[BATTERY_SHUTDOWN_LEVEL])
        battery_status = BATTERY_SHUTDOWN | BATTERY_LOW;
    else if (bat_avg <= battery_levels[BATTERY_HI_TO_LO_LEVEL])
        battery_status = BATTERY_LOW;
}

/*checks whether autosave can be used or whether there is a manually saved game.*/
static enum E_AUTOSAVE Hw_Get_Autosave_State(int *bckp_ram_ok)
{
    uint32_t crc_32;
    BACKUP_GAME *backup_ptr;

    *bckp_ram_ok = 0;

    backup_ptr = &(((SYS_BACKUP *) backup_ram_ptr)->backup_game);

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

/*triggers the watchdog and reads the keyboard, non-blocking.
called from the application to get the keyboard input (if any).*/
enum E_KEY Hw_Getch(enum E_WAIT_SLEEP sleep_mode)
{
    enum E_KEY pressed_key;

    Drv_IWDG_TRG_WD();

    pressed_key = Hw_Keybd_QU_Read();

    if (battery_status & BATTERY_SHUTDOWN) /*battery very low?*/
    {
         /*the battery shutdown monitoring in the HMI,
           see Hmi_Battery_Shutdown()*/
        if (Hw_Bat_Mon_Callback != NULL)
            (*Hw_Bat_Mon_Callback)(USER_TURN); /*shut down system*/
    }

    /*if nothing is going on, save energy. the next interrupt or systick
      will wake up the system. The systick has to wake up the CPU because
      even in user idle mode, the thinking time in the display needs to
      be updated regularly.
      The race condition here between the check and WFI can be ignored
      because of the systick wakeup.*/
    if ((pressed_key == KEY_NONE) && (sleep_mode == SLEEP_ALLOWED))
        Drv_CPU_Sleep();

    return(pressed_key);
}

/*sets the system speed either to low, for saving energy, or to high
for maximum computing power. the overclocking configuration option is
taken care of.
the system mode refers to the keyboard handling.*/
void Hw_Set_Speed(enum E_SYS_SPEED speed, enum E_SYS_MODE mode, enum E_CLK_FORCE force_high)
{
    /*Disable the systick before reconfiguring that stuff.*/
    Drv_Stop_Systick();

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
            if (cfg_option == CFG_CLOCK_145)
                (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_240MHZ, &system_core_clock);
            else
                (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_216MHZ, &system_core_clock);
        } else
        {
            if ((cfg_option == CFG_CLOCK_100) || (force_high == CLK_FORCE_HIGH))
                (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_168MHZ, &system_core_clock);
            else
            {
                switch (cfg_option)
                {
                case CFG_CLOCK_070:
                    (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_120MHZ, &system_core_clock);
                    break;
                case CFG_CLOCK_050:
                    (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_84MHZ, &system_core_clock);
                    break;
                case CFG_CLOCK_025:
                    (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_42MHZ, &system_core_clock);
                    break;
                case CFG_CLOCK_010:
                    (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_18MHZ, &system_core_clock);
                    break;
                default:
                    (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_168MHZ, &system_core_clock);
                    break;
                }
            }
        }
        hw_system_speed = SYSTEM_SPEED_HIGH;
    } else /*low speed mode*/
    {
        (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_USER, &system_core_clock);
        hw_system_speed = SYSTEM_SPEED_LOW;
    }

    /*for the microsecond waiting routine - avoiding to do that division
    within the delay routine makes it more precise.*/
    system_core_clock_div_1M = system_core_clock / 1000000UL;

    if (mode == SYSTEM_MODE_USER)
        Hw_Keybd_Set_Mode(HW_KEYBD_MODE_USER);
    else if (mode == SYSTEM_MODE_COMP)
        Hw_Keybd_Set_Mode(HW_KEYBD_MODE_COMP);

    /*1ms timer interrupt*/
    Drv_Start_Systick(system_core_clock, 1000UL / NORM_TIMER_PERIOD);
}

/*checks whether underclocking is active, and if so, reduces the speed.*/
void Hw_Throttle_Speed(void)
{
    uint64_t cfg_option = CFG_GET_OPT(CFG_CLOCK_MODE);

    hw_speed_force = CLK_ALLOW_LOW;

    if (cfg_option >= CFG_CLOCK_100) /*no underclocking*/
        return;

    Drv_Stop_Systick();

    switch (cfg_option)
    {
    case CFG_CLOCK_070:
        (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_120MHZ, &system_core_clock);
        break;
    case CFG_CLOCK_050:
        (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_84MHZ, &system_core_clock);
        break;
    case CFG_CLOCK_025:
        (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_42MHZ, &system_core_clock);
        break;
    case CFG_CLOCK_010:
        (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_18MHZ, &system_core_clock);
        break;
    default:
        break;
    }

    system_core_clock_div_1M = system_core_clock / 1000000UL;

    Drv_Start_Systick(system_core_clock, 1000UL / NORM_TIMER_PERIOD);
}

/*wait and sleep between the systick interrupts*/
static void Hw_Wait_In_Powerdown(int32_t target_time)
{
    while (sys_time < target_time)
    {
        Drv_IWDG_TRG_WD();
        Drv_CPU_Sleep();
    }
}

/*shuts down most energy consumption, but that is not an actual sleep mode.
the LED has to blink, or else the user would think he hasn't to switch the device off.
BUT: the board and the voltage converters STILL draw current even if we put the
CPU in total ARM-no-power-sleep-preserve mode. And that would destroy the batteries,
NiMH batteries are picky with deep discharge. So keep the red LED blinking, then the
user knows the system isn't content until switched off.

NOTE: in all the calling paths, we are already at LOW system speed at this point
because the user had to be informed beforehand, and that required to change to
user mode with low speed.*/
void Hw_Powerdown_System(void)
{
    unsigned int speaker_active;
    /*the keyboard click may have been active: wait for the time that the click
    would take, then switch off the peripherals. the user will perceive that as
    regular keyboard click.*/
    Drv_Stop_Systick();

    /*ramp down system speed with disregard to USART and USB*/
    (void) Drv_Set_Sys_Speed_Type(SYS_SPEED_OSC, &system_core_clock);
    /*for the microsecond waiting routine*/
    system_core_clock_div_1M = system_core_clock / 1000000UL;

    /*wait for 10 ms via CPU frequency calibrated delay loop*/
    Hw_Wait_Micsecs(BEEP_CLICK * 1000UL);

    Drv_SETRESET_BITS(ONBOARDLED_BSRR, ONBOARDLED_PIN_OFF);  /*switch off onboard-LED*/
    Drv_SETRESET_BITS(BUZ_GPIO_BSRR, BUZZ_ODR_OUT << 16);    /*switch off buzzer*/
    Drv_SETRESET_BITS(LED_GPIO_BSRR, LED_GR_ODR_OUT << 16);  /*switch off green LED*/
    Drv_SETRESET_BITS(LIGHT_GPIO_BSRR, LIGHT_ODR_OUT << 16); /*switch off display backlight*/
    Hw_Disp_Switch(HW_DISP_OFF);                          /*switch off display*/

    /*keep only port B enabled for LED and buzzer*/
    Drv_Periph_Shutdown();

    /*set Systick to disregard keyboard scan and signal handler*/
    hw_system_stopped = 1;
    /*reset system time*/
    Hw_Set_System_Time(0);
    /*and start Systick again with 10 ms period*/
    Drv_Start_Systick(system_core_clock, 1000UL / SHUTDOWN_TIMER_PERIOD);

    if (!(CFG_HAS_OPT(CFG_SPEAKER_MODE, CFG_SPEAKER_OFF)))
        speaker_active = 1;
    else
        speaker_active = 0;

    for (;;)
    {
        int32_t start_cycle_time = sys_time;

        /*red LED on for a 100 ms*/
        Drv_SETRESET_BITS(LED_GPIO_BSRR, LED_RD_ODR_OUT);

        if (speaker_active)
        {
            /*beeper on for 10 ms unless config is set to silent mode*/
            Drv_SETRESET_BITS(BUZ_GPIO_BSRR, BUZZ_ODR_OUT);
            Hw_Wait_In_Powerdown(start_cycle_time + BEEP_CLICK);
            /*beeper off again*/
            Drv_SETRESET_BITS(BUZ_GPIO_BSRR, BUZZ_ODR_OUT << 16);
        }

        Hw_Wait_In_Powerdown(start_cycle_time + 100UL);
        /*red LED off again*/
        Drv_SETRESET_BITS(LED_GPIO_BSRR, LED_RD_ODR_OUT << 16);
        /*after a total cycle period of 1 second, repeat*/
        Hw_Wait_In_Powerdown(start_cycle_time + 1000UL);
    }
}

/*gets the system time in milliseconds*/
int32_t Hw_Get_System_Time(void)
{
    return (sys_time);
}

/*sets the system time in milliseconds*/
void Hw_Set_System_Time(int32_t new_time)
{
    sys_time = new_time;
}

/*returns whether the filtered voltage is OK for starting a new game.*/
int Hw_Battery_Newgame_Ok(void)
{
    uint32_t bat_avg = battery_voltage_avg >> BAT_VOLTAGE_BUF_LEN_BITS;

    if (bat_avg >= battery_levels[BATTERY_STARTUP_VOLTAGE])
        return(1);

    return(0);
}

/*this backup RAM test writes something different to each cell for
being able to detect also address line problems.*/
static int Hw_Test_Backup_Ram(void)
{
    volatile uint32_t *backup_ram_area;
    uint32_t write_value, read_value, i, ram_test_fail = 0;
    const uint32_t test_start_value = 0x8F0E32A6UL;

    backup_ram_area = (volatile uint32_t *) backup_ram_ptr;
    write_value = test_start_value;

    Drv_Unlock_BKP();

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
            ram_test_fail++;
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
            ram_test_fail++;
        write_value = (write_value >> 19) | (write_value << (32-19));
    }

    Drv_Lock_BKP();

    autosave_state = HW_AUTOSAVE_ON;

    return(ram_test_fail == 0);
}

/*hardware initialisation of the system*/
void Hw_Setup_System(void)
{
    uint32_t hw_pcb_id;
    unsigned int i;

    Hw_Bat_Mon_Callback = NULL;

    /*system clock was set in the booter code*/
    system_core_clock = SPEED_168MHZ;
    system_core_clock_div_1M = SPEED_168MHZ / 1000000UL; /*for the microsecond waiting routine*/

    hw_randomness_state = 0;
    hw_randomness_seed = 0;

    sys_time = 0;
    hw_system_stopped = 0;
    user_interaction_passed = 0;

    Drv_DWT_Setup();
    hw_pcb_id = Drv_Periph_Enable();

    /*evaluate HW PCB features as per PA5-7 pin programming,
      valid range is 0 - 7.*/
    switch (hw_pcb_id)
    {
    case HW_PCB_ID_VD:
        battery_levels = battery_levels_47k;
        break;
    case HW_PCB_ID_ORIGINAL:
    default:
        battery_levels = battery_levels_20k;
        break;
    }

    /*avoid an initial false "battery low" warning*/
    for (i = 0; i < (1U << BAT_VOLTAGE_BUF_LEN_BITS); i++)
        battery_voltage[i] = battery_levels[BATTERY_NORM_LEVEL];

    /*the average is the sum of the values*/
    battery_voltage_avg = battery_levels[BATTERY_NORM_LEVEL] << BAT_VOLTAGE_BUF_LEN_BITS;
    battery_voltage_cnt = 0;
    battery_status = BATTERY_HIGH;
    battery_confirmation = BATTERY_CONF_HIGH;

#ifndef HW_CT_DEBUG
    Drv_IWDG_Setup_Watchdog(2000UL /*milliseconds*/);
#endif

    /*check for "stuck" keys, return value must be zero*/
    poweron_keys_pressed = Hw_Keybd_Check_Keys();
    Hw_Keybd_Set_Mode(HW_KEYBD_MODE_INIT);

    Hw_Load_Config(&hw_config);

    autosave_state = Hw_Get_Autosave_State(&backup_ram_ok);
    /*if there is a valid config, then backup_ram_ok is now 1, otherwise 0.*/
    if (!backup_ram_ok) /*no valid config - do a dedicated RAM check.*/
        backup_ram_ok = Hw_Test_Backup_Ram();

    Hw_Sig_Send_Msg(HW_MSG_INIT, HW_MSG_NO_DURATION, HW_MSG_PARAM_NONE);

    Hw_Disp_Set_Conf_Contrast();

    /*now enable the display DAC contrast output.
    the buffer must be enabled, or else the contrast will not work.*/
    DAC_CR = DAC_CR_EN1;

    Hw_Disp_Init(); /*get the display going.*/

    /*Hw_Set_Speed() will turn on the timer interrupt*/
    Hw_Set_Speed(SYSTEM_SPEED_HIGH, SYSTEM_MODE_USER, CLK_FORCE_HIGH);
}

/*set up the battery shutdown callback handler to the HMI layer*/
void Hw_Set_Bat_Mon_Callback(void (*Bat_Mon_Callback)(enum E_WHOSE_TURN))
{
    Hw_Bat_Mon_Callback = Bat_Mon_Callback;
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
    Drv_IWDG_TRG_WD();
}

/*wait for the watchdog do bite*/
void Hw_Sys_Reset(void)
{
    Drv_Unlock_BKP();
    /*set up the "watchdog has bitten on purpose"-flag*/
    RTC_BKP3R = WDG_SYS_RESET_OK;
    Drv_Lock_BKP();
    for (;;) ;
}

/*save the configuration to two of the backup registers.
the CRC32 of the 4 config bytes goes into a third register.*/
void Hw_Save_Config(const uint64_t *config)
{
    uint32_t crc_32;

    crc_32 = Util_Crc32(((void *) config), sizeof(uint64_t));
    Drv_Unlock_BKP();
    RTC_BKP0R = (uint32_t)((*config) >> 32);
    RTC_BKP1R = (uint32_t)((*config) & 0xFFFFFFFFULL);
    RTC_BKP2R = crc_32;
    Drv_Lock_BKP();
}

/*sets and saves the default configuration from confdefs.h*/
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

    cfg_0 = RTC_BKP0R;
    cfg_1 = RTC_BKP1R;
    cfg_2 = RTC_BKP2R;

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
        /*system time gets reset to 0.*/
        const int32_t system_time = 0;

        hw_config = backup_ptr->hw_config;
        hw_randomness_state = backup_ptr->randomness_state;

        Time_Load_Status(backup_ptr, system_time); /*get the time management variables*/
        Hmi_Load_Status(backup_ptr);               /*get the pretty print*/
        Menu_Load_Status(backup_ptr, system_time); /*get the menu start time*/
        Play_Load_Status(backup_ptr);              /*get the game-related stuff*/

        Hw_Disp_Set_Conf_Contrast();

        /*for the adjusted time stamps*/
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
    are switched off.*/

    SYS_BACKUP sys_backup; /*4k stack!*/
    int sys_speed_changed;
    int32_t system_time;
    uint32_t int_disable_state;

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

    /*minimise the critical power loss time window*/
    int_disable_state = __GET_PRIMASK();
    __DISABLE_IRQ();
    Drv_Unlock_BKP();
    Util_Memcpy(backup_ram_ptr, (void *) &sys_backup, sizeof (SYS_BACKUP));
    Drv_Lock_BKP();
    /*reenable interrupts only if they had been enabled before*/
    if (!int_disable_state)
        __ENABLE_IRQ();

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
    Drv_Unlock_BKP();
    Util_Memzero((void *) backup_ram_ptr, BKPSRAM_SIZE);
    Drv_Lock_BKP();
    autosave_state = HW_AUTOSAVE_ON; /*re-enable autosave*/
    return(HW_FILEOP_OK_AUTO);
}

/*the random number generator is available in hardware, too - but the
software shall also be portable to other microcontrollers.*/
void Hw_Seed(void)
{
    hw_randomness_state = hw_randomness_seed;
}

/*the keyboard handler uses the keypress timing for more entropy.*/
void Hw_Add_Randomness(void)
{
    uint32_t new_bits;

    new_bits  = (uint32_t)(sys_time / NORM_TIMER_PERIOD);
    new_bits &= 0x03UL;

    hw_randomness_state <<= 2;
    hw_randomness_state  |= new_bits;
}

/*returns an integer between 0 and 65535.*/
uint32_t Hw_Rand(void)
{
    hw_randomness_state *= 1103515245UL;
    hw_randomness_state += 12345UL;
    return(hw_randomness_state >> 16);
}

/*verifies the integrity of the firmware via CRC-32.
  also used for checking system stability when overclocking.*/
unsigned int Hw_Check_FW_Image(void)
{
    uint32_t expected_crc32, observed_crc32;
    /*last four bytes are for storing the CRC-32.*/
    observed_crc32 = Util_Crc32((void *) FLASH_BASE_ADDR, IMAGE_LENGTH - sizeof(uint32_t));
    expected_crc32 = Util_Hex_Long_To_Int((void *) (FLASH_BASE_ADDR + IMAGE_LENGTH - sizeof(uint32_t)));
    if (observed_crc32 != expected_crc32)
        return(HW_ROM_FAIL);
    else
        return(HW_SYSTEM_OK);
}

/*does three things:
- check the firmware image (CRC)
- get the result from the RAM test in the bootup code
- get the result from switching on the external quarz (also during bootup)*/
unsigned int Hw_Check_RAM_ROM_XTAL_Keys(void)
{
    uint32_t res;

    /*volatile because the failure codes got set up outside of the
      C compiler's context. the compiler could be free to analyse that
      the first read access to a global variable that is not initialised
      explicitely must result in 0 because the C standard demands that.
      volatile forces an actual read.*/
    volatile uint32_t *ct_stack_ptr;

    res = Hw_Check_FW_Image();

    if (!backup_ram_ok)
        res |= HW_RAM_FAIL;

    ct_stack_ptr = (volatile uint32_t *) ct_stack;

    /*that one got set up in the reset handler - see startup file*/
    if (*ct_stack_ptr != RAM_TEST_OK_RESULT)
        res |= HW_RAM_FAIL;

    *ct_stack_ptr++ = 0; /*clear the status*/

    if (*ct_stack_ptr != XTAL_TEST_OK_RESULT)
        res |= HW_XTAL_FAIL;

    *ct_stack_ptr = 0; /*clear the status*/

    if (poweron_keys_pressed != 0) /*any stuck keys?*/
        res |= HW_KEYS_FAIL;

    return(res);
}

/*map the reset register bits to logical system reset causes.

the watchdog reset is a bit tricky because the intentional system
reset is also done by the watchdog. so the backup register which holds
this information has been considered and cleared.

caution here: the reset pin will be triggered by all of the
other reset causes so that a real reset by the reset pin
only has been occurring if none of the other reset causes
is there!*/
unsigned int Hw_Get_Reset_Cause(void)
{
    uint32_t reg32, result;
    reg32 = RCC_CSR;
    result = 0;

    if (reg32 & (RCC_CSR_LPWRRSTF | RCC_CSR_PORRSTF | RCC_CSR_BORRSTF))
        result |= HW_SYSRESET_POWER;

    if (reg32 & (RCC_CSR_WWDGRSTF | RCC_CSR_IWDGRSTF))
    {
        /*if the watchdog has bitten, but due to an intentional system reset,
        then this does not count as software error, so don't report it. just clear
        the flag.*/
        if (RTC_BKP3R == WDG_SYS_RESET_OK)
        {
            Drv_Unlock_BKP();
            RTC_BKP3R = 0;
            Drv_Lock_BKP();
        } else
            result |= HW_SYSRESET_WDG;
    }
    if (reg32 & (RCC_CSR_SFTRSTF))
        result |= HW_SYSRESET_SW;

    if ((reg32 & (RCC_CSR_LPWRRSTF | RCC_CSR_PORRSTF | RCC_CSR_BORRSTF
                  | RCC_CSR_WWDGRSTF | RCC_CSR_IWDGRSTF | RCC_CSR_SFTRSTF)) == 0)
        if (reg32 & (RCC_CSR_PINRSTF))
            result |= HW_SYSRESET_PIN;

    return(result);
}

/*clear the reset status register*/
void Hw_Clear_Reset_Cause(void)
{
    RCC_CSR |= RCC_CSR_RMVF;
}

/*tell the autosave state for a potential warning on startup
if the autosave is disabled. it would be annoying to rely on
autosave, have a wall power outage and only then discover that
the game has not been saved!*/
enum E_AUTOSAVE Hw_Tell_Autosave_State(void)
{
    return(autosave_state);
}

/*ok, the user has had the possibility to interact. now we can
kick in the overclocking (if configured).*/
void Hw_User_Interaction_Passed(void)
{
    user_interaction_passed = 1;
}
