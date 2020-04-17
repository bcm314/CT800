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

/*system speed selection*/
enum E_SYS_SPEED_MHZ
{
    SYS_SPEED_OSC,
    SYS_SPEED_18MHZ,
    SYS_SPEED_USER = SYS_SPEED_18MHZ,
    SYS_SPEED_42MHZ,
    SYS_SPEED_84MHZ,
    SYS_SPEED_120MHZ,
    SYS_SPEED_168MHZ,
    SYS_SPEED_216MHZ,
    SYS_SPEED_240MHZ
};

/*physical system frequencies*/

/*external oscillator on the board, may be changed in the BOM.
  speed in MHz must be an integer with 2 <= speed[MHz] <= 63.*/
#ifndef SPEED_OSC_EXT_MHZ
    #define SPEED_OSC_EXT_MHZ       8UL
#endif

#if ((SPEED_OSC_EXT_MHZ < 2) || (SPEED_OSC_EXT_MHZ > 63))
    #error "external oscillator must be between 2 and 63 MHz!"
#endif

BUILD_ASSERT((((int)(SPEED_OSC_EXT_MHZ)) == SPEED_OSC_EXT_MHZ),_build_assert_no_2,"external oscillator speed in MHz must be an integer!");

#define SPEED_OSC_EXT       (SPEED_OSC_EXT_MHZ * 1000000UL)

/*CPU internal oscillator, fixed at 16 MHz*/
#define SPEED_OSC_INT        16000000UL

#define SPEED_18MHZ          18000000UL
#define SPEED_42MHZ          42000000UL
#define SPEED_84MHZ          84000000UL
#define SPEED_120MHZ        120000000UL
#define SPEED_144MHZ        144000000UL
#define SPEED_168MHZ        168000000UL
#define SPEED_216MHZ        216000000UL
#define SPEED_240MHZ        240000000UL

uint32_t Drv_Periph_Enable(void);
void     Drv_Periph_Disable(void);
void     Drv_Periph_Shutdown(void);
void     Drv_PWR_STBY_Mode(void);

void     Drv_Lock_BKP(void);
void     Drv_Unlock_BKP(void);

void     Drv_IWDG_Setup_Watchdog(uint32_t wd_timeout_ms);
void     Drv_IWDG_Trigger_Watchdog(void);
void     Drv_IWDG_Disable_Watchdog(void);

uint32_t Drv_Set_Sys_Speed_Type(enum E_SYS_SPEED_MHZ sys_speed_type, uint32_t *new_sys_freq);

void     Drv_DWT_Setup(void);
void     Drv_DWT_Stop(void);
void     Drv_Stop_Systick(void);
void     Drv_Start_Systick(uint32_t cpu_freq, uint32_t tick_freq);

