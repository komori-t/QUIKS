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

#include <LPC8xx.h>
#include <rom_api.h>
#include <iap.h>
#include "Protocol.h"

#define US_TO_CLOCK(us) ((us) * 15)

typedef void (*isr_t)(void);
extern void (* const g_pfnVectors[])(void);
extern const uint32_t __vectors_start__;

void __attribute__((section(".bootloader2"), noreturn, noinline)) bootloader_reboot()
{
    /* Request system reset */
    SCB->AIRCR = (0x05FA << 16) | (1 << 2);
    __builtin_unreachable();
}

STATIC INLINE void __attribute__((section(".bootloader1"))) rs485_init()
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
    LPC_USART0->ADDR = 0; /* Use address register for counter variable */
}

void __attribute__((section(".bootloader1"), noreturn)) bootloader_reset_handler()
{
    LPC_PWRD_API->set_fro_frequency(30000); /* Set core clock 15 MHz */
    
    /* Enable peripheral clocks */
    LPC_SYSCON->SYSAHBCLKCTRL[0] |= (1 << 6)   /* GPIO */
                                  | (1 << 7)   /* switch-matrix */
                                  | (1 << 10)  /* multi-rate timer */
                                  | (1 << 14)  /* USART0 */
                                  | (1 << 18); /* IOCON */
    
    LPC_IOCON->PIO0_1 = 1 << 7; /* Disable pull-up and hysteresis (RS485 EN) */
    LPC_GPIO_PORT->DIRSET[0] = 1 << 1; /* Set PIO0_1 as output */
    
    LPC_SWM->PINASSIGN[0] = (11 << 0) /* Assign P0_11 Tx */
                          | (4 << 8) /* Assign P0_4 Rx */
                          | (0xFFFF << 16);
    
    rs485_init();
    
    /* Wait 2s for firmware update packet */
    /* Format: 0x46 0x93 <Number of pages> */
    /* This also waits ICM20948 to launch (100ms) */
    NVIC_EnableIRQ(MRT_IRQn);
    LPC_MRT->Channel[0].CTRL = (1 << 1)  /* One-shot mode */
                             | (1 << 0); /* Enable interrupt */
    LPC_MRT->Channel[0].INTVAL = US_TO_CLOCK(2000000); /* Set interval to 2s */
    while (LPC_MRT->Channel[0].STAT & (1 << 1)) __WFI();
    
    /* Disable peripheral clocks */
    LPC_SYSCON->SYSAHBCLKCTRL[0] &= ~((1 << 7)    /* switch-matrix */
                                  |   (1 << 10)   /* multi-rate timer clock */
                                  |   (1 << 18)); /* IOCON */
    
    /* Initialize stack and branch to user code */
    SCB->VTOR = __vectors_start__;
    asm volatile ("ldr r0, =__vectors_start__;"
                  "ldr r0, [r0];"
                  "ldr r1, [r0];"
                  "mov sp, r1;"
                  "ldr r1, [r0, #4];"
                  "bx  r1");
    
    /* Declare with `noreturn' makes GCC omit return instructions */
    /* But GCC cannot see above inline assembly will return or not */
    /* So we put unreachable mark here to supress warning */
    __builtin_unreachable();
}

#define WAIT_RECEIVE while ((LPC_USART0->STAT & (1 << 0)) == 0)
#define WAIT_SEND while ((LPC_USART0->STAT & (1 << 3)) == 0)
#define ACTIVATE_TRANSMITTER LPC_GPIO_PORT->SET[0] = 1 << 1
#define ACTIVATE_RECEIVER LPC_GPIO_PORT->CLR[0] = 1 << 1

void __attribute__((section(".bootloader1"))) rs485_program_flash_impl(uint8_t numUsedPage)
{
    __disable_irq();
    
    struct sIAP iap;
    uint8_t __attribute__((aligned(4))) pageBuffer[64];
    
    ACTIVATE_TRANSMITTER;
    LPC_USART0->TXDAT = 1;
    WAIT_SEND;
    ACTIVATE_RECEIVER;
    
    for (uint32_t pageCounter = 0; pageCounter < numUsedPage; ++pageCounter) {
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
        
        const uint32_t flashPage = pageCounter + ((uintptr_t)&__vectors_start__) / 64;
        const uint32_t sector = flashPage / 16;
        
        iap.cmd = IAP_PREPARE;
        iap.par[0] = sector;
        iap.par[1] = sector;
        IAP_Call(&iap.cmd, &iap.stat);
        
        iap.cmd = IAP_ERASE_PAGE;
        iap.par[0] = flashPage;
        iap.par[1] = flashPage;
        IAP_Call(&iap.cmd, &iap.stat);
        
        iap.cmd = IAP_PREPARE;
        iap.par[0] = sector;
        iap.par[1] = sector;
        IAP_Call(&iap.cmd, &iap.stat);
        
        iap.cmd = IAP_COPY_RAM2FLASH;
        iap.par[0] = flashPage * 64;
        iap.par[1] = (uintptr_t)pageBuffer;
        iap.par[2] = 64;
        IAP_Call(&iap.cmd, &iap.stat);
        
        ACTIVATE_TRANSMITTER;
        LPC_USART0->TXDAT = 1;
        WAIT_SEND;
        ACTIVATE_RECEIVER;
    }

    bootloader_reboot();
}

