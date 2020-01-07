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
#include "ICM20948.h"
#include "spi.h"
#include "rs485.h"
#include "Q30.h"

static struct __attribute__((packed)) {
    uint8_t dummy[3];
    uint8_t entry;
    union {
        uint8_t  byte[16];
        uint16_t halfword[8];
        uint32_t word[4];
    };
} __attribute__((aligned(4))) spiRxBuffer;

static volatile int isWaitingSPI;
static volatile int isWaitingRS485;
static quaternion_t quaternion;

typedef struct buffer_t {
    struct buffer_t *next;
    uint8_t buf[17];
} buffer_t;

static buffer_t rs485Buffers[2] = {
    {.next = &rs485Buffers[1], .buf = {125}},
    {.next = &rs485Buffers[0], .buf = {125}}
};
static buffer_t *currentReadBuffer = &rs485Buffers[0];
static buffer_t *currentWriteBuffer = &rs485Buffers[0];

static const uint8_t initializeCommand[] = {
    /* Length(0 indicates the end), Address, Datas */
    2, 127, 0, /* Select BANK 0 */
    2, 3, 1 << 4, /* USER_CTRL, disable I2C */
    2, 5, 0x70, /* LP_CONFIG, enable duty-cycled mode */
    2, 6, 0x01, /* PWR_MGMT_1, clear sleep bit */
    0
};

static const uint8_t enableDMPCommand0[] = {
    2, 127, 0x20, /* Select BANK 2 */
    3, 80, 0x10, 0x00, /* setup DMP start address and firmware (undocumented) */
    
    /* Reset DMP output control registers (undocumented) */
    2, 127, 0, /* Select BANK 0 */
    2, 126, 0, /* select DMP bank #0 */
    2, 124, 0x40, /* select DMP register address */
    3, 125, 0x00, 0x00, /* reset data output control registers */
    2, 124, 0x42, /* select DMP register address */
    3, 125, 0x00, 0x00, /* reset data output control registers */
    2, 124, 0x4C, /* select DMP register address */
    3, 125, 0x00, 0x00, /* reset data interrupt control register */
    2, 124, 0x4E, /* select DMP register address */
    3, 125, 0x00, 0x00, /* reset motion event control register */
    2, 124, 0x8A, /* select DMP register address */
    3, 125, 0x00, 0x00, /* reset data ready status register */
    
    /* Sets FIFO watermark (undocumented) */
    2, 126, 0x01, /* select DMP bank #1 */
    2, 124, 0xFE, /* select DMP register address */
    3, 125, 0x03, 0x20, /* set FIFO watermark to 80% of actual FIFO size */
    
    2, 16, 0x02, /* INT_ENABLE, enable DMP interrupt */
    2, 18, 0x01, /* INT_ENABLE_2, enable FIFO interrupt */
    2, 38, 0xE4, /* REG_SINGLE_FIFO_PRIORITY_SEL (undocumented) */
    
    /* Disable HW temp fix (undocumented) */
    2, 117, 72,
    
    2, 127, 0x20, /* select BANK 2 */
    2, 0, 0x13, /* GYRO_SMPLRT_DIV, 56 Hz */
    3, 16, 0x00, 0x13, /* ACCEL_SMPLRT_DIV_{1,2}, 56 Hz */
    
    /* Init the sample rate to 56 Hz for BAC,STEPC and B2S (undocumented) */
    2, 127, 0x00, /* select BANK 0 */
    2, 126, 0x03, /* select DMP bank #3 */
    2, 124, 0x0A, /* select DMP register address */
    3, 125, 0x00, 0x00, /* set BAC_RATE 56 Hz */
    2, 124, 0x08, /* select DMP register address */
    3, 125, 0x00, 0x00, /* set B2C_RATE 56 Hz */
    
    /* set compass orientation matrix (undocumented) */
    2, 126, 0x01, /* select DMP bank #1 */
    2, 124, 0x70, /* select DMP register address */
    5, 125, 0x09, 0x99, 0x99, 0x99, /* matrix 00 = 161061273 */
    2, 124, 0x74,
    5, 125, 0x00, 0x00, 0x00, 0x00, /* matrix 01 = 0 */
    2, 124, 0x78,
    5, 125, 0x00, 0x00, 0x00, 0x00, /* matrix 02 = 0 */
    2, 124, 0x7C,
    5, 125, 0x00, 0x00, 0x00, 0x00, /* matrix 10 = 0 */
    2, 124, 0x80,
    5, 125, 0xF6, 0x66, 0x66, 0x67, /* matrix 11 = -161061273 */
    2, 124, 0x84,
    5, 125, 0x00, 0x00, 0x00, 0x00, /* matrix 12 = 0 */
    2, 124, 0x88,
    5, 125, 0x00, 0x00, 0x00, 0x00, /* matrix 20 = 0 */
    2, 124, 0x8C,
    5, 125, 0x00, 0x00, 0x00, 0x00, /* matrix 21 = 0 */
    2, 124, 0x90,
    5, 125, 0xF6, 0x66, 0x66, 0x67, /* matrix 22 = -161061273 */
    
    2, 127, 0x20, /* select BANK 2 */
    2, 20, 0x02, /* ACCEL_CONFIG */
    2, 21, 0x00, /* ACCEL_CONFIG_2 */
    2, 127, 0x00, /* select BANK 0 */
    
    /* Sets scale in DMP to convert accel data to 1g=2^25 regardless of fsr. (undocumented) */
    2, 124, 0xE0, /* select DMP register address */
    5, 125, 0x04, 0x00, 0x00, 0x00,
    
    /* According to input fsr, a scale factor will be set at memory location ACC_SCALE2 (undocumented) */
    2, 126, 0x04, /* select DMP bank #4 */
    2, 124, 0xF4, /* select DMP register address */
    5, 125, 0x00, 0x00, 0x00, 0x00,
    
    2, 127, 0x20, /* select BANK 2 */
    3, 1, 0x07, 0x00, /* GYRO_CONFIG_{1,2} */
    2, 127, 0x00, /* select BANK 0 */
    
    /* Sets the gyro_sf used by quaternions on the DMP. (undocumented) */
    2, 127, 0x10, /* select BANK 1 */
    2, (1 << 7) | 40, 0, /* TIMEBASE_CORRECTION_PLL */
    0
};

