/**
 * @file    perf_measure.c
 * @brief   DWT-based cycle-counter utilities for performance profiling.
 * @target  Cortex-M3/M4/M7 with DWT.
 *
 * NOTE: On some targets the debugger must first unlock the DWT via
 * CoreDebug->DEMCR.  This is done in Perf_Init().
 */

#include "perf_measure.h"
#include "stm32f4xx.h"

/* ==================================================================
 *  PUBLIC API
 * ================================================================== */

void Perf_Init(void)
{
#if PERF_MEASURE_ENABLED
    /* Enable trace and debug block (required for DWT) */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* Reset and enable the cycle counter */
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
#endif
}

void Perf_StartMeasure(void)
{
#if PERF_MEASURE_ENABLED
    DWT->CYCCNT = 0;                        /* Reset counter           */
#endif
}

uint32_t Perf_StopMeasure(void)
{
#if PERF_MEASURE_ENABLED
    return DWT->CYCCNT;
#else
    return 0;
#endif
}

uint32_t Perf_CyclesToUs(uint32_t cycles)
{
    /* cycles / (SYSTEM_CLOCK_HZ / 1 000 000) */
    return cycles / (SYSTEM_CLOCK_HZ / 1000000UL);
}
