/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2016-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of the CT800 (ARM CPU driver).
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
#include "ctdefs.h"
#include "arm_driver.h"

/*--------- no external variables ---------*/

/*disable write access to the backup domain*/
void Drv_Lock_BKP(void)
{
    __DSB();
    PWR_CR &= ~PWR_CR_DBP;
    __DSB();
}

/*enable write access to the backup domain*/
void Drv_Unlock_BKP(void)
{
    __DSB();
    PWR_CR |=  PWR_CR_DBP;
    __DSB();
}

/*** central Periph_Clock function ***********************************/
/*set up all peripherals that the system needs and returns HW PCB ID*/
uint32_t Drv_Periph_Enable(void)
{
    uint32_t reg32;

    /*reset the peripherals*/
    RCC_AHB1RSTR = RCC_AHB1RSTR_GPCRST | RCC_AHB1RSTR_GPBRST | RCC_AHB1RSTR_GPARST;
    RCC_APB1RSTR = RCC_AHB1RSTR_DACRST;
    RCC_APB2RSTR = RCC_APB2RSTR_ADCRST;
    RCC_AHB1RSTR = 0;
    RCC_APB1RSTR = 0;
    RCC_APB2RSTR = 0;

    /*enable the clocks*/
    RCC_AHB1ENR = RCC_AHB1ENR_GPCEN | RCC_AHB1ENR_GPBEN | RCC_AHB1ENR_GPAEN | RCC_AHB1ENR_BKPEN | RCC_AHB1ENR_CCMEN;
    RCC_APB1ENR = RCC_APB1ENR_DACEN | RCC_APB1ENR_PWREN;
    RCC_APB2ENR = RCC_APB2ENR_ADC1EN;
    __DSB(); /*see STM32F40x errata sheet - use DSB after RCC enable*/

    /*light req/cmd, ADC, DAC, PCB ID config on PA4*/
    GPIOA_MODER = PORT_A_MODER_OUT | PORT_A_MODER_RESET;
    GPIOA_PUPDR = LIGHT_PUPDR | PCB_ID_PUPDR | PORT_A_PUPDR_RESET;

    /*ADC is on PA3, that's ADC1 on ADC channel 3.*/
    ADC_CCR = ADC_CCR_PREDIV8 | ADC_CCR_DELAY;
    ADC1_CR1  = 0;
    ADC1_CR2  = 0;
    ADC1_SQR1 = 0;
    ADC1_SMPR2 = ADC1_SMPR2_CH3_MAX; /*maximum sampling time to get the input impedance up*/
    ADC1_SQR3  = ADC1_SQR3_SQ1_CH3; /*map ADC channel 3 to ADC1*/
    ADC1_CR2   = ADC1_CR2_ADON;
    /*start AD conversion. the reference manual says this is only possible
      when the ADON bit is set, but it does not explain what this means:
        1) ADON has to be set already
        OR
        2) ADON can be set at the same time as SWSTART
      => to be on the safe side, ADON has already been set in the previous
      line, and now SWSTART is set additionally.*/
    ADC1_CR2 = ADC1_CR2_ADON | ADC1_CR2_SWSTART;

    /*DAC not enabled yet, that's done in hardware_arm.c AFTER finding out the config.*/

    /*LEDs, display and buzzer on port B, pins 0/1, 5-14 and 15*/
    GPIOB_MODER = PORT_B_MODER_OUT | PORT_B_MODER_RESET;
    GPIOB_PUPDR = PORT_B_PUPDR_RESET;

    /*PORT C: keypad. not set up here, only PORTC was clocked.*/

    /*enable backup regulator for backup RAM if not enabled already*/
    if ((PWR_CSR & PWR_CSR_BRR) != PWR_CSR_BRR)
    {
        /*enable write access to backup SRAM and backup registers*/
        Drv_Unlock_BKP();
        /*enable backup voltage regulator*/
        PWR_CSR |= PWR_CSR_BRE;
        /*wait for the regulator to come up*/
        while ((PWR_CSR & PWR_CSR_BRR) != PWR_CSR_BRR) ;
        /*disable write access to backup domain*/
        Drv_Lock_BKP();
    }

    /*read HW PCB ID on PA5-7, grounded pins*/
    reg32   = GPIOA_IDR;
    reg32   = ~reg32;        /*active low logic*/
    reg32  &= PCB_ID_REQ;    /*filter out PA5-7*/
    reg32 >>= PCB_ID_OFFSET; /*map them to numbers 0-7*/

    /*switch off the unnecessary PCB ID pull-ups*/
    GPIOA_PUPDR = LIGHT_PUPDR | PORT_A_PUPDR_RESET;

    if ((reg32 == HW_PCB_ID_ORIGINAL) || (HW_PCB_ID_VD))
    {
        /*put PA-8 to 0, that is ESP_EN*/
        GPIOA_MODER = PORT_A_MODER_OUT | ESP_EN_MODER | PORT_A_MODER_RESET;
        GPIOA_BSRR = ESP_EN_ODR_OUT << 16;
    }

    return(reg32);
}