#if defined(STM32F405) || defined(STM32F407)

    /*the WD trigger can also be done via macro.*/
    #define Drv_IWDG_TRG_WD()       IWDG_KR = IWDG_KR_TRG

    /*set and clear GPIO bits*/
    #define Drv_SETRESET_BITS(GPIO_BSRR, BITMASK) GPIO_BSRR = (BITMASK)

    /*note on the exception handler priorities:

      technically, they have 8 bits, but only the upper 4 are implemented
      in F405. The lowest are 0, and write is ignored.

      Give the Systick a higher priority numerical value, i.e. a lower
      priority, than the fault handlers so that fault handlers can kick
      in even during Systick servicing. But the Systick should not have
      the lowest possible priority so that even lower prioritised things
      could be added later.

      All fault handlers have maximum priority, i.e. number 0, and once
      one of them is active, the system has to be rebooted anyway, so
      there is no need for other to override.*/

    /*systick priority*/
    #define SYSTICK_PRIO_MASK       (0xFFUL << 24)
    #define SYSTICK_PRIO            (11UL   << 28)

    /*fault priorities, not used (all 0)*/
    #define USAGEFAULT_PRIO_MASK    (0xFFUL << 16)
    #define BUSFAULT_PRIO_MASK      (0xFFUL << 8)
    #define MEMFAULT_PRIO_MASK      (0xFFUL << 0)

    #define PRIMASK_INT_DISABLED    (0x01UL << 0)

    #define FLASH_BASE_ADDR     0x08000000UL

    /*system control block*/
    #define SCB_BASE            0xE000ED00UL
    #define SCB_ICSR            (*((volatile uint32_t *)(SCB_BASE+0x04UL)))
    #define SCB_ICSR_PENDSTCLR  ((uint32_t)(1UL << 25))
    #define SCB_ICSR_PENDSTSET  ((uint32_t)(1UL << 26))
    #define SCB_VTOR            (*((volatile uint32_t *)(SCB_BASE+0x08UL)))
    #define SCB_SCR             (*((volatile uint32_t *)(SCB_BASE+0x10UL)))
    #define SCB_SCR_SLEEPDEEP   ((uint32_t)(1UL << 2))
    #define SCB_CCR             (*((volatile uint32_t *)(SCB_BASE+0x14UL)))
    #define SCB_CCR_STKALIGN    ((uint32_t)(1UL << 9))
    #define SCB_CCR_DIV0_TRP    ((uint32_t)(1UL << 4))
    #define SCB_CCR_UNALIGN_TRP ((uint32_t)(1UL << 3))
    #define SCB_SHPR1           (*((volatile uint32_t *)(SCB_BASE+0x18UL)))
    #define SCB_SHPR2           (*((volatile uint32_t *)(SCB_BASE+0x1CUL)))
    #define SCB_SHPR3           (*((volatile uint32_t *)(SCB_BASE+0x20UL)))
    #define SCB_CFSR            (*((volatile const uint32_t *)(SCB_BASE+0x28UL)))

    /*AHB1*/
    #define RCC_BASE            0x40023800UL
    #define RCC_CR              (*((volatile uint32_t *)(RCC_BASE + 0x00UL)))
    #define RCC_CR_PLLRDY       ((uint32_t)(1UL << 25))
    #define RCC_CR_PLLON        ((uint32_t)(1UL << 24))
    #define RCC_CR_HSERDY       ((uint32_t)(1UL << 17))
    #define RCC_CR_HSEON        ((uint32_t)(1UL << 16))
    #define RCC_CR_HSIRDY       ((uint32_t)(1UL << 1))
    #define RCC_CR_HSION        ((uint32_t)(1UL << 0))

    #define RCC_CSR             (*((volatile uint32_t *)(RCC_BASE + 0x74UL)))
    #define RCC_CSR_LPWRRSTF    ((uint32_t)(1UL << 31))
    #define RCC_CSR_WWDGRSTF    ((uint32_t)(1UL << 30))
    #define RCC_CSR_IWDGRSTF    ((uint32_t)(1UL << 29))
    #define RCC_CSR_SFTRSTF     ((uint32_t)(1UL << 28))
    #define RCC_CSR_PORRSTF     ((uint32_t)(1UL << 27))
    #define RCC_CSR_PINRSTF     ((uint32_t)(1UL << 26))
    #define RCC_CSR_BORRSTF     ((uint32_t)(1UL << 25))
    #define RCC_CSR_RMVF        ((uint32_t)(1UL << 24))
    #define RCC_CSR_LSIRDY      ((uint32_t)(1UL << 1))
    #define RCC_CSR_LSION       ((uint32_t)(1UL << 0))

    #define RCC_PLLCFGR         (*((volatile uint32_t *)(RCC_BASE + 0x04UL)))
    #define RCC_PLLCFGR_PLLSRC  ((uint32_t)(1UL << 22))
    #define RCC_CFGR            (*((volatile uint32_t *)(RCC_BASE + 0x08UL)))
    #define RCC_CFGR_APB2_DIV2  ((uint32_t)(4UL << 13))
    #define RCC_CFGR_APB2_DIV4  ((uint32_t)(5UL << 13))
    #define RCC_CFGR_APB1_DIV2  ((uint32_t)(4UL << 10))
    #define RCC_CFGR_APB1_DIV4  ((uint32_t)(5UL << 10))
    #define RCC_CFGR_APB1_DIV8  ((uint32_t)(6UL << 10))
    #define RCC_CFGR_APB_NO_DIV ((uint32_t)(0UL))
    #define RCC_CFGR_SW_HSI     ((uint32_t)(0UL << 0))
    #define RCC_CFGR_SW_HSE     ((uint32_t)(1UL << 0))
    #define RCC_CFGR_SW_PLL     ((uint32_t)(2UL << 0))
    #define RCC_CFGR_SWS_ALL    ((uint32_t)(3UL << 2))
    #define RCC_CFGR_SWS_HSI    ((uint32_t)(0UL << 2))
    #define RCC_CFGR_SWS_HSE    ((uint32_t)(1UL << 2))
    #define RCC_CFGR_SWS_PLL    ((uint32_t)(2UL << 2))

    #define RCC_AHB1ENR         (*((volatile uint32_t *)(RCC_BASE + 0x30UL)))
    #define RCC_AHB1ENR_CCMEN   ((uint32_t)(1UL << 20))
    #define RCC_AHB1ENR_BKPEN   ((uint32_t)(1UL << 18))
    #define RCC_AHB1ENR_GPCEN   ((uint32_t)(1UL << 2))
    #define RCC_AHB1ENR_GPBEN   ((uint32_t)(1UL << 1))
    #define RCC_AHB1ENR_GPAEN   ((uint32_t)(1UL << 0))
    #define RCC_AHB1RSTR        (*((volatile uint32_t *)(RCC_BASE + 0x10UL)))
    #define RCC_AHB1RSTR_GPARST ((uint32_t)(1UL << 0))
    #define RCC_AHB1RSTR_GPBRST ((uint32_t)(1UL << 1))
    #define RCC_AHB1RSTR_GPCRST ((uint32_t)(1UL << 2))

    #define RCC_APB1ENR         (*((volatile uint32_t *)(RCC_BASE + 0x40UL)))
    #define RCC_APB1ENR_DACEN   ((uint32_t)(1UL << 29))
    #define RCC_APB1ENR_PWREN   ((uint32_t)(1UL << 28))
    #define RCC_APB1RSTR        (*((volatile uint32_t *)(RCC_BASE + 0x20UL)))
    #define RCC_AHB1RSTR_PWRRST ((uint32_t)(1UL << 28))
    #define RCC_AHB1RSTR_DACRST ((uint32_t)(1UL << 29))

    #define RCC_APB2ENR         (*((volatile uint32_t *)(RCC_BASE + 0x44UL)))
    #define RCC_APB2ENR_ADC1EN  ((uint32_t)(1UL << 8))
    #define RCC_APB2ENR_ADC2EN  ((uint32_t)(1UL << 9))
    #define RCC_APB2ENR_ADC3EN  ((uint32_t)(1UL << 10))
    #define RCC_APB2RSTR        (*((volatile uint32_t *)(RCC_BASE + 0x24UL)))
    #define RCC_APB2RSTR_ADCRST ((uint32_t)(1UL << 8))

    /*the backup ram base address and size*/
    #define BKPSRAM_BASE        0x40024000UL
    #define BKPSRAM_SIZE        4096UL

    #define GPIOA_BASE          0x40020000UL
    #define GPIOA_MODER         (*((volatile uint32_t *)(GPIOA_BASE + 0x00UL)))
    #define GPIOA_OTYPER        (*((volatile uint32_t *)(GPIOA_BASE + 0x04UL)))
    #define GPIOA_OSPEEDR       (*((volatile uint32_t *)(GPIOA_BASE + 0x08UL)))
    #define GPIOA_PUPDR         (*((volatile uint32_t *)(GPIOA_BASE + 0x0CUL)))
    #define GPIOA_IDR           (*((volatile uint32_t *)(GPIOA_BASE + 0x10UL)))
    #define GPIOA_ODR           (*((volatile uint32_t *)(GPIOA_BASE + 0x14UL)))
    #define GPIOA_BSRR          (*((volatile uint32_t *)(GPIOA_BASE + 0x18UL)))

    #define GPIOB_BASE          0x40020400UL
    #define GPIOB_MODER         (*((volatile uint32_t *)(GPIOB_BASE + 0x00UL)))
    #define GPIOB_OTYPER        (*((volatile uint32_t *)(GPIOB_BASE + 0x04UL)))
    #define GPIOB_OSPEEDR       (*((volatile uint32_t *)(GPIOB_BASE + 0x08UL)))
    #define GPIOB_PUPDR         (*((volatile uint32_t *)(GPIOB_BASE + 0x0CUL)))
    #define GPIOB_IDR           (*((volatile uint32_t *)(GPIOB_BASE + 0x10UL)))
    #define GPIOB_ODR           (*((volatile uint32_t *)(GPIOB_BASE + 0x14UL)))
    #define GPIOB_BSRR          (*((volatile uint32_t *)(GPIOB_BASE + 0x18UL)))

    #define GPIOC_BASE          0x40020800UL
    #define GPIOC_MODER         (*((volatile uint32_t *)(GPIOC_BASE + 0x00UL)))
    #define GPIOC_OTYPER        (*((volatile uint32_t *)(GPIOC_BASE + 0x04UL)))
    #define GPIOC_OSPEEDR       (*((volatile uint32_t *)(GPIOC_BASE + 0x08UL)))
    #define GPIOC_PUPDR         (*((volatile uint32_t *)(GPIOC_BASE + 0x0CUL)))
    #define GPIOC_IDR           (*((volatile uint32_t *)(GPIOC_BASE + 0x10UL)))
    #define GPIOC_ODR           (*((volatile uint32_t *)(GPIOC_BASE + 0x14UL)))
    #define GPIOC_BSRR          (*((volatile uint32_t *)(GPIOC_BASE + 0x18UL)))

    #define GPIO_Pin_0          (1UL << 0)
    #define GPIO_Pin_1          (1UL << 1)
    #define GPIO_Pin_2          (1UL << 2)
    #define GPIO_Pin_3          (1UL << 3)
    #define GPIO_Pin_4          (1UL << 4)
    #define GPIO_Pin_5          (1UL << 5)
    #define GPIO_Pin_6          (1UL << 6)
    #define GPIO_Pin_7          (1UL << 7)
    #define GPIO_Pin_8          (1UL << 8)
    #define GPIO_Pin_9          (1UL << 9)
    #define GPIO_Pin_10         (1UL << 10)
    #define GPIO_Pin_11         (1UL << 11)
    #define GPIO_Pin_12         (1UL << 12)
    #define GPIO_Pin_13         (1UL << 13)
    #define GPIO_Pin_14         (1UL << 14)
    #define GPIO_Pin_15         (1UL << 15)

    #define FLASH_IF_BASE       0x40023C00UL
    #define FLASH_ACR           (*((volatile uint32_t *)(FLASH_IF_BASE + 0x00UL)))
    #define FLASH_ACR_WS        ((uint32_t) 7UL)
    #define FLASH_ACR_DCRST     (1UL << 12)
    #define FLASH_ACR_ICRST     (1UL << 11)
    #define FLASH_ACR_CACHERST  (FLASH_ACR_DCRST | FLASH_ACR_ICRST)
    #define FLASH_ACR_DCACHE    (1UL << 10)
    #define FLASH_ACR_ICACHE    (1UL << 9)
    #define FLASH_ACR_PRFTCH    (1UL << 8)
    #define FLASH_ACR_ALLCACHE  (FLASH_ACR_DCACHE | FLASH_ACR_ICACHE | FLASH_ACR_PRFTCH)
    #define FLASH_ACR_18        ((uint32_t)(/*no cache*/         0UL /*WS*/))
    #define FLASH_ACR_42        ((uint32_t)(FLASH_ACR_ALLCACHE | 1UL /*WS*/))
    #define FLASH_ACR_84        ((uint32_t)(FLASH_ACR_ALLCACHE | 2UL /*WS*/))
    #define FLASH_ACR_120       ((uint32_t)(FLASH_ACR_ALLCACHE | 3UL /*WS*/))
    #define FLASH_ACR_168       ((uint32_t)(FLASH_ACR_ALLCACHE | 5UL /*WS*/))
    /*200 MHz+ is overclocking - NO additional waitstates used here!*/
    #define FLASH_ACR_216       ((uint32_t)(FLASH_ACR_ALLCACHE | 5UL /*WS*/))
    #define FLASH_ACR_240       ((uint32_t)(FLASH_ACR_ALLCACHE | 5UL /*WS*/))

    /*APB1*/
    #define RTC_BASE            0x40002800UL
    #define RTC_BKP0R           (*((volatile uint32_t *)(RTC_BASE + 0x50UL)))
    #define RTC_BKP1R           (*((volatile uint32_t *)(RTC_BASE + 0x54UL)))
    #define RTC_BKP2R           (*((volatile uint32_t *)(RTC_BASE + 0x58UL)))
    #define RTC_BKP3R           (*((volatile uint32_t *)(RTC_BASE + 0x5CUL)))

    #define IWDG_BASE           0x40003000UL
    #define IWDG_KR             (*((volatile uint32_t *)(IWDG_BASE + 0x00UL)))
    #define IWDG_KR_TRG         ((uint32_t)(0xAAAAUL))
    #define IWDG_KR_CONF        ((uint32_t)(0x5555UL))
    #define IWDG_KR_START       ((uint32_t)(0xCCCCUL))
    #define IWDG_PR             (*((volatile uint32_t *)(IWDG_BASE + 0x04UL)))
    #define IWDG_RLR            (*((volatile uint32_t *)(IWDG_BASE + 0x08UL)))
    #define IWDG_SR             (*((volatile const uint32_t *)(IWDG_BASE + 0x0CUL)))
    #define IWDG_SR_RVU         ((uint32_t)(1UL << 1))
    #define IWDG_SR_PVU         ((uint32_t)(1UL << 0))


    #define PWR_BASE            0x40007000UL
    #define PWR_CR              (*((volatile uint32_t *)(PWR_BASE + 0x00UL)))
    #define PWR_CR_VOS          ((uint32_t)(1UL << 14))
    #define PWR_CR_DBP          ((uint32_t)(1UL << 8))
    #define PWR_CR_CWUF         ((uint32_t)(1UL << 2))
    #define PWR_CR_PDDS         ((uint32_t)(1UL << 1))

    #define PWR_CSR             (*((volatile uint32_t *)(PWR_BASE + 0x04UL)))
    #define PWR_CSR_VOSRDY      ((uint32_t)(1UL << 14))
    #define PWR_CSR_BRE         ((uint32_t)(1UL << 9))
    #define PWR_CSR_BRR         ((uint32_t)(1UL << 3))

    #define DAC_BASE            0x40007400UL
    #define DAC_CR              (*((volatile uint32_t *)(DAC_BASE + 0x00UL)))
    #define DAC_CR_BOFF1_DIS    ((uint32_t)(1UL << 1))
    #define DAC_CR_EN1          ((uint32_t)(1UL << 0))
    #define DAC_DHR12R1         (*((volatile uint32_t *)(DAC_BASE + 0x08UL)))
    #define DAC_DOR1            (*((volatile uint32_t *)(DAC_BASE + 0x2CUL)))

    /*APB2*/
    #define ADC_BASE            0x40012000UL
    #define ADC1_CR1            (*((volatile uint32_t *)(ADC_BASE + 0x04UL)))
    #define ADC1_CR2            (*((volatile uint32_t *)(ADC_BASE + 0x08UL)))
    #define ADC1_CR2_SWSTART    ((uint32_t)(1UL << 30))
    #define ADC1_CR2_ADON       ((uint32_t)(1UL << 0))
    #define ADC1_SMPR2          (*((volatile uint32_t *)(ADC_BASE + 0x10UL)))
    #define ADC1_SMPR2_CH3_MAX  ((uint32_t)(7UL << 9))
    #define ADC1_SQR1           (*((volatile uint32_t *)(ADC_BASE + 0x2CUL)))
    #define ADC1_SQR3           (*((volatile uint32_t *)(ADC_BASE + 0x34UL)))
    #define ADC1_SQR3_SQ1_CH3   ((uint32_t)(3UL << 0))
    #define ADC1_DR             (*((volatile const uint32_t *)(ADC_BASE + 0x4CUL)))
    #define ADC_CCR             (*((volatile uint32_t *)(ADC_BASE + 0x300CUL)))
    #define ADC_CCR_PREDIV8     ((uint32_t)(3UL << 16))
    #define ADC_CCR_DELAY       ((uint32_t)(15UL << 8))

    /*core debug*/
    #define DWT_BASE            0xE0001000UL
    #define DWT_CTRL            (*((volatile uint32_t *)(DWT_BASE + 0x00UL)))
    #define DWT_CTRL_CYCCNTENA  ((uint32_t)(1UL << 0))
    #define DWT_CTRL_NOEXTTRIG  ((uint32_t)(1UL << 26))
    #define DWT_CYCCNT          (*((volatile uint32_t *)(DWT_BASE + 0x04UL)))

    #define CDBG_BASE           0xE000EDF0UL
    #define CDBG_DEMCR          (*((volatile uint32_t *)(CDBG_BASE + 0x0CUL)))
    #define CDBG_DEMCR_TRCENA   ((uint32_t)(1UL << 24))

    /*systick timer*/
    #define SYST_BASE           0xE000E010UL
    #define SYST_CSR            (*((volatile uint32_t *)(SYST_BASE+0x00UL)))
    #define SYST_CSR_TICK_INT   ((uint32_t)(1UL << 1))
    #define SYST_CSR_ENABLE     ((uint32_t)(1UL << 0))
    #define SYST_CSR_SET_START  (SYST_CSR_TICK_INT | SYST_CSR_ENABLE) /*run with AHB/8 prescaler*/
    #define SYST_CSR_SET_STOP   (0x00UL)
    #define SYST_CSR_COUNT_FLAG ((uint32_t)(1UL << 16))
    #define SYST_RVR            (*((volatile uint32_t *)(SYST_BASE+0x04UL)))
    #define SYST_CVR            (*((volatile uint32_t *)(SYST_BASE+0x08UL)))

    /*note for all PLL value sets: if clocked with 8 MHz xtal,
      there will always be 48 MHz at the USB clock. Also if
      the xtal fails and the internal oscillator is used.*/

    /*PLL_M must always be so that clock source divided by PLL_M is 1 MHz.*/
    #define PLL_M_HSE          (SPEED_OSC_EXT / 1000000UL)
    #define PLL_M_HSI          (SPEED_OSC_INT / 1000000UL)

    /*PLL: the set of values for 18 MHz*/
    #define PLL_N_18            144UL
    #define PLL_P_18            8UL
    #define PLL_Q_18            3UL

    /*PLL: the set of values for 42 MHz*/
    #define PLL_N_42            336UL
    #define PLL_P_42            8UL
    #define PLL_Q_42            7UL

    /*PLL: the set of values for 84 MHz*/
    #define PLL_N_84            336UL
    #define PLL_P_84            4UL
    #define PLL_Q_84            7UL

    /*PLL: the set of values for 120 MHz*/
    #define PLL_N_120           240UL
    #define PLL_P_120           2UL
    #define PLL_Q_120           5UL

    /*PLL: the set of values for 168 MHz*/
    #define PLL_N_168           336UL
    #define PLL_P_168           2UL
    #define PLL_Q_168           7UL

    /*PLL: the set of values for 216 MHz (overclocked)*/
    #define PLL_N_216           432UL
    #define PLL_P_216           2UL
    #define PLL_Q_216           9UL

    /*PLL: the set of values for 240 MHz (overclocked)*/
    #define PLL_N_240           480UL
    #define PLL_P_240           2UL
    #define PLL_Q_240           10UL

    /*timeout counter for starting HSE while running on HSI*/
    #define HSE_MAX_CNT         0x666UL

    /*special note on port A and port B: the JTAG pins connect there,
    and after reset, the pins PA13, PA14, PA15 and PB3, PB4 are in
    alternate function mode. it is important to leave them there, or else
    the JTAG will be switched off.*/

    /*PORT A:
    light req:       PA1 (input with pull-up, active-low)
    light enable:    PA2 (output push/pull)
    ADC:             PA3, ADC3
    DAC:             PA4, DAC1*/
    #define PORT_A_MODER_RESET  0xA8000000UL
    #define PORT_A_PUPDR_RESET  0x64000000UL
    #define LIGHT_MODER_OUT     0x00000010UL
    #define LIGHT_ODR_OUT       0x00000004UL
    #define ANA_MODER_OUT       0x000003C0UL
    #define PORT_A_MODER_OUT    (LIGHT_MODER_OUT | ANA_MODER_OUT)
    #define LIGHT_PUPDR         0x00000004UL
    #define LIGHT_REQ           0x00000002UL

    /*PA5-7: input with pull-up, active-low*/
    #define PCB_ID_PUPDR        0x00005400UL
    #define PCB_ID_REQ          0x000000E0UL
    #define PCB_ID_OFFSET       5UL

    /*PA-8: ESP_EN for HW IDs 0 and 1*/
    #define ESP_EN_MODER        0x00010000UL
    #define ESP_EN_ODR_OUT      0x00000100UL

    /*PORT B:
    LEDs, display and buzzer on port B, pins 0/1, 5-14 and 15.
    e.g. LED: lowest pairs of mode each 01bin = 5dec*/
    #define PORT_B_MODER_RESET  0x00000280UL
    #define PORT_B_PUPDR_RESET  0x00000100UL
    #define LED_MODER_OUT       0x00000005UL
    #define LED_GR_ODR_OUT      0x00000001UL
    #define LED_RD_ODR_OUT      0x00000002UL
    #define DISP_MODER_OUT      0x15555400UL
    #define BUZZ_MODER_OUT      0x40000000UL
    #define BUZZ_ODR_OUT        0x00008000UL
    #define PORT_B_MODER_OUT    (LED_MODER_OUT | DISP_MODER_OUT | BUZZ_MODER_OUT)

    /*PORT C:
    keypad rows on port C, pin 0-3.
    row 1 is pin 1 of the keypad connector.
    keypad cols on port C, pin 5-8.
    col 1 is pin 5 of the keypad connector.*/
    #define PC_LED_MODER_OUT    0x01000000UL
    #define KEY_ROWS_MODER_OUT  (0x00000055UL | PC_LED_MODER_OUT)
    #define KEY_COLS_MODER_OUT  (0x00015400UL | PC_LED_MODER_OUT)
    #define KEY_ROWS_PUPDR      0x00000055UL
    #define KEY_COLS_PUPDR      0x00015400UL

    #define KEY_ROW_1           GPIO_Pin_0
    #define KEY_ROW_2           GPIO_Pin_1
    #define KEY_ROW_3           GPIO_Pin_2
    #define KEY_ROW_4           GPIO_Pin_3
    #define KEY_ROW_ALL         (KEY_ROW_1 | KEY_ROW_2 | KEY_ROW_3 | KEY_ROW_4)
    #define KEY_COL_1           GPIO_Pin_5
    #define KEY_COL_2           GPIO_Pin_6
    #define KEY_COL_3           GPIO_Pin_7
    #define KEY_COL_4           GPIO_Pin_8
    #define KEY_COL_ALL         (KEY_COL_1 | KEY_COL_2 | KEY_COL_3 | KEY_COL_4)

    /*set ROW 3 to GND, set row 1,2,4 as inputs.
    set col 1, 2, 3 as inputs.
    set col 4 as input with pull-up.*/
    #define KEY_COMP_MODER_OUT  0x00000010UL
    #define KEY_COMP_PUPDR_OUT  0x00015445UL

    /*set ROW 4 to GND, set row 1,2,3 as inputs.
    set col 1, 2, 3 as inputs.
    set col 4 as input with pull-up.*/
    #define KEY_EN_MODER_OUT    0x00000040UL
    #define KEY_EN_PUPDR_OUT    0x00015415UL

