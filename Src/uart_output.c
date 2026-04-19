/**
 * @file    uart_output.c
 * @brief   Non-blocking UART transmit driver using DMA.
 * @target  STM32F407VG — USART2 (PA2 = TX)
 *
 * Uses DMA1 Stream 6, Channel 4 for USART2_TX.
 * Transmissions are fire-and-forget; if a transfer is already in
 * progress the new request is silently dropped.
 */

#include "uart_output.h"
#include "stm32f4xx.h"

/* ------------------------------------------------------------------ */
/*  Internal state                                                     */
/* ------------------------------------------------------------------ */
static volatile uint8_t uart_tx_busy = 0;

/* ==================================================================
 *  PRIVATE: GPIO PA2 → USART2 TX (AF7)
 * ================================================================== */
static void GPIO_Init_UART_Pin(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA2 → Alternate Function mode */
    GPIOA->MODER  &= ~(3U << (2 * 2));
    GPIOA->MODER  |=  (2U << (2 * 2));     /* AF mode                 */

    /* AF7 for USART2 (AFR[0] for pins 0-7) */
    GPIOA->AFR[0] &= ~(0xFU << (2 * 4));
    GPIOA->AFR[0] |=  (7U   << (2 * 4));   /* AF7                     */

    /* High speed */
    GPIOA->OSPEEDR |= (3U << (2 * 2));
}

/* ==================================================================
 *  PUBLIC API
 * ================================================================== */

void UART_Init(void)
{
    GPIO_Init_UART_Pin();

    /* Enable USART2 clock (APB1) */
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* Baud rate:  BRR = f_CK / baud
     * APB1 = 42 MHz.  42 000 000 / 115200 ≈ 364.58
     * Mantissa = 364 = 0x16C,  Fraction = 0.58 × 16 ≈ 9              */
    USART2->BRR = (364U << 4) | 9U;

    /* Enable transmitter and USART */
    USART2->CR1  = USART_CR1_TE | USART_CR1_UE;
    USART2->CR3  = 0;

    /* ---- DMA1 Stream 6 Channel 4 for USART2_TX -------------------- */
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    while (DMA1_Stream6->CR & DMA_SxCR_EN) { /* wait */ }

    DMA1->HIFCR = (0x3FU << 16);           /* Clear Stream 6 flags    */

    DMA1_Stream6->CR  = 0;
    DMA1_Stream6->CR |= (4U << DMA_SxCR_CHSEL_Pos); /* Channel 4      */
    DMA1_Stream6->CR |= (1U << DMA_SxCR_DIR_Pos);   /* Mem → Periph   */
    DMA1_Stream6->CR |= DMA_SxCR_MINC;              /* Mem increment   */
    DMA1_Stream6->CR |= DMA_SxCR_TCIE;              /* TC interrupt    */

    /* Peripheral address: USART2 data register */
    DMA1_Stream6->PAR = (uint32_t)&USART2->DR;

    /* Enable DMA1 Stream 6 interrupt in NVIC */
    NVIC_SetPriority(DMA1_Stream6_IRQn, 2);
    NVIC_EnableIRQ(DMA1_Stream6_IRQn);

    /* Enable DMA mode in USART2 */
    USART2->CR3 |= USART_CR3_DMAT;
}

void UART_Transmit_DMA(const uint8_t *data, uint16_t length)
{
    if (uart_tx_busy || length == 0)
        return;                             /* Drop if busy            */

    uart_tx_busy = 1;

    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    while (DMA1_Stream6->CR & DMA_SxCR_EN) { /* wait */ }

    DMA1->HIFCR = (0x3FU << 16);

    DMA1_Stream6->M0AR = (uint32_t)data;
    DMA1_Stream6->NDTR = length;

    DMA1_Stream6->CR |= DMA_SxCR_EN;
}

uint8_t UART_IsBusy(void)
{
    return uart_tx_busy;
}

/* ==================================================================
 *  DMA1 Stream 6 ISR — USART2 TX complete
 * ================================================================== */
void DMA1_Stream6_IRQHandler(void)
{
    if (DMA1->HISR & DMA_HISR_TCIF6) {
        DMA1->HIFCR = DMA_HIFCR_CTCIF6;
        uart_tx_busy = 0;
    }
}