/*not used*/
void Drv_Periph_Disable(void)
{
    /*disable write access to backup domain*/
    Drv_Lock_BKP();

    /*reset the peripherals*/
    RCC_AHB1RSTR = RCC_AHB1RSTR_GPCRST | RCC_AHB1RSTR_GPBRST | RCC_AHB1RSTR_GPARST;
    RCC_APB1RSTR = RCC_AHB1RSTR_DACRST;
    RCC_APB2RSTR = RCC_APB2RSTR_ADCRST;
    RCC_AHB1RSTR = 0;
    RCC_APB1RSTR = 0;
    RCC_APB2RSTR = 0;

    /*and disable them*/
    RCC_APB1ENR &= ~RCC_APB1ENR_DACEN;
    RCC_APB2ENR &= ~RCC_APB2ENR_ADC1EN;
    RCC_AHB1ENR &= ~(RCC_AHB1ENR_GPCEN | RCC_AHB1ENR_GPBEN | RCC_AHB1ENR_GPAEN | RCC_AHB1ENR_BKPEN | RCC_APB1ENR_PWREN);
}

/*when the system shuts down because of failing batteries, keep only
  CCM and port B enabled for LED and buzzer*/
void Drv_Periph_Shutdown(void)
{
    /*disable write access to backup domain*/
    Drv_Lock_BKP();

    /*and disable unnecessary peripherals*/
    RCC_AHB1ENR = RCC_AHB1ENR_GPBEN | RCC_AHB1ENR_CCMEN;
    RCC_APB1ENR = 0;
    RCC_APB2ENR = 0;
}

/*not used*/
void Drv_PWR_STBY_Mode(void)
{
    PWR_CR  |= PWR_CR_CWUF; /*reset the wakeup flag*/
    PWR_CR  |= PWR_CR_PDDS; /*activate STANDBY mode*/
    SCB_SCR |= SCB_SCR_SLEEPDEEP; /*set sleep_deep*/
    __WFI(); /*the interrupt will not happen - wait for power cycle*/
}

/*** watchdog related functions **************************************/

/*set up the wtachdog; WD timeout given in millisconds (up to 8000ms).*/
void Drv_IWDG_Setup_Watchdog(uint32_t wd_timeout_ms)
{
    /*unknown WD state - better retrigger it now*/
    IWDG_KR  = IWDG_KR_TRG;
    RCC_CSR |= RCC_CSR_LSION;
    while ((RCC_CSR & RCC_CSR_LSIRDY) != RCC_CSR_LSIRDY) ;
    wd_timeout_ms /= 2UL; /*500Hz -> one tick is 2ms -> 1ms is 1/2 tick*/
    wd_timeout_ms &= 0xFFFUL; /*maximum reload value -> up to 8 seconds*/

    /*wait until no reload operation is ongoing*/
    while (IWDG_SR & (IWDG_SR_RVU | IWDG_SR_PVU)) ;

    IWDG_KR  = IWDG_KR_CONF;
    IWDG_PR  = 4UL; /*prescaler 64: ~32kHz / 64 = ~500Hz*/
    IWDG_RLR = wd_timeout_ms;
    IWDG_KR  = IWDG_KR_START;
    __DSB();

    /*wait until reload operation has finished*/
    while (IWDG_SR & (IWDG_SR_RVU | IWDG_SR_PVU)) ;

    IWDG_KR  = IWDG_KR_TRG;
}