#else
    #error "no processor defined!"
#endif

/*instruction sync barrier*/
static inline __attribute__((always_inline)) void __ISB(void)
{__asm volatile("ISB" : : : "memory");}

/*data sync barrier*/
static inline __attribute__((always_inline)) void __DSB(void)
{__asm volatile("DSB" : : : "memory");}

/*data memory barrier*/
static inline __attribute__((always_inline)) void __DMB(void)
{__asm volatile("DMB" : : : "memory");}

/*disable all interrupts*/
static inline __attribute__((always_inline)) void __DISABLE_IRQ(void)
{__asm volatile("CPSID i" : : : "memory");}

/*enable all interrupts*/
static inline __attribute__((always_inline)) void __ENABLE_IRQ(void)
{__asm volatile("CPSIE i" : : : "memory");}

/*wait for interrupt*/
static inline __attribute__((always_inline)) void __WFI(void)
{ __asm volatile ("WFI" : : : "memory");}

/*CPU sleep
  note that WFI alone does not save that much energy, also the CPU
  frequency needs to be reduced.*/
static inline __attribute__((always_inline)) void Drv_CPU_Sleep(void)
{ __asm volatile ("DSB\n" "WFI" : : : "memory");}


/*Systick pause*/
static inline __attribute__((always_inline)) void Drv_Pause_Systick(void)
{
    /*disable interrupt for write-back, but keep the Systick counter running.
      the "has reached zero" COUNTFLAG gets already cleared in the Systick
      handler everytime it runs.*/
    SYST_CSR = SYST_CSR_ENABLE;
    __DSB();
}

