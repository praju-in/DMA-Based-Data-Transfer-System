/**
 * @file    main.c
 * @brief   Entry point — Cortex-M DMA-Based Data Transfer System.
 * @target  STM32F407VG (Cortex-M4)
 *
 * System overview:
 *   TIM3 TRGO ──► ADC1 conversion ──► DMA2 Stream0 ──► adc_buffer[]
 *                                                          │
 *                    main-loop processes half-buffers ◄─────┘
 *                            │
 *                    USART2 DMA TX (optional output)
 *
 * The main loop is intentionally kept simple:
 *   1. Check ISR flags
 *   2. Process whichever half-buffer is ready
 *   3. Optionally transmit results via UART
 *   4. Measure processing time with DWT
 */

#include "stm32f4xx.h"
#include "system_config.h"
#include "dma_adc.h"
#include "uart_output.h"
#include "perf_measure.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  LED helpers (STM32F4-Discovery: PD12–PD15)                         */
/* ------------------------------------------------------------------ */
static void LED_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;

    /* PD12-PD15 → General Purpose Output */
    GPIOD->MODER &= ~( (3U << (LED_GREEN_PIN  * 2))
                      | (3U << (LED_ORANGE_PIN * 2))
                      | (3U << (LED_RED_PIN    * 2))
                      | (3U << (LED_BLUE_PIN   * 2)) );

    GPIOD->MODER |=  ( (1U << (LED_GREEN_PIN  * 2))
                      | (1U << (LED_ORANGE_PIN * 2))
                      | (1U << (LED_RED_PIN    * 2))
                      | (1U << (LED_BLUE_PIN   * 2)) );
}

static inline void LED_On(uint8_t pin)  { GPIOD->BSRR = (1U << pin); }
static inline void LED_Off(uint8_t pin) { GPIOD->BSRR = (1U << (pin + 16)); }
static inline void LED_Toggle(uint8_t pin)
{
    GPIOD->ODR ^= (1U << pin);
}

/* ------------------------------------------------------------------ */
/*  System Clock — configure for 168 MHz using HSE + PLL               */
/*  (Adjust to match your crystal; 8 MHz HSE is typical on Discovery)  */
/* ------------------------------------------------------------------ */
static void SystemClock_Config(void)
{
    /* Enable HSE */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) { /* wait */ }

    /* Configure Flash latency for 168 MHz (5 WS) */
    FLASH->ACR = FLASH_ACR_LATENCY_5WS
               | FLASH_ACR_PRFTEN
               | FLASH_ACR_ICEN
               | FLASH_ACR_DCEN;

    /* PLL configuration:
     *   PLL_M = 8   (HSE / 8 = 1 MHz VCO input)
     *   PLL_N = 336  (1 MHz × 336 = 336 MHz VCO)
     *   PLL_P = 2   (336 / 2 = 168 MHz SYSCLK)
     *   PLL_Q = 7   (336 / 7 = 48 MHz for USB — unused here)        */
    RCC->PLLCFGR = (8U   << RCC_PLLCFGR_PLLM_Pos)
                 | (336U << RCC_PLLCFGR_PLLN_Pos)
                 | (0U   << RCC_PLLCFGR_PLLP_Pos)  /* 00 = /2 */
                 | RCC_PLLCFGR_PLLSRC_HSE
                 | (7U   << RCC_PLLCFGR_PLLQ_Pos);

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY)) { /* wait */ }

    /* AHB = SYSCLK, APB1 = SYSCLK/4, APB2 = SYSCLK/2 */
    RCC->CFGR = RCC_CFGR_HPRE_DIV1
              | RCC_CFGR_PPRE1_DIV4
              | RCC_CFGR_PPRE2_DIV2;

    /* Switch system clock to PLL */
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { /* wait */ }

    SystemCoreClock = SYSTEM_CLOCK_HZ;
}

/* ------------------------------------------------------------------ */
/*  Data processing — called from the main loop (NOT from ISR)         */
/* ------------------------------------------------------------------ */

/** Processed result of the latest half-buffer. */
static uint32_t last_average = 0;
static uint32_t last_process_cycles = 0;

/**
 * @brief  Compute the arithmetic average of a half-buffer.
 *         This can be replaced with FIR filtering, peak detection, etc.
 * @param  buf    Pointer to the start of the half-buffer.
 * @param  count  Number of samples to process.
 */
static void process_buffer(volatile uint16_t *buf, uint32_t count)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        sum += buf[i];
    }
    last_average = sum / count;
}

/* ------------------------------------------------------------------ */
/*  UART formatting helper                                             */
/* ------------------------------------------------------------------ */
static char tx_msg[UART_TX_BUFFER_SIZE];

static void send_result_uart(void)
{
    if (UART_IsBusy())
        return;

    uint32_t mv = (last_average * ADC_VREF_MV) / ADC_MAX_VALUE;
    int len = snprintf(tx_msg, sizeof(tx_msg),
                       "AVG: %lu  (%lu mV)  [%lu us]\r\n",
                       last_average, mv,
                       Perf_CyclesToUs(last_process_cycles));
    if (len > 0) {
        UART_Transmit_DMA((const uint8_t *)tx_msg, (uint16_t)len);
    }
}

/* ================================================================== */
/*  MAIN                                                               */
/* ================================================================== */
int main(void)
{
    /* ---- Phase 1: System init ----------------------------------- */
    SystemClock_Config();
    LED_Init();
    Perf_Init();

    /* ---- Phase 2-4: Peripheral init ----------------------------- */
    DMA_ADC_Init();
    UART_Init();

    /* ---- Start acquisition -------------------------------------- */
    DMA_ADC_Start();
    LED_On(LED_GREEN_PIN);                  /* Indicate system running  */

    /* ---- Phase 5-8: Main processing loop ------------------------ */
    while (1) {
        /* --- First half-buffer ready ----------------------------- */
        if (process_first_half) {
            process_first_half = 0;

            LED_On(LED_ORANGE_PIN);         /* Profiling: mark start   */
            Perf_StartMeasure();

            process_buffer(&adc_buffer[0], HALF_BUFFER_SIZE);

            last_process_cycles = Perf_StopMeasure();
            LED_Off(LED_ORANGE_PIN);        /* Profiling: mark end     */

            send_result_uart();
        }

        /* --- Second half-buffer ready ---------------------------- */
        if (process_second_half) {
            process_second_half = 0;

            LED_On(LED_ORANGE_PIN);
            Perf_StartMeasure();

            process_buffer(&adc_buffer[HALF_BUFFER_SIZE], HALF_BUFFER_SIZE);

            last_process_cycles = Perf_StopMeasure();
            LED_Off(LED_ORANGE_PIN);

            send_result_uart();
        }

        /* --- Overrun detection ----------------------------------- */
        if (process_first_half && process_second_half) {
            /* Both flags set simultaneously → processing too slow.
             * Light the red LED as a visual error indicator.         */
            LED_On(LED_RED_PIN);
        }
    }
}