/*also available as macro in arm_driver.h, with a slightly different name*/
void Drv_IWDG_Trigger_Watchdog(void)
{
    IWDG_KR = IWDG_KR_TRG;
}

/*not used: turn off LSI, which is what makes the watchdog count down.*/
void Drv_IWDG_Disable_Watchdog(void)
{
    /*retrigger the watchdog to avoid it to hit right now*/
    IWDG_KR = IWDG_KR_TRG;
    /*turn off LSI*/
    RCC_CSR &= ~RCC_CSR_LSION;
    while ((RCC_CSR & RCC_CSR_LSIRDY) == RCC_CSR_LSIRDY) ;
}

/*** system speed related functions **********************************/

/*sets a given system speed, based on an 8 MHz xtal or using the 16 MHz
  internal oscillator as fallback.

  VCO_in = PLL_in / PLL_M; 2 <= PLL_M <= 63; 1 MHz <= VCO_in <= 2 MHz (best at 2 MHz)
  VCO_out = VCO_in * PLL_N; 50 <= PLL_N <= 432; 100 MHz <= VCO_out <= 432 MHz
  PLL_out = VCO_out / PLL_P; PLL_out <= 168 MHz; PLL_P = 2, 4, 6, 8
  USB_RND_SDIO_freq = VCO / PLL_Q; 2 <= PLL_Q <=15*/
static uint32_t Drv_RCC_PLL_Set(uint32_t new_speed,
                                uint32_t pll_m_hse,
                                uint32_t pll_m_hsi,
                                uint32_t pll_n,
                                uint32_t pll_p,
                                uint32_t pll_q,
                                uint32_t flash_config,
                                uint32_t abp_dividers)
{
    uint32_t reg32, waitstates, cnt = 0, pll_m, clk_src = RCC_PLLCFGR_PLLSRC; /*hse per default*/

    /*get both high speed oscillators going*/
    RCC_CR |= RCC_CR_HSEON | RCC_CR_HSION;
    __DSB();
    /*wait for internal oscillator*/
    while ((RCC_CR & RCC_CR_HSIRDY) != RCC_CR_HSIRDY) ;

    /*reset the whole control register*/
    RCC_CFGR = RCC_CFGR_SW_HSI;
    __DSB();
    /*and wait for the system to run on HSI*/
    while ((RCC_CFGR & RCC_CFGR_SWS_ALL) != RCC_CFGR_SWS_HSI) ;

    /*no waitstates and cache for now*/
    FLASH_ACR = FLASH_ACR_18;
    __ISB(); __DSB();
    while ((FLASH_ACR & FLASH_ACR_WS) != (FLASH_ACR_18 & FLASH_ACR_WS)) ;

    /*reset data and instruction cache*/
    FLASH_ACR |=  FLASH_ACR_CACHERST;
    __ISB(); __DSB();
    FLASH_ACR &= ~FLASH_ACR_CACHERST;
    __ISB(); __DSB();

    /*switch off PLL*/
    RCC_CR &= ~RCC_CR_PLLON;
    __DSB();
    /*wait for PLL to go down*/
    while ((RCC_CR & RCC_CR_PLLRDY) == RCC_CR_PLLRDY) ;

    /*wait for external oscillator*/
    while (((RCC_CR & RCC_CR_HSERDY) != RCC_CR_HSERDY) && (cnt < HSE_MAX_CNT))
        cnt++;

    if (cnt >= HSE_MAX_CNT) /*HSE doesn't work. HW damaged?*/
    {
        /*ok, so run on the HSI. It's not as exact as the quartz,
        but it should still work.*/
        RCC_CR &= ~RCC_CR_HSEON; /*switch off HSE*/
        clk_src = 0;
        pll_m = pll_m_hsi;
    } else
        pll_m = pll_m_hse; /*OK: divider for external oscillator*/

    /*power the interface clock*/
    RCC_APB1ENR |= RCC_APB1ENR_PWREN;
    __DSB(); /*see STM32F40x errata sheet - use DSB after RCC enable*/

    /*in lower speed mode, the voltage scale can be reduced.*/
    if (new_speed <= SPEED_144MHZ)
        PWR_CR &= ~PWR_CR_VOS;
    else
        PWR_CR |= PWR_CR_VOS;
    __DSB();
    /*and wait for the regulator to be ready*/
    while ((PWR_CSR & PWR_CSR_VOSRDY) != PWR_CSR_VOSRDY) ;

    /*see the Reference Manual how to set up the PLL configuration register*/
    reg32  = pll_m;
    reg32 |= pll_n << 6;
    pll_p /= 2UL;
    pll_p--;
    reg32 |= pll_p << 16;
    reg32 |= pll_q << 24;
    reg32 |= clk_src; /*select clock source: HSE/HSI*/

    /*write the PLL settings*/
    RCC_PLLCFGR = reg32;

    /*turn on PLL and wait for ready*/
    RCC_CR |= RCC_CR_PLLON;
    __DSB();
    while ((RCC_CR & RCC_CR_PLLRDY) != RCC_CR_PLLRDY) ;

    /*set the dividers*/
    RCC_CFGR |= abp_dividers;

    /*set possible waitstates before ramping up the speed*/
    FLASH_ACR  = flash_config;
    __ISB();
    waitstates = flash_config & FLASH_ACR_WS;
    while ((FLASH_ACR & FLASH_ACR_WS) != waitstates) ;

    /*run the system on PLL*/
    RCC_CFGR |= RCC_CFGR_SW_PLL;
    __DSB();

    /*wait for the PLL to source the system*/
    while ((RCC_CFGR & RCC_CFGR_SWS_ALL) != RCC_CFGR_SWS_PLL) ;

    if (clk_src) /*HSE works*/
    {
        /*switch off HSI to save energy*/
        RCC_CR &= ~RCC_CR_HSION;
        return(0);
    }

    return(0xDEADBEEFUL); /*error flag*/
}