/*Systick continue*/
static inline __attribute__((always_inline)) void Drv_Cont_Systick(void)
{
    __DSB();
    uint32_t reg32 = SYST_CSR;          /*reading also clears COUNTFLAG*/
    if (reg32 & SYST_CSR_COUNT_FLAG)    /*missed a systick?*/
        SCB_ICSR |= SCB_ICSR_PENDSTSET; /*schedule it manually*/
    SYST_CSR = SYST_CSR_SET_START;      /*get regular Systick going*/
}

/*Systick clear zero bit*/
static inline __attribute__((always_inline)) void Drv_Clear_Systick(void)
{SYST_CSR; /*reading clears COUNTFLAG*/}


/*Priority mask - bit 1 is relevant*/
static inline __attribute__((always_inline)) uint32_t __GET_PRIMASK(void)
{uint32_t res;__asm volatile ("MRS %0, PRIMASK" : "=r" (res));return(res & PRIMASK_INT_DISABLED);}

static inline __attribute__((always_inline)) void __SET_PRIMASK(uint32_t pri_mask)
{__asm volatile ("MSR PRIMASK, %0" : : "r" (pri_mask) : "memory");}

/*Base priority*/
static inline __attribute__((always_inline)) uint32_t __GET_BASEPRI(void)
{uint32_t res; __asm volatile ("MRS %0, BASEPRI_MAX" : "=r" (res));return(res);}

static inline __attribute__((always_inline)) void __SET_BASEPRI(uint32_t val)
{__asm volatile ("MSR BASEPRI, %0" : : "r" (val) : "memory");}
