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

#include <LPC8xx.h>
#include <rom_api.h>
#include "Protocol.h"
#ifndef INLINE_ALL
#include "rs485.h"
#include "spi.h"
#include "Q30.h"
#include "flash.h"
#include "ICM20948.h"
#include "Quaternion.h"
#else
#include "rs485.c"
#include "spi.c"
#include "Q30.c"
#include "Quaternion.c"
#include "flash.c"
#endif

#define ENTER_SLEEP SCB->SCR = SCB_SCR_SLEEPONEXIT_Msk; /* Enter sleep on return from ISR */ \
                    __WFI()
#define EXIT_SLEEP SCB->SCR = 0 /* Leave sleep on return from this ISR */
#define US_TO_CLOCK(us) ((us) * 15)
#define LED_ON LPC_GPIO_PORT->B0[9] = 1
#define LED_OFF LPC_GPIO_PORT->B0[9] = 0

static enum {
    state_initializing,
    state_downloading_dmp,
    state_waiting_for_header,
    state_waiting_for_id,
    state_waiting_for_command,
    state_waiting_for_unity_offset,
    state_waiting_for_axis,
    state_waiting_for_new_id,
    state_waiting_for_num_pages,
    state_replying_ack,
    state_replying_quaternion,
    state_replying_compass_accuracy,
    state_flashing,
} state = state_initializing;

static volatile int isDMPFirmwareDownloaded = 0;
static uint8_t serialBuffer[16];

static const uint8_t replyAckPacket[] = {
    PACKET_HEADER, /* ID = 0 will be automatically inserted */ Command_Reply_Ack, 1
};

static volatile struct __attribute__((packed)) {
    uint8_t dummy[2];
    uint8_t header;
    /* ID = 0 will be automatically inserted */
    uint8_t command;
    float w;
    float x;
    float y;
    float z;
} __attribute__((aligned(4))) quaternionReplyPacket = {
    .header = PACKET_HEADER, .command = Command_Reply_Quaternion,
};

static volatile struct __attribute__((packed)) {
    uint8_t header;
    /* ID = 0 will be automatically inserted */
    uint8_t command;
    uint8_t accuracy;
} replyCompassAccuracyPacket = {
    .header = PACKET_HEADER, .command = Command_Reply_Compass_Accuracy
};

static volatile quaternion_t currentChipQuaternion;
static volatile quaternion_t chipOffset = QUATERNION_INITIALIZER;
static volatile quaternion_t unityOffset = QUATERNION_INITIALIZER;

static flash_data_t __attribute__((aligned(4))) retainedData;

STATIC INLINE void replyAck()
{
    state = state_replying_ack;
    rs485_send(replyAckPacket, sizeof(replyAckPacket));
}

void PININT0_IRQHandler()
{
    LPC_PIN_INT->RISE = 1 << 0; /* Clear interrupt flag */
    EXIT_SLEEP;
}

void MRT_IRQHandler()
{
    LPC_MRT->IRQ_FLAG = 1 << 0; /* Clear interrupt flag */
}

INLINE void ICM20948_quaternion_callback(const quaternion_t *quaternion)
{
    __disable_irq();
    quaternion_copy(quaternion, (quaternion_t *)&currentChipQuaternion);
    const quaternion_t theChipOffset = QUATERNION_INIT_COPY(chipOffset);
    const quaternion_t theUnityOffset = QUATERNION_INIT_COPY(unityOffset);
    __enable_irq();
    quaternion_t chipQuat;
    quaternion_multiply(&theChipOffset, quaternion, &chipQuat);
    quaternion_t unityQuat;
    unityQuat.w.value = chipQuat.w.value;
    unityQuat.x.value = retainedData.xSign * chipQuat.axis[retainedData.xIndex].value;
    unityQuat.y.value = retainedData.ySign * chipQuat.axis[retainedData.yIndex].value;
    unityQuat.z.value = retainedData.zSign * chipQuat.axis[retainedData.zIndex].value;
    quaternion_left_mutable_multiply(&unityQuat, &theUnityOffset);
    const float ieeeW = convertQ30ToFloat(unityQuat.w.value);
    const float ieeeX = convertQ30ToFloat(unityQuat.x.value);
    const float ieeeY = convertQ30ToFloat(unityQuat.y.value);
    const float ieeeZ = convertQ30ToFloat(unityQuat.z.value);
    __disable_irq();
    quaternionReplyPacket.w = ieeeW;
    quaternionReplyPacket.x = ieeeX;
    quaternionReplyPacket.y = ieeeY;
    quaternionReplyPacket.z = ieeeZ;
    __enable_irq();
}

