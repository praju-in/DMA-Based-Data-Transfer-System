/**
 * @file    dma_adc.c
 * @brief   ADC + DMA driver — register-level implementation.
 * @target  STM32F407VG (Cortex-M4)
 *
 * Peripherals used:
 *   - TIM3   : ADC trigger @ SAMPLING_FREQ_HZ via TRGO
 *   - ADC1   : Channel 0 (PA0), 12-bit, externally triggered
 *   - DMA2   : Stream 0, Channel 0, circular, HT+TC interrupts
 *
 * The ISR sets volatile flags only; processing is deferred to main().
 */

#include "dma_adc.h"
#include "stm32f4xx.h"          /* CMSIS device header                 */

/* ------------------------------------------------------------------ */
/*  Global buffer — MUST reside in standard SRAM (not CCM @ 0x10000000)*/
/* ------------------------------------------------------------------ */
volatile uint16_t adc_buffer[BUFFER_SIZE];

/* ------------------------------------------------------------------ */
/*  ISR flags                                                          */
/* ------------------------------------------------------------------ */
volatile uint8_t process_first_half  = 0;
volatile uint8_t process_second_half = 0;

/* ==================================================================
 *  PRIVATE: GPIO initialisation (PA0 = analog input)
 * ================================================================== */
static void GPIO_Init_ADC_Pin(void)
{
    /* Enable GPIOA clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA0 → Analog mode (MODER bits [1:0] = 0b11) */
    GPIOA->MODER |= (3U << (ADC_CHANNEL * 2));
}

/* ==================================================================
 *  PRIVATE: TIM3 configuration — TRGO on update event
 * ================================================================== */
static void TIM3_Init(void)
{
    /* Enable TIM3 clock (APB1) */
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    TIM3->PSC  = TIM_PRESCALER;             /* Prescaler               */
    TIM3->ARR  = TIM_PERIOD;                /* Auto-reload value       */
    TIM3->CNT  = 0;

    /* Master Mode Selection: Update event → TRGO (MMS = 010) */
    TIM3->CR2 &= ~TIM_CR2_MMS;
    TIM3->CR2 |=  TIM_CR2_MMS_1;           /* MMS = 010               */

    /* Do NOT start the timer here — DMA_ADC_Start() will do it.       */
}

/* ==================================================================
 *  PRIVATE: ADC1 configuration
 * ================================================================== */
static void ADC1_Init(void)
{
    /* Enable ADC1 clock (APB2) */
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    /* ---- Common ADC settings ------------------------------------ */
    ADC->CCR &= ~ADC_CCR_ADCPRE;           /* PCLK2 / 2 (default)     */

    /* ---- ADC1 control registers --------------------------------- */
    ADC1->CR1  = 0;                         /* Clear control reg 1     */
    ADC1->CR2  = 0;                         /* Clear control reg 2     */

    /* Resolution: 12-bit (RES = 00) — already 0 after clear          */

    /* External trigger: TIM3 TRGO
     * EXTSEL[3:0] = 0b1000  (Table in RM0090 §13.13.4)
     * EXTEN[1:0]  = 0b01    (trigger on rising edge)                 */
    ADC1->CR2 |= (8U  << ADC_CR2_EXTSEL_Pos);   /* EXTSEL = 1000      */
    ADC1->CR2 |= (1U  << ADC_CR2_EXTEN_Pos);    /* Rising edge        */

    /* Continuous mode: DISABLED (timer triggers each conversion)      */
    ADC1->CR2 &= ~ADC_CR2_CONT;

    /* Enable DMA request + DDS (DMA Disable Selection = 1 for circ)  */
    ADC1->CR2 |= ADC_CR2_DMA | ADC_CR2_DDS;

    /* Sequence length = 1 conversion (L[3:0] = 0000)                 */
    ADC1->SQR1 &= ~ADC_SQR1_L;

    /* First conversion in sequence = Channel 0 (SQ1[4:0] = 0)        */
    ADC1->SQR3 &= ~ADC_SQR3_SQ1;
    ADC1->SQR3 |= (ADC_CHANNEL << ADC_SQR3_SQ1_Pos);

    /* Sampling time for Channel 0: 84 cycles (SMP0 = 100)            */
    ADC1->SMPR2 &= ~ADC_SMPR2_SMP0;
    ADC1->SMPR2 |= (4U << ADC_SMPR2_SMP0_Pos);  /* 100 = 84 cycles   */

    /* Power-on the ADC */
    ADC1->CR2 |= ADC_CR2_ADON;
}