static const uint8_t enableDMPCommand1[] = {
    2, 127, 0x00, /* select BANK 0 */
    2, 126, 0x01, /* select DMP bank #1 */
    2, 124, 0x30, /* select DMP register address */
    0
};

static const uint8_t enableDMPCommand2[] = {
    /* Sets data output control register 1. (undocumented) */
    2, 126, 0x00, /* select DMP bank #0 */
    2, 124, 0x40, /* select DMP register address */
    3, 125, 0x04, 0x08, /* set 0x0408 to get accuracy info */
    
    /* Sets data interrupt control register. (undocumented) */
    2, 124, 0x4C, /* select DMP register address */
    3, 125, 0x04, 0x08, /* set 0x0408 to get accuracy info */
    
    /* Sets data output control register 2. (undocumented) */
    2, 124, 0x42, /* select DMP register address */
    3, 125, 0x10, 0x00, /* set 0x1000 to get accuracy info */
    
    /* Sets motion event control register. (undocumented) */
    2, 124, 0x4E, /* select DMP register address */
    3, 125, 0x03, 0xC0,
    
    /* Sets accel quaternion gain according to accel engine rate. (undocumented) */
    2, 126, 0x01, /* select DMP bank #1 */
    2, 124, 0x0C, /* select DMP register address */
    5, 125, 0x00, 0xE8, 0xBA, 0x2E,
    
    /* Sets accel cal parameters based on different accel engine rate/accel cal running rate (undocumented) */
    2, 126, 0x05, /* select DMP bank #5 */
    2, 124, 0xB0, /* select DMP register address */
    5, 125, 0x3D, 0x27, 0xD2, 0x7D,
    2, 124, 0xC0,
    5, 125, 0x02, 0xD8, 0x2D, 0x83,
    
    2, 127, 0x20, /* select BANK 2 */
    3, 16, 0x00, 0x04, /* ACCEL_SMPLRT_DIV_{1,2} */
    2, 0, 0x04, /* GYRO_SMPLRT_DIV */
    2, 127, 0x00, /* select BANK 0 */
    
    /* Sets sensor ODR. (undocumented) */
    2, 126, 0x00, /* select DMP bank #0 */
    2, 124, 0xA8, /* select DMP register address */
    3, 125, 0x00, 0x02,
    
    2, 127, 0x30, /* select BANK 3 */
    2, 0, 0x04, /* I2C_MST_ODR_CONFIG */
    
    2, 127, 0x00, /* select BANK 0 */
    2, 7, 0x40, /* PWR_MGMT_2, all sensors on */
    2, 127, 0x30, /* select BANK 3 */
    2, 3, 0x8C, /* I2C_SLV0_ADDR, AK09916 for read */
    2, 4, 0x03, /* I2C_SLV0_REG */
    2, 5, 0xDA, /* I2C_SLV0_CTRL, read 10 bytes */
    2, 7, 0xC, /* I2C_SLV1_ADDR, AK09916 for write */
    2, 8, 0x31, /* I2C_SLV1_REG */
    2, 10, 0x01, /* I2C_SLV1_DO */
    2, 9, 0x81, /* I2C_SLV1_CTRL, write 1 byte */
    2, 127, 0x00, /* select BANK 0 */
    2, 3, 0xF0, /* USER_CTRL, enable I2C */
    
    /* Sets data rdy status register. (undocumented) */
    2, 126, 0x00, /* select DMP bank #0 */
    2, 124, 0x8A, /* select DMP register address */
    3, 125, 0x00, 0x0B,
    
    2, 6, 0x21, /* PWR_MGMT_1, enter LP mode */
    0
};

