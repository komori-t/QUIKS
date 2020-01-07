#include "rs485.h"
#include "Protocol.h"
#include <LPC8xx.h>

void UART0_IRQHandler()
{
    const uint32_t flag = LPC_USART0->INTSTAT;
    if (flag & (1 << 0)) {
        /* Rx ready */
        const uint8_t rxData = LPC_USART0->RXDAT;
        if (rs485ShouldSkipZero) {
            rs485ShouldSkipZero = 0;
            if (rxData == 0) {
                return;
            }
        } else if (rxData == PACKET_HEADER) {
            rs485ShouldSkipZero = 1;
        }
        *rs485ReceiveBuffer = rxData;
        if (--rs485BytesToReceive) {
            ++rs485ReceiveBuffer;
        } else {
            rs485_receive_callback();
        }
    }
    if (flag & (1 << 2)) {
        /* Tx ready */
        if (rs485ShouldSendPadding) {
            rs485ShouldSendPadding = 0;
            LPC_USART0->TXDAT = 0;
        } else {
            LPC_USART0->TXDAT = *rs485SendBuffer;
            if (*rs485SendBuffer == PACKET_HEADER) {
                rs485ShouldSendPadding = 1;
                return;
            }
        }
        if (--rs485BytesToSend) {
            ++rs485SendBuffer;
        } else {
            LPC_USART0->INTENCLR = 1 << 2; /* Disable Tx ready interrupt */
            LPC_USART0->INTENSET = 1 << 3; /* Enable Tx idle interrupt */
            rs485_send_callback();
        }
    }
    if (flag & (1 << 3)) {
        /* Tx idle */
        LPC_GPIO_PORT->B0[1] = 0; /* Activate receiver */
        LPC_USART0->INTENCLR = 1 << 3; /* Disable Tx idle interrupt */
    }
}
