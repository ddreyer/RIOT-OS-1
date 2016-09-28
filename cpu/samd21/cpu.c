/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_samd21
 * @{
 *
 * @file
 * @brief       Implementation of the CPU initialization
 *
 * @author      Thomas Eichinger <thomas.eichinger@fu-berlin.de>
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @}
 */

#include "cpu.h"
#include "periph_conf.h"
#include "periph/init.h"

/**
 * @brief   Configure clock sources and the cpu frequency
 */
static void clk_init(void)
{
    /* enable clocks for the power, sysctrl and gclk modules */
    PM->APBAMASK.reg = (PM_APBAMASK_PM | PM_APBAMASK_SYSCTRL |
                        PM_APBAMASK_GCLK);

    /* adjust NVM wait states, see table 42.30 (p. 1070) in the datasheet */
#if (CLOCK_CORECLOCK > 24000000)
    PM->APBBMASK.reg |= PM_APBBMASK_NVMCTRL;
    NVMCTRL->CTRLB.reg |= NVMCTRL_CTRLB_RWS(1);
    PM->APBBMASK.reg &= ~PM_APBBMASK_NVMCTRL;
#endif


	/* configure internal 8MHz oscillator to run without prescaler */
    /*SYSCTRL->OSC8M.bit.PRESC = 0;
    SYSCTRL->OSC8M.bit.ONDEMAND = 0;
    SYSCTRL->OSC8M.bit.RUNSTDBY = 0;
    SYSCTRL->OSC8M.bit.ENABLE = 0;*/
    //while (!(SYSCTRL->PCLKSR.reg & SYSCTRL_PCLKSR_OSC8MRDY)) {}

    /* hskim: Main clock setting for hamilton
	          Step 1) OSCULP32K oscilator feeds Clock generator 1
              Step 2) Clock generator 1 feeds DFLL48M
              Step 3) DFLL48M feeds Clock generator 0 which is the main clock
       Note: OSCULP32K consumes 115nA and DFLL48M consumes 420uA */
#if CLOCK_USE_FLL
    /* reset the GCLK module so it is in a known state */
    GCLK->CTRL.reg = GCLK_CTRL_SWRST;
    while (GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY) {}

    /* hskim: 1) Setup OSCULP32K to feed Clock generator 1 with 32.768kHz*/
    GCLK->GENDIV.reg  = (GCLK_GENDIV_ID(1)   | GCLK_GENDIV_DIV(0));
    GCLK->GENCTRL.reg = (GCLK_GENCTRL_ID(1)  | GCLK_GENCTRL_GENEN |
						 GCLK_GENCTRL_SRC_OSCULP32K);

	/* hskim: 2) Setup Clock generator 1 to feed DFLL48M with 32.768kHz */
    GCLK->CLKCTRL.reg = (GCLK_CLKCTRL_GEN(1) | GCLK_CLKCTRL_CLKEN |
						 GCLK_CLKCTRL_ID(GCLK_CLKCTRL_ID_DFLL48_Val));
                         //GCLK_CLKCTRL_ID(GCLK_CLKCTRL_ID_GCLK_DPLL_Val) |
    while (GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY) {}

	/* hskim: 3) Enable DFLL48M */
	SYSCTRL->DFLLCTRL.bit.ONDEMAND = 0; // Sleep in STANDBY mode
	SYSCTRL->DFLLCTRL.bit.RUNSTDBY = 0; // Sleep in STANDBY mode
	/*SYSCTRL->DFLLVAL.bit.COARSE = ((*((uint32_t*)(SYSCTRL_FUSES_DFLL48M_COARSE_CAL_ADDR)) >>	 			    						SYSCTRL_FUSES_DFLL48M_COARSE_CAL_Pos) &
									0x3Fu);
	SYSCTRL->DFLLVAL.bit.FINE = 512;*/
	SYSCTRL->DFLLCTRL.bit.MODE   = 1;     // Closed loop mode
	SYSCTRL->DFLLCTRL.bit.QLDIS  = 0;     // Quick lock is enabled
	SYSCTRL->DFLLCTRL.bit.CCDIS  = 0;	  // Chill cycle is enabled
	SYSCTRL->DFLLCTRL.bit.LLAW   = 0;     // Locks will not be lost after waking up from sleep modes
	SYSCTRL->DFLLMUL.bit.CSTEP   = 0x1f/4;
	SYSCTRL->DFLLMUL.bit.FSTEP   = 0xff/4;
	SYSCTRL->DFLLMUL.bit.MUL     = CLOCK_CORECLOCK/32768U;
	SYSCTRL->DFLLCTRL.bit.ENABLE = 1;     // Enable DFLL
    while (!(SYSCTRL->PCLKSR.reg & SYSCTRL_PCLKSR_DFLLRDY)) {} // This can be problematic

    /*SYSCTRL->DPLLRATIO.reg = (SYSCTRL_DPLLRATIO_LDR(CLOCK_PLL_MUL));
    SYSCTRL->DPLLCTRLB.reg = (SYSCTRL_DPLLCTRLB_REFCLK_GCLK);
    SYSCTRL->DPLLCTRLA.reg = (SYSCTRL_DPLLCTRLA_ENABLE);
    while(!(SYSCTRL->DPLLSTATUS.reg &
           (SYSCTRL_DPLLSTATUS_CLKRDY | SYSCTRL_DPLLSTATUS_LOCK))) {}*/

    /* hskim: 4) Setup DFLL48M to feed Clock generator 0 (CPU core clock) */
    GCLK->GENDIV.reg  = (GCLK_GENDIV_ID(0)  | GCLK_GENDIV_DIV(0)); //CLOCK_PLL_DIV) |
    GCLK->GENCTRL.reg = (GCLK_GENCTRL_ID(0) | GCLK_GENCTRL_GENEN |
						 GCLK_GENCTRL_SRC_DFLL48M);
                         //GCLK_GENCTRL_SRC_FDPLL96M |
#else
	/* do not use DFLL48M, use internal 32kHz oscillator directly */
    GCLK->GENDIV.reg  = (GCLK_GENDIV_ID(0)  | GCLK_GENDIV_DIV(0)); //CLOCK_DIV) |
    GCLK->GENCTRL.reg = (GCLK_GENCTRL_ID(0) | GCLK_GENCTRL_GENEN |
                         GCLK_GENCTRL_SRC_OSCULP32K);
#endif
    /* make sure we synchronize clock generator 0 before we go on */
    while (GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY) {}


#if TIMER_RTT_EN
    // hskim: Setup Clock generator 2 with divider 1 (32.768kHz)
    GCLK->GENDIV.reg  = (GCLK_GENDIV_ID(2)  | GCLK_GENDIV_DIV(0));
    GCLK->GENCTRL.reg = (GCLK_GENCTRL_ID(2) | GCLK_GENCTRL_GENEN |
#if RTT_RUNSTDBY
                         GCLK_GENCTRL_RUNSTDBY |
#endif
                         GCLK_GENCTRL_SRC_OSCULP32K);
    while (GCLK->STATUS.bit.SYNCBUSY) {}
#endif

	/* hskim: We don't use OSC8M due to its high power consuption (64uA)
	   Caution: Given that OSC8M was originally the source of Clock generator 0,
			    we can turn it off "AFTER" setting up another oscilator to feed Clock generator 0.
                Otherwise, hamilton will stop. */
	SYSCTRL->OSC8M.bit.ENABLE = 0;
    while ((SYSCTRL->PCLKSR.reg & SYSCTRL_PCLKSR_OSC8MRDY)) {}

	/* redirect all peripherals to a disabled clock generator (7) by default */
    for (int i = 0x3; i <= 0x22; i++) {
        GCLK->CLKCTRL.reg = ( GCLK_CLKCTRL_ID(i) | GCLK_CLKCTRL_GEN_GCLK7 );
        while (GCLK->STATUS.bit.SYNCBUSY) {}
    }

}

void cpu_init(void)
{
    /* disable the watchdog timer */
    WDT->CTRL.bit.ENABLE = 0;
    /* initialize the Cortex-M core */
    cortexm_init();
    /* Initialise clock sources and generic clocks */
    clk_init();
    /* trigger static peripheral initialization */
    periph_init();
}