static const uint8_t readIntStatusCommand[] = {
    (1 << 7) | 24, 0, 0
};

static const uint8_t readFifoCountCommand[] = {
    (1 << 7) | 112, 0, 0
};

static const uint8_t readFifoDataCommand[17] = {
    (1 << 7) | 114
};

INLINE void ICM20948_rs485_callback()
{
    isWaitingRS485 = 0;
    currentReadBuffer = currentReadBuffer->next;
    rs485_continous_receive(&currentReadBuffer->buf[1], 16);
}

INLINE void spi_transfer_callback()
{
    isWaitingSPI = 0;
}

STATIC INLINE void do_spi(const void *data, uint32_t length)
{
    isWaitingSPI = 1;
    spi_transfer(data, &spiRxBuffer.entry, length);
    while (isWaitingSPI) ;
}

STATIC INLINE void writeRegisters(const uint8_t *commands)
{
    while (1) {
        const uint8_t length = *commands;
        if (length == 0) {
            break;
        }
        ++commands;
        do_spi(commands, length);
        commands += length;
    }
}

STATIC INLINE void writeDMPBank(uint8_t bank)
{
    const uint8_t command[] = {126, bank};
    do_spi(command, 2);
}

STATIC INLINE void writeDMPAddress(uint8_t address)
{
    const uint8_t command[] = {124, address};
    do_spi(command, 2);
}

INLINE void ICM20948_init()
{
    writeRegisters(initializeCommand);
}

STATIC INLINE void do_write(uint16_t *address)
{
    writeDMPAddress(*address);
    while (isWaitingRS485) ;
    isWaitingRS485 = 1;
    do_spi(currentWriteBuffer->buf, 17);
    currentWriteBuffer = currentWriteBuffer->next;
    *address += 16;
}