/*run directly on the 8 MHz xtal HSE, or on 16 MHz HSI if the HSE fails.
  only used in battery fail shutdown because this clock mode will never
  work with USB as there is no 48 MHz PLL output after the PLL-Q divider,
  and that's because the whole PLL is switched off anyway.*/
static uint32_t Drv_RCC_Run_HSE(void)
{
    uint32_t cnt = 0;

    /*get both high speed oscillators going*/
    RCC_CR |= RCC_CR_HSEON | RCC_CR_HSION;
    __DSB();
    /*wait for internal oscillator*/
    while ((RCC_CR & RCC_CR_HSIRDY) != RCC_CR_HSIRDY) ;

    /*reset the whole control register*/
    RCC_CFGR = RCC_CFGR_SW_HSI;
    __DSB();
    /*and wait for the system to run on HSI*/
    while ((RCC_CFGR & RCC_CFGR_SWS_ALL) != RCC_CFGR_SWS_HSI) ;

    /*no waitstate and cache for low speed mode*/
    FLASH_ACR = FLASH_ACR_18;
    __ISB(); __DSB();
    while ((FLASH_ACR & FLASH_ACR_WS) != (FLASH_ACR_18 & FLASH_ACR_WS)) ;

    /*reset data and instruction cache*/
    FLASH_ACR |=  FLASH_ACR_CACHERST;
    __ISB(); __DSB();
    FLASH_ACR &= ~FLASH_ACR_CACHERST;
    __ISB(); __DSB();

    /*switch off PLL*/
    RCC_CR &= ~RCC_CR_PLLON;
    __DSB();
    /*wait for PLL to go down*/
    while ((RCC_CR & RCC_CR_PLLRDY) == RCC_CR_PLLRDY) ;

    /*power the interface clock*/
    RCC_APB1ENR |= RCC_APB1ENR_PWREN;
    __DSB(); /*see STM32F40x errata sheet - use DSB after RCC enable*/

    /*in low speed mode, the voltage scale can be reduced.*/
    PWR_CR &= ~PWR_CR_VOS;
    __DSB();

    /*and wait for the regulator to be ready*/
    while ((PWR_CSR & PWR_CSR_VOSRDY) != PWR_CSR_VOSRDY) ;

    /*set the dividers: no divider in low speed mode*/
    RCC_CFGR |= RCC_CFGR_APB_NO_DIV;

    /*wait for external oscillator*/
    while (((RCC_CR & RCC_CR_HSERDY) != RCC_CR_HSERDY) && (cnt < HSE_MAX_CNT))
        cnt++;

    if (cnt >= HSE_MAX_CNT) /*HSE doesn't work. HW damaged?*/
    {
        /*ok, so keep running run on the HSI. It's not as
        exact as the quartz, and it draws more current
        (16 MHz vs. 8 MHz), but it should still work.*/
        RCC_CR &= ~RCC_CR_HSEON; /*switch off HSE*/
        return(0xDEADBEEFUL); /*error flag*/
    } else
    {
        /*run the system on HSE*/
        RCC_CFGR |= RCC_CFGR_SW_HSE;
        __DSB();

        /*wait for the HSE to source the system*/
        while ((RCC_CFGR & RCC_CFGR_SWS_ALL) != RCC_CFGR_SWS_HSE) ;

        /*switch off HSI to save energy*/
        RCC_CR &= ~RCC_CR_HSION;
        return(0);
    }
}

