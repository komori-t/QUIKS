/*
 * Copyright 2020 mtkrtk
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

void SPI0_IRQHandler()
{
    const uint32_t irqFlag = LPC_SPI0->INTSTAT;
    if (irqFlag & (1 << 0)) {
        /* Rx ready */
        const uint32_t data = LPC_SPI0->RXDAT;
        *spiReceiveBuffer = data;
        if (--spiBytesToReceive) {
            ++spiReceiveBuffer;
        } else {
            spi_transfer_callback();
        }
    }
    if (irqFlag & (1 << 1)) {
        /* Tx ready */
        if (--spiBytesToSend) {
            LPC_SPI0->TXDAT = *spiSendBuffer;
            ++spiSendBuffer;
        } else {
            LPC_SPI0->INTENCLR = 1 << 1; /* Tx ready */
            LPC_SPI0->TXDATCTL = *spiSendBuffer
                               | (1 << 20)  /* End of transfer */
                               | (7 << 24); /* 8bit data length */
        }
    }
}
