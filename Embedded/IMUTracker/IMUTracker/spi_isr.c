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
