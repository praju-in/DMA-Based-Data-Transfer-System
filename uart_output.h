/**
 * @file    uart_output.h
 * @brief   Non-blocking UART transmit driver (DMA-backed).
 * @target  STM32F407VG — USART2 (PA2 TX, PA3 RX)
 *
 * Provides a simple API to transmit processed data over UART without
 * blocking the main processing loop.
 */

#ifndef UART_OUTPUT_H
#define UART_OUTPUT_H

#include <stdint.h>
#include "system_config.h"

/**
 * @brief  Initialise USART2 and its DMA stream for TX.
 */
void UART_Init(void);

/**
 * @brief  Queue a buffer for DMA-based transmission.
 * @param  data   Pointer to the byte array to send.
 * @param  length Number of bytes to transmit.
 * @note   Non-blocking.  If a previous transfer is still in progress
 *         the call is silently dropped to avoid data corruption.
 */
void UART_Transmit_DMA(const uint8_t *data, uint16_t length);

/**
 * @brief  Check whether the UART TX DMA is currently busy.
 * @return 1 if busy, 0 if idle.
 */
uint8_t UART_IsBusy(void);

#endif /* UART_OUTPUT_H */