/* ==================================================================
 *  PRIVATE: DMA2 Stream 0 configuration (ADC1 → memory, circular)
 * ================================================================== */
static void DMA2_Stream0_Init(void)
{
    /* Enable DMA2 clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

    /* Disable stream before configuring */
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    while (DMA2_Stream0->CR & DMA_SxCR_EN) { /* wait */ }

    /* Clear all interrupt flags for Stream 0 (LIFCR bits 0-5) */
    DMA2->LIFCR = 0x3FU;

    /* ---- Stream configuration ----------------------------------- */
    DMA2_Stream0->CR  = 0;                  /* Reset                   */

    /* Channel selection: Channel 0 (CHSEL = 000) — already 0         */

    /* Data direction: Peripheral → Memory (DIR = 00) — already 0     */

    /* Peripheral settings */
    DMA2_Stream0->CR |= DMA_SxCR_PSIZE_0;  /* Peripheral size: 16-bit */
    /* Peripheral increment: DISABLED (always read ADC1->DR)          */

    /* Memory settings */
    DMA2_Stream0->CR |= DMA_SxCR_MSIZE_0;  /* Memory size: 16-bit     */
    DMA2_Stream0->CR |= DMA_SxCR_MINC;     /* Memory increment: ON    */

    /* Circular mode */
    DMA2_Stream0->CR |= DMA_SxCR_CIRC;

    /* Interrupt enables: Half Transfer + Transfer Complete */
    DMA2_Stream0->CR |= DMA_SxCR_HTIE | DMA_SxCR_TCIE;

    /* Number of data items */
    DMA2_Stream0->NDTR = BUFFER_SIZE;

    /* Peripheral address: ADC1 data register */
    DMA2_Stream0->PAR  = (uint32_t)&ADC1->DR;

    /* Memory address: our SRAM buffer */
    DMA2_Stream0->M0AR = (uint32_t)adc_buffer;

    /* Enable DMA2 Stream 0 interrupt in NVIC */
    NVIC_SetPriority(DMA2_Stream0_IRQn, 1);
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

/* ==================================================================
 *  PUBLIC API
 * ================================================================== */

void DMA_ADC_Init(void)
{
    GPIO_Init_ADC_Pin();
    TIM3_Init();
    ADC1_Init();
    DMA2_Stream0_Init();
}

void DMA_ADC_Start(void)
{
    /* Enable DMA stream first */
    DMA2_Stream0->CR |= DMA_SxCR_EN;

    /* Start ADC conversion (SWSTART is harmless here — timer drives) */
    ADC1->CR2 |= ADC_CR2_SWSTART;

    /* Start the trigger timer last */
    TIM3->CR1 |= TIM_CR1_CEN;
}

void DMA_ADC_Stop(void)
{
    /* Stop timer */
    TIM3->CR1 &= ~TIM_CR1_CEN;

    /* Stop ADC */
    ADC1->CR2 &= ~ADC_CR2_ADON;

    /* Disable DMA stream */
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
}

/* ==================================================================
 *  DMA2 Stream 0 ISR
 *  CRITICAL: Only set flags here.  No heavy processing.
 * ================================================================== */
void DMA2_Stream0_IRQHandler(void)
{
    /* ---- Half-Transfer Complete --------------------------------- */
    if (DMA2->LISR & DMA_LISR_HTIF0) {
        DMA2->LIFCR = DMA_LIFCR_CHTIF0;    /* Clear HT flag           */
        process_first_half = 1;
    }

    /* ---- Transfer Complete -------------------------------------- */
    if (DMA2->LISR & DMA_LISR_TCIF0) {
        DMA2->LIFCR = DMA_LIFCR_CTCIF0;    /* Clear TC flag           */
        process_second_half = 1;
    }
}
