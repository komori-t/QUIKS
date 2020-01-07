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

#include "rs485.h"
#include <LPC8xx.h>
#include <iap.h>
#include "Protocol.h"

/* Optimal baud rate configs
  ---------------------
 |  Baud  | MULT | BRG |
 |  9600  |  244 |  50 |
 | 115200 |   91 |   6 |
 | 230400 |   91 |   3 |
 | 460800 |    4 |   2 |
  ---------------------
 */

const uint8_t *rs485SendBuffer;
uint32_t rs485BytesToSend;
uint32_t rs485ShouldSendPadding = 0;
uint8_t *rs485ReceiveBuffer;
uint32_t rs485BytesToReceive;
int rs485ShouldSkipZero;

INLINE void rs485_init()
{
    LPC_SYSCON->UART0CLKSEL = 0x2; /* Select FRG clock for USART0 */
    LPC_SYSCON->FRG0DIV = 0xFF;
    LPC_SYSCON->FRG0MULT = 4;
    
    LPC_GPIO_PORT->B0[1] = 0; /* Activate receiver */
    LPC_USART0->BRG = 2 - 1; /* Set baud rate */
    LPC_USART0->INTENSET = 1 << 0; /* Enable Rx ready interrupt */
    NVIC_SetPriority(UART0_IRQn, 0);
    NVIC_EnableIRQ(UART0_IRQn);
    LPC_USART0->CFG = (1 << 0)  /* Enable USART0 */
                    | (1 << 2); /* Set data length to 8bit */
}

INLINE void rs485_send(const void *buf, uint32_t length)
{
    rs485BytesToSend = length;
    rs485SendBuffer = buf;
    asm volatile ("":::"memory"); /* Parameters must be set before interrupt is enabled */
    LPC_GPIO_PORT->B0[1] = 1; /* Activate transmitter */
    LPC_USART0->INTENSET = 1 << 2; /* Enable Tx ready interrupt */
}

INLINE void rs485_receive(void *buf, uint32_t length)
{
    rs485BytesToReceive = length;
    rs485ReceiveBuffer = buf;
    rs485ShouldSkipZero = 0;
    asm volatile ("":::"memory"); /* Ensure parameters are written */
}

INLINE void rs485_continous_receive(void *buf, uint32_t length)
{
    rs485BytesToReceive = length;
    rs485ReceiveBuffer = buf;
    asm volatile ("":::"memory"); /* Ensure parameters are written */
}

#define WAIT_RECEIVE while ((LPC_USART0->STAT & (1 << 0)) == 0)
#define WAIT_SEND while ((LPC_USART0->STAT & (1 << 3)) == 0)
#define ACTIVATE_TRANSMITTER LPC_GPIO_PORT->SET[0] = 1 << 1
#define ACTIVATE_RECEIVER LPC_GPIO_PORT->CLR[0] = 1 << 1

void __attribute__((section(".iap"))) rs485_program_flash_impl(uint8_t numUsedPage)
{
    __disable_irq();
    
    struct sIAP iap;
    uint8_t __attribute__((aligned(4))) pageBuffer[64];
    
    ACTIVATE_TRANSMITTER;
    LPC_USART0->TXDAT = 1;
    WAIT_SEND;
    ACTIVATE_RECEIVER;
    
    for (uint32_t page = 0; page < numUsedPage; ++page) {
        while (1) {
            for (uint32_t byte = 0; byte < 64; ++byte) {
                WAIT_RECEIVE;
                pageBuffer[byte] = LPC_USART0->RXDAT;
                if (pageBuffer[byte] == PACKET_HEADER) {
                    WAIT_RECEIVE;
                    (void)LPC_USART0->RXDAT;
                }
            }
            ACTIVATE_TRANSMITTER;
            for (uint32_t byte = 0; byte < 64; ++byte) {
                LPC_USART0->TXDAT = pageBuffer[byte];
                WAIT_SEND;
                if (pageBuffer[byte] == PACKET_HEADER) {
                    LPC_USART0->TXDAT = 0;
                    WAIT_SEND;
                }
            }
            ACTIVATE_RECEIVER;
            WAIT_RECEIVE;
            if (LPC_USART0->RXDAT) {
                break;
            }
        }
        
        const uint32_t sector = page / 16;
        
        iap.cmd = IAP_PREPARE;
        iap.par[0] = sector;
        iap.par[1] = sector;
        IAP_Call(&iap.cmd, &iap.stat);
        
        iap.cmd = IAP_ERASE_PAGE;
        iap.par[0] = page;
        iap.par[1] = page;
        IAP_Call(&iap.cmd, &iap.stat);
        
        iap.cmd = IAP_PREPARE;
        iap.par[0] = sector;
        iap.par[1] = sector;
        IAP_Call(&iap.cmd, &iap.stat);
        
        iap.cmd = IAP_COPY_RAM2FLASH;
        iap.par[0] = page * 64;
        iap.par[1] = (uintptr_t)pageBuffer;
        iap.par[2] = 64;
        IAP_Call(&iap.cmd, &iap.stat);
        
        ACTIVATE_TRANSMITTER;
        LPC_USART0->TXDAT = 1;
        WAIT_SEND;
        ACTIVATE_RECEIVER;
    }

    while (1) ; /* Power down is required */
}