INLINE void ICM20948_download(void)
{
    isWaitingRS485 = 1;
    rs485_receive(&currentReadBuffer->buf[1], 16);
    uint8_t dmpBank = 0;
    uint16_t dmpAddress = 0x90;
    while (dmpBank < 0x38) {
        writeDMPBank(dmpBank);
        while (dmpAddress < 0x100) {
            do_write(&dmpAddress);
        }
        dmpBank += 1;
        dmpAddress = 0;
    }
    writeDMPBank(dmpBank);
    while (dmpAddress < 0x60) {
        do_write(&dmpAddress);
    }
    writeDMPAddress(dmpAddress);
    while (isWaitingRS485) ;
    do_spi(currentWriteBuffer->buf, 3);
}

INLINE void ICM20948_enable_dmp(void)
{
    writeRegisters(enableDMPCommand0);
    
    const uint8_t pllError = spiRxBuffer.byte[0];
    const uint64_t MagicConstant = 264446880937391;
    const uint64_t MagicConstantScale = 100000;
    uint64_t resultLL;
    int32_t gyroSf;
    if (pllError & 0x80) {
        resultLL = (MagicConstant * (int64_t)(1ULL << 3) * (1 + 4) / (1270 - (pllError & 0x7F)) / MagicConstantScale);
    } else {
        resultLL = (MagicConstant * (int64_t)(1ULL << 3) * (1 + 4) / (1270 + pllError) / MagicConstantScale);
    }
    if (resultLL > 0x7FFFFFFF) {
        gyroSf = 0x7FFFFFFF;
    } else {
        gyroSf = (int32_t)resultLL;
    }
    gyroSf = __REV(gyroSf);
    
    writeRegisters(enableDMPCommand1);
    uint8_t gyroSfCommand[5];
    gyroSfCommand[0] = 125;
    *(uint32_t *)&gyroSfCommand[1] = gyroSf;
    do_spi(gyroSfCommand, 5);
    
    writeRegisters(enableDMPCommand2);
}

#ifndef INLINE_ALL
#pragma GCC push_options
#pragma GCC optimize ("O0")
/*
 * BUG:
 *     GCC cannot handle REV instruction with optimization when not inlined
 *     Compiling with -Os successes, but specifying Os with pragma failes (why?)
 *     When inlining is enabled, compiling with -O2 successes
 */
#endif

INLINE void ICM20948_process_fifo(void)
{
    do_spi(readIntStatusCommand, 3);
    if ((spiRxBuffer.halfword[0] & ((1 << 0) | (1 << (1 + 8)))) == 0) {
        return;
    }
    do_spi(readFifoCountCommand, 3);
    const uint32_t fifoCount = __REV16(spiRxBuffer.halfword[0]);
    if (fifoCount < 2) {
        return;
    }
    do_spi(readFifoDataCommand, 3);
    int isCompassAccuracyAvailable = 0;
    const uint16_t header1 = spiRxBuffer.halfword[0];
    if (header1 & (1 << 11)) {
        /* header2 available */
        do_spi(readFifoDataCommand, 3);
        const uint16_t header2 = spiRxBuffer.halfword[0];
        if (header2 & (1 << 4)) {
            isCompassAccuracyAvailable = 1;
        }
    }
    if (header1 & (1 << 2)) {
        /* quaternion available */
        do_spi(readFifoDataCommand, 17);
        const int32_t x = __REV(spiRxBuffer.word[0]);
        const int32_t y = __REV(spiRxBuffer.word[1]);
        const int32_t z = __REV(spiRxBuffer.word[2]);
        quaternion.x.value = x;
        quaternion.y.value = y;
        quaternion.z.value = z;
        quaternion.w.value = sqrtQ30((1 << 30) - squareQ30(x) - squareQ30(y) - squareQ30(z));
        ICM20948_quaternion_callback(&quaternion);
    }
    if (isCompassAccuracyAvailable) {
        do_spi(readFifoDataCommand, 3);
        ICM20948_compass_accuracy_callback(spiRxBuffer.byte[1]);
    }
}

#ifndef INLINE_ALL
#pragma GCC pop_options
#endif
