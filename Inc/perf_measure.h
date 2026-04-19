/**
 * @file    perf_measure.h
 * @brief   Lightweight performance-measurement utilities using the
 *          Cortex-M DWT cycle counter (DWT_CYCCNT).
 * @target  Any Cortex-M3/M4/M7 with DWT unit.
 *
 * Usage:
 *   Perf_Init();           // call once at startup
 *   Perf_StartMeasure();   // before the section under test
 *   ...
 *   uint32_t cycles = Perf_StopMeasure();  // elapsed core cycles
 */

#ifndef PERF_MEASURE_H
#define PERF_MEASURE_H

#include <stdint.h>
#include "system_config.h"

/**
 * @brief  Enable the DWT cycle counter (requires debug/trace access).
 */
void Perf_Init(void);

/**
 * @brief  Reset and start the cycle counter.
 */
void Perf_StartMeasure(void);

/**
 * @brief  Read the elapsed cycle count since Perf_StartMeasure().
 * @return Number of core clock cycles elapsed.
 */
uint32_t Perf_StopMeasure(void);

/**
 * @brief  Convert a cycle count to microseconds using SYSTEM_CLOCK_HZ.
 * @param  cycles  Value returned by Perf_StopMeasure().
 * @return Elapsed time in microseconds.
 */
uint32_t Perf_CyclesToUs(uint32_t cycles);

#endif /* PERF_MEASURE_H */
