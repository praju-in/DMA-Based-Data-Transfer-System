/**
 * @file    dma_adc.h
 * @brief   ADC + DMA driver interface for continuous data acquisition.
 * @target  STM32F407VG (Cortex-M4)
 *
 * Provides initialisation routines and a start function that configures:
 *   - TIM3 as an ADC trigger at the desired sampling frequency.
 *   - ADC1 (channel 0, PA0) in scan/single-conversion mode triggered
 *     externally by TIM3 TRGO.
 *   - DMA2 Stream 0 in circular mode with Half-Transfer and
 *     Transfer-Complete interrupts.
 *
 * Data is written into a global buffer.  The ISR only sets volatile
 * flags; actual processing is deferred to the main loop.
 */

#ifndef DMA_ADC_H
#define DMA_ADC_H

#include <stdint.h>
#include "system_config.h"

/* ------------------------------------------------------------------ */
/*  Global buffer (placed in standard SRAM, NOT CCM)                   */
/* ------------------------------------------------------------------ */
extern volatile uint16_t adc_buffer[BUFFER_SIZE];

/* ------------------------------------------------------------------ */
/*  Flags set by ISR, consumed by main loop                            */
/* ------------------------------------------------------------------ */
extern volatile uint8_t process_first_half;
extern volatile uint8_t process_second_half;

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief  Initialise GPIO, Timer, ADC, and DMA peripherals.
 *         Does NOT start conversions — call DMA_ADC_Start() afterwards.
 */
void DMA_ADC_Init(void);

/**
 * @brief  Start the ADC+DMA acquisition loop.
 *         After this call, data flows automatically into adc_buffer[].
 */
void DMA_ADC_Start(void);

/**
 * @brief  Stop the ADC+DMA acquisition loop and disable interrupts.
 */
void DMA_ADC_Stop(void);

#endif /* DMA_ADC_H */
