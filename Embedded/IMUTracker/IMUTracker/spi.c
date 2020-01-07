/*
 * Copyright 2019 mtkrtk
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "spi.h"
#include <LPC8xx.h>

const uint8_t *spiSendBuffer;
uint8_t *spiReceiveBuffer;
uint32_t spiBytesToSend;
uint32_t spiBytesToReceive;

INLINE void spi_init()
{
    LPC_SYSCON->SPI0CLKSEL = 0x0; /* Select FRO clock for SPI0 */
    
    LPC_SPI0->DIV = 3 - 1; /* Set SPI clock 5 MHz */
    LPC_SPI0->DLY = 3 << 4; /* Set POST_DELAY 3 clock = 600 ns */
    LPC_SPI0->INTENSET = 1 << 0; /* Rx ready */
    LPC_SPI0->CFG = (1 << 0)  /* Enable SPI */
                  | (1 << 2)  /* Master mode */
                  | (1 << 4)  /* CPHA = 1 */
                  | (1 << 5); /* CPOL = 1 */
    NVIC_SetPriority(SPI0_IRQn, 1);
    NVIC_EnableIRQ(SPI0_IRQn);
}

INLINE void spi_transfer(const void *txBuffer, void *rxBuffer, uint32_t length)
{
    spiSendBuffer = (const uint8_t *)txBuffer;
    spiReceiveBuffer = (uint8_t *)rxBuffer;
    spiBytesToSend = length;
    spiBytesToReceive = length;
    asm volatile ("":::"memory"); /* Parameters must be set before interrupt is enabled */
    LPC_SPI0->TXCTL = 7 << 24; /* 8bit data length (This clears end of transfer flag) */
    LPC_SPI0->INTENSET = 1 << 1; /* Tx ready */
}