INLINE void ICM20948_compass_accuracy_callback(uint8_t accuracy)
{
    if (accuracy > replyCompassAccuracyPacket.accuracy) {
        replyCompassAccuracyPacket.accuracy = accuracy;
    }
    if (accuracy >= 3) {
        LED_OFF;
    }
}

#ifdef INLINE_ALL
#include "ICM20948.c"
#endif

INLINE void rs485_receive_callback()
{
    switch (state) {
        case state_downloading_dmp:
            ICM20948_rs485_callback();
            break;
            
        case state_waiting_for_header:
            if (serialBuffer[0] == PACKET_HEADER) {
                state = state_waiting_for_id;
            }
            rs485_receive(serialBuffer, 1);
            break;
            
        case state_waiting_for_id:
            if (serialBuffer[0] == DMP_UPLOAD_ID && isDMPFirmwareDownloaded == 0) {
                state = state_downloading_dmp;
                EXIT_SLEEP;
                return;
            }
            if (serialBuffer[0] == retainedData.id) {
                state = state_waiting_for_command;
                rs485_receive(serialBuffer, 1);
            } else {
                state = state_waiting_for_header;
                rs485_receive_callback();
            }
            break;
            
        case state_waiting_for_command:
            switch (serialBuffer[0]) {
                case Command_Read_Quaternion:
                    state = state_replying_quaternion;
                    rs485_send((void *)&quaternionReplyPacket.header, 18);
                    break;
                    
                case Command_Set_Unity_Offset:
                    state = state_waiting_for_unity_offset;
                    rs485_receive(serialBuffer, 16);
                    break;
                    
                case Command_Set_ID:
                    state = state_waiting_for_new_id;
                    rs485_receive((uint8_t *)&retainedData.id, 1);
                    break;
                    
                case Command_Flash:
                    state = state_flashing;
                    EXIT_SLEEP;
                    break;
                    
                case Command_Ping:
                    replyAck();
                    break;
                    
                case Command_Program:
                    state = state_waiting_for_num_pages;
                    rs485_receive(serialBuffer, 1);
                    break;
                    
                case Command_Read_Compass_Accuracy:
                    state = state_replying_compass_accuracy;
                    rs485_send((void *)&replyCompassAccuracyPacket.header, 3);
                    break;
                    
                case Command_Set_Chip_Offset:
                    quaternion_inverse((quaternion_t *)&currentChipQuaternion, (quaternion_t *)&chipOffset);
                    replyAck();
                    break;
                    
                case Command_Set_Axis:
                    state = state_waiting_for_axis;
                    rs485_receive(serialBuffer, 1);
                    break;
                    
                default:
                    state = state_waiting_for_header;
                    rs485_receive_callback();
                    break;
            }
            break;
            
        case state_waiting_for_unity_offset:
            unityOffset.w.value = ((int32_t *)serialBuffer)[0];
            unityOffset.x.value = ((int32_t *)serialBuffer)[1];
            unityOffset.y.value = ((int32_t *)serialBuffer)[2];
            unityOffset.z.value = ((int32_t *)serialBuffer)[3];
            replyAck();
            break;
            
        case state_waiting_for_axis:
            retainedData.xSign = (serialBuffer[0] & (1 << 0)) ? -1 : 1;
            retainedData.xIndex = (serialBuffer[0] >> 1) & 0b11;
            retainedData.ySign = (serialBuffer[0] & (1 << 3)) ? -1 : 1;
            retainedData.yIndex = (serialBuffer[0] >> 4) & 0b11;
            retainedData.zSign = (serialBuffer[0] & (1 << 6)) ? -1 : 1;
            retainedData.zIndex = 3 - retainedData.xIndex - retainedData.yIndex;
            replyAck();
            break;
            
        case state_waiting_for_new_id:
            replyAck();
            break;
            
        case state_waiting_for_num_pages:
            rs485_program_flash(serialBuffer[0]);
            break;
            
        default:
            break;
    }
}