/*selects the system speed.
  note that clock speeds above 168 MHz (overclocking) is not guaranteed
  to work.
  although USB is currently not used, the parameters are chosen so that
  all settings with 8 MHz xtal (or 16 MHz internal oscillator as backup)
  will result in exactly 48 MHz for the USB clock, which is what it needs.
  See also "RCC PLL configuration register (RCC_PLLCFGR)" in RM0090
  Reference manual.*/
uint32_t Drv_Set_Sys_Speed_Type(enum E_SYS_SPEED_MHZ sys_speed_type, uint32_t *new_sys_freq)
{
    uint32_t ret;

    switch (sys_speed_type)
    {
    default:
    case SYS_SPEED_18MHZ:
        ret = Drv_RCC_PLL_Set(SPEED_18MHZ, PLL_M_HSE, PLL_M_HSI, PLL_N_18, PLL_P_18, PLL_Q_18, FLASH_ACR_18, RCC_CFGR_APB_NO_DIV);
        /*APB2 USART speed: 18 MHz*/
        *new_sys_freq = SPEED_18MHZ;
        break;
    case SYS_SPEED_42MHZ:
        ret = Drv_RCC_PLL_Set(SPEED_42MHZ, PLL_M_HSE, PLL_M_HSI, PLL_N_42, PLL_P_42, PLL_Q_42, FLASH_ACR_42, RCC_CFGR_APB_NO_DIV);
        /*APB2 USART speed: 42 MHz*/
        *new_sys_freq = SPEED_42MHZ;
        break;
    case SYS_SPEED_84MHZ:
        ret = Drv_RCC_PLL_Set(SPEED_84MHZ, PLL_M_HSE, PLL_M_HSI, PLL_N_84, PLL_P_84, PLL_Q_84, FLASH_ACR_84, RCC_CFGR_APB1_DIV2);
        /*APB2 USART speed: 84 MHz*/
        *new_sys_freq = SPEED_84MHZ;
        break;
    case SYS_SPEED_120MHZ:
        ret = Drv_RCC_PLL_Set(SPEED_120MHZ, PLL_M_HSE, PLL_M_HSI, PLL_N_120, PLL_P_120, PLL_Q_120, FLASH_ACR_120, RCC_CFGR_APB1_DIV4 | RCC_CFGR_APB2_DIV2);
        /*APB2 USART speed: 60 MHz*/
        *new_sys_freq = SPEED_120MHZ;
        break;
    case SYS_SPEED_168MHZ:
        ret = Drv_RCC_PLL_Set(SPEED_168MHZ, PLL_M_HSE, PLL_M_HSI, PLL_N_168, PLL_P_168, PLL_Q_168, FLASH_ACR_168, RCC_CFGR_APB1_DIV4 | RCC_CFGR_APB2_DIV2);
        /*APB2 USART speed: 84 MHz*/
        *new_sys_freq = SPEED_168MHZ;
        break;
    case SYS_SPEED_216MHZ:
        ret = Drv_RCC_PLL_Set(SPEED_216MHZ, PLL_M_HSE, PLL_M_HSI, PLL_N_216, PLL_P_216, PLL_Q_216, FLASH_ACR_216, RCC_CFGR_APB1_DIV8 | RCC_CFGR_APB2_DIV4);
        /*APB2 USART speed: 54 MHz*/
        *new_sys_freq = SPEED_216MHZ;
        break;
    case SYS_SPEED_240MHZ:
        ret = Drv_RCC_PLL_Set(SPEED_240MHZ, PLL_M_HSE, PLL_M_HSI, PLL_N_240, PLL_P_240, PLL_Q_240, FLASH_ACR_240, RCC_CFGR_APB1_DIV8 | RCC_CFGR_APB2_DIV4);
        /*APB2 USART speed: 60 MHz*/
        *new_sys_freq = SPEED_240MHZ;
        break;
    case SYS_SPEED_OSC:
        /*this is only used for shutdown with low batteries
          and doesn't work with USART or USB.*/
        ret = Drv_RCC_Run_HSE();
        if (ret == 0)
            *new_sys_freq = SPEED_OSC_EXT; /*external oscillator*/
        else
            *new_sys_freq = SPEED_OSC_INT; /*internal oscillator*/
        break;
    }
    return(ret);
}

