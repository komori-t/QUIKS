#include <LPC8xx.h>
#include <rom_api.h>
#include <iap.h>

#define WAIT_RECEIVE while ((LPC_USART0->STAT & (1 << 0)) == 0)
#define WAIT_SEND while ((LPC_USART0->STAT & (1 << 3)) == 0)
#define ACTIVATE_TRANSMITTER LPC_GPIO_PORT->SET[0] = 1 << 1
#define ACTIVATE_RECEIVER LPC_GPIO_PORT->CLR[0] = 1 << 1

int main()
{
    __disable_irq();
    LPC_PWRD_API->set_fro_frequency(30000);
    LPC_SYSCON->SYSMEMREMAP = 2;
    LPC_SYSCON->SYSAHBCLKCTRL[0] |= (1 << 6)   /* GPIO */
                                  | (1 << 7)   /* switch-matrix */
                                  | (1 << 14)  /* USART0 */
                                  | (1 << 18); /* IOCON */
    LPC_IOCON->PIO0_1 = 1 << 7;
    LPC_GPIO_PORT->DIRSET[0] = 1 << 1;
    LPC_SWM->PINASSIGN[0] = (11 << 0) /* Assign P0_11 Tx */
                          | (4 << 8) /* Assign P0_4 Rx */
                          | (0xFFFF << 16);
    
    LPC_SYSCON->UART0CLKSEL = 0x2; /* Select FRG clock for USART0 */
    LPC_SYSCON->FRG0DIV = 0xFF;
    LPC_SYSCON->FRG0MULT = 4;
    
    ACTIVATE_RECEIVER;
    LPC_USART0->BRG = 2 - 1; /* Set baud rate */
    LPC_USART0->CFG = (1 << 0)  /* Enable USART0 */
                    | (1 << 2); /* Set data length to 8bit */
    
    struct sIAP iap;
    uint8_t __attribute__((aligned(4))) pageBuffer[64];
    
    WAIT_RECEIVE;
    const uint8_t numUsedPage = LPC_USART0->RXDAT;
    
    ACTIVATE_TRANSMITTER;
    LPC_USART0->TXDAT = 1;
    WAIT_SEND;
    ACTIVATE_RECEIVER;
    
    for (int32_t page = 0; page < numUsedPage; ++page) {
        while (1) {
            for (uint32_t byte = 0; byte < 64; ++byte) {
                WAIT_RECEIVE;
                pageBuffer[byte] = LPC_USART0->RXDAT;
            }
            ACTIVATE_TRANSMITTER;
            for (uint32_t byte = 0; byte < 64; ++byte) {
                LPC_USART0->TXDAT = pageBuffer[byte];
                WAIT_SEND;
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
    
    while (1) ;
}
