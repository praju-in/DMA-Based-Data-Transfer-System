/**
 * @file    system_config.h
 * @brief   System-wide definitions for the DMA-based data transfer system.
 * @target  STM32F407VG (Cortex-M4)
 *
 * This header contains all tuneable parameters: buffer sizes, sampling
 * frequency, clock speeds, and pin assignments. Modify values here to
 * adapt the project to a different board or use-case.
 */

#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Clock configuration                                                */
/* ------------------------------------------------------------------ */
#define SYSTEM_CLOCK_HZ         168000000UL   /* 168 MHz (max for F407)  */
#define APB1_CLOCK_HZ           42000000UL    /* APB1 = HCLK / 4        */
#define APB2_CLOCK_HZ           84000000UL    /* APB2 = HCLK / 2        */

/* ------------------------------------------------------------------ */
/*  ADC configuration                                                  */
/* ------------------------------------------------------------------ */
#define ADC_RESOLUTION_BITS     12
#define ADC_MAX_VALUE           ((1U << ADC_RESOLUTION_BITS) - 1)  /* 4095 */
#define ADC_CHANNEL             0             /* PA0 — ADC1 Channel 0   */
#define ADC_SAMPLE_TIME_CYCLES  84            /* Sampling-time in clocks */
#define ADC_VREF_MV             3300          /* Reference voltage (mV) */

/* ------------------------------------------------------------------ */
/*  Sampling / Timer configuration                                     */
/* ------------------------------------------------------------------ */
#define SAMPLING_FREQ_HZ        10000U        /* 10 kHz sampling rate   */

/*  Timer prescaler and period are computed so that:
 *  Timer_freq  = APB1_TIMER_CLK / (TIM_PRESCALER + 1)
 *  Sample_rate = Timer_freq     / (TIM_PERIOD    + 1)
 *
 *  APB1 timers run at 2 × APB1_CLOCK = 84 MHz when APB1 prescaler > 1.
 *  With PSC = 83  → tick = 1 MHz
 *  With ARR = 99  → overflow = 10 kHz                                  */
#define TIM_PRESCALER           83
#define TIM_PERIOD              99

/* ------------------------------------------------------------------ */
/*  DMA buffer                                                         */
/* ------------------------------------------------------------------ */
#define BUFFER_SIZE             1024U         /* Total samples in ring  */
#define HALF_BUFFER_SIZE        (BUFFER_SIZE / 2)

/* ------------------------------------------------------------------ */
/*  UART configuration (optional output)                               */
/* ------------------------------------------------------------------ */
#define UART_BAUD_RATE          115200U
#define UART_TX_BUFFER_SIZE     256U

/* ------------------------------------------------------------------ */
/*  Performance measurement                                            */
/* ------------------------------------------------------------------ */
#define PERF_MEASURE_ENABLED    1             /* 1 = enable DWT timing  */

/* ------------------------------------------------------------------ */
/*  Pin assignments (active-low LED on many Discovery boards)          */
/* ------------------------------------------------------------------ */
#define LED_GREEN_PIN           12            /* PD12 on STM32F4-DISCO  */
#define LED_ORANGE_PIN          13            /* PD13                   */
#define LED_RED_PIN             14            /* PD14                   */
#define LED_BLUE_PIN            15            /* PD15                   */

#endif /* SYSTEM_CONFIG_H */