/*** SYSTICK related functions ***************************************/

void Drv_DWT_Setup(void)
{
    /*activate DWT - for the HW microseconds wait routine*/
    CDBG_DEMCR |= CDBG_DEMCR_TRCENA;
    DWT_CYCCNT  = 0;
    DWT_CTRL   |= DWT_CTRL_CYCCNTENA;
}

void Drv_DWT_Stop(void)
{
    /*deactivate DWT*/
    DWT_CYCCNT  = 0;
    DWT_CTRL   &= ~DWT_CTRL_CYCCNTENA;
    CDBG_DEMCR &= ~CDBG_DEMCR_TRCENA;
}

/*stop the SysTick timer and disable the interrupt.*/
void Drv_Stop_Systick(void)
{
    SYST_CSR = SYST_CSR_SET_STOP;
    __DSB();
    SCB_ICSR = SCB_ICSR_PENDSTCLR; /*clear pending systick interrupt*/
}

/*configure and start the SysTick timer.
  note: the prescaling is by /8 (see SYST_CSR_SET_START definition) because
  this is just as exact as not prescaling, but might save some energy.*/
void Drv_Start_Systick(uint32_t cpu_freq, uint32_t tick_freq)
{
    uint32_t reload_value, systick_prio_reg;

    SYST_CSR = SYST_CSR_SET_STOP; /*stop the systick interrupt*/
    __DSB();
    SCB_ICSR = SCB_ICSR_PENDSTCLR; /*clear pending systick interrupt*/

    reload_value = (cpu_freq / 8UL) / tick_freq; /*calculate the counter reload value*/
    reload_value--;
    reload_value &= 0x00FFFFFFUL; /*maximum is 24 bits*/
    SYST_RVR = reload_value;

    /*set the systick priority (less important than fault handlers)*/
    systick_prio_reg  = SCB_SHPR3;
    systick_prio_reg &= ~SYSTICK_PRIO_MASK;
    systick_prio_reg |= SYSTICK_PRIO;
    SCB_SHPR3 = systick_prio_reg;

    SYST_CVR = 0;

    SYST_CSR = SYST_CSR_SET_START; /*and get it going*/
}
