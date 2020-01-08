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