INLINE void rs485_send_callback(void)
{
    switch (state) {
        case state_replying_ack:
        case state_replying_quaternion:
        case state_replying_compass_accuracy:
            state = state_waiting_for_header;
            rs485_receive(serialBuffer, 1);
            break;

        default:
            break;
    }
}

#ifdef INLINE_ALL
#include "rs485_isr.c"
#include "spi_isr.c"
#endif

int main()
{
    LPC_PWRD_API->set_fro_frequency(30000);
    
    /* Enable peripheral clocks */
    LPC_SYSCON->SYSAHBCLKCTRL[0] |= (1 << 6)   /* GPIO */
                                  | (1 << 7)   /* switch-matrix */
                                  | (1 << 10)  /* multi-rate timer */
                                  | (1 << 11)  /* SPI0 */
                                  | (1 << 14)  /* USART0 */
                                  | (1 << 18)  /* IOCON */
                                  | (1 << 28); /* GPIO_INT */
    LPC_SYSCON->PINTSEL[0] = 0; /* Assign P0_0 for GPIO interrupt 0 */
    LPC_SYSCON->IRQLATENCY = 0;
    
    LPC_IOCON->PIO0_1 = 1 << 7; /* Disable pull-up and hysteresis (RS485 EN) */
    LPC_IOCON->PIO0_9 = 1 << 7; /* LED */
    LPC_IOCON->PIO0_0 = (1 << 5) | (1 << 7); /* Disable pull-up (ICM intterrupt) */
    
    LPC_GPIO_PORT->DIRSET[0] = (1 << 9) | (1 << 1); /* Set PIO0_1, PIO0_9 as output */
    LED_OFF;
    
    LPC_SWM->PINASSIGN[0] = (11 << 0) /* Assign P0_11 Tx */
                          | (4 << 8) /* Assign P0_4 Rx */
                          | (0xFFFF << 16);
    
    LPC_SWM->PINASSIGN[2] = (13 << 0)   /* P0_13 for SCK */
                          | (7 << 8)    /* P0_7 for MOSI */
                          | (12 << 16)  /* P0_12 for MISO */
                          | (17 << 24); /* P0_17 for SS0 */
    
    LPC_PIN_INT->IENR = 1 << 0; /* Enable interrupt for rising edge on P0_0 */
    NVIC_EnableIRQ(PININT0_IRQn);
    
    /* Wait ICM20948 to launch (I spent a day to figure out this...) */
    NVIC_EnableIRQ(MRT_IRQn);
    LPC_MRT->Channel[0].CTRL = (1 << 1)  /* One-shot mode */
                             | (1 << 0); /* Enable interrupt */
    LPC_MRT->Channel[0].INTVAL = US_TO_CLOCK(100000); /* Set interval to 100 ms */
    while (LPC_MRT->Channel[0].STAT & (1 << 1)) __WFI();
    
    /* Disable peripheral clocks */
    LPC_SYSCON->SYSAHBCLKCTRL[0] &= ~((1 << 7)    /* switch-matrix */
                                  |   (1 << 10)   /* multi-rate timer clock */
                                  |   (1 << 18)); /* IOCON */
    
    flash_read(&retainedData);
    spi_init();
    rs485_init();
    ICM20948_init();
    
    state = state_waiting_for_header;
    rs485_receive(serialBuffer, 1);
    
    while (isDMPFirmwareDownloaded == 0) {
        ENTER_SLEEP;
        switch (state) {
            case state_downloading_dmp:
                ICM20948_download();
                isDMPFirmwareDownloaded = 1;
                break;
                
            case state_flashing:
                flash_write(&retainedData);
                replyAck();
                break;
                
            default:
                break;
        }
    }
    
    LED_ON;
    ICM20948_enable_dmp();
    state = state_waiting_for_header;
    rs485_receive(serialBuffer, 1);
    
    while (1) {
        ENTER_SLEEP;
        ICM20948_process_fifo();
    }

    return 0;
}