static void __attribute__((section(".bootloader2"))) rs485_handler()
{
    const uint32_t rs485_update_magic_0 = 0x46;
    const uint32_t rs485_update_magic_1 = 0x93;
    
    const uint32_t flag = LPC_USART0->INTSTAT;
    if ((flag & (1 << 0)) == 0) {
        /* Reboot if it is not receive interrupt */
        bootloader_reboot();
    }
    
    switch (LPC_USART0->ADDR) {
        case 0:
            if (LPC_USART0->RXDAT == rs485_update_magic_0) {
                LPC_USART0->ADDR = 1;
            }
            break;
            
        case 1:
            if (LPC_USART0->RXDAT == rs485_update_magic_1) {
                LPC_USART0->ADDR = 2;
            } else {
                LPC_USART0->ADDR = 0;
            }
            break;
            
        case 2:
            rs485_program_flash_impl(LPC_USART0->RXDAT);
            break;
            
        default:
            bootloader_reboot();
            break;
    }
}

static void __attribute__((section(".bootloader2"))) mrt_handler()
{
    LPC_MRT->IRQ_FLAG = 1 << 0; /* Clear interrupt flag */
}

extern void _vStackTop(void);
extern void __attribute__((weak)) bootloader_checksum();

static const __attribute__((used, section(".bootloader_vector"))) isr_t vectors[] = {
    // Core Level - CM0plus
    &_vStackTop, // The initial stack pointer
    bootloader_reset_handler,       // The reset handler
    bootloader_reboot,              // The NMI handler
    bootloader_reboot,              // The hard fault handler
    0,                              // Reserved
    0,                              // Reserved
    0,                              // Reserved
    bootloader_checksum,            // LPC MCU Checksum
    0,                              // Reserved
    0,                              // Reserved
    0,                              // Reserved
    bootloader_reboot,                         // SVCall handler
    0,                              // Reserved
    0,                              // Reserved
    bootloader_reboot,              // The PendSV handler
    bootloader_reboot,              // The SysTick handler

    // Chip Level - LPC80x
    bootloader_reboot,              //  0 - SPI0
    0,                              //  1 - Reserved
    bootloader_reboot,              //  2 - DAC0
    rs485_handler,                  //  3 - UART0
    bootloader_reboot,              //  4 - UART1
    0,                              //  5 - Reserved
    0,                              //  6 - Reserved
    bootloader_reboot,              //  7 - I2C1
    bootloader_reboot,              //  8 - I2C0
    0,                              //  9 - Reserved
    mrt_handler,                    // 10 - Multi-rate timer
    bootloader_reboot,              // 11 - Analog comparator / Cap Touch
    bootloader_reboot,              // 12 - Windowed watchdog timer
    bootloader_reboot,              // 13 - BOD
    0,                              // 14 - Reserved
    bootloader_reboot,              // 15 - Self wake-up timer
    bootloader_reboot,              // 16 - ADC seq A
    bootloader_reboot,              // 17 - ADC_seq B
    bootloader_reboot,              // 18 - ADC threshold compare
    bootloader_reboot,              // 19 - ADC overrun
    0,                              // 20 - Reserved
    0,                              // 21 - Reserved
    0,                              // 22 - Reserved
    bootloader_reboot,              // 23 - Timer 0
    bootloader_reboot,              // 24 - PININT0
    bootloader_reboot,              // 25 - PININT1
    bootloader_reboot,              // 26 - PININT2
    bootloader_reboot,              // 27 - PININT3
    bootloader_reboot,              // 28 - PININT4
    bootloader_reboot,              // 29 - PININT5
    bootloader_reboot,              // 30 - PININT6
    bootloader_reboot               // 31 - PININT7
};
