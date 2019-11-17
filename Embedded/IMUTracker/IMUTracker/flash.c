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

#include "flash.h"
#include <iap.h>
#include <LPC8xx.h>

static const flash_data_t __attribute__((used, aligned(64))) defaultFlashData = {
    .id = 1,
    .xSign = 1, .xIndex = 0, .ySign = 1, .yIndex = 1, .zSign = 1, .zIndex = 2
};

void flash_read(flash_data_t *data)
{
    const uint32_t *src = (const uint32_t *)&defaultFlashData;
    uint32_t *dst = (uint32_t *)data;
    uint32_t count = sizeof(flash_data_t) / 4;
    do {
        *dst++ = *src++;
    } while (--count);
}

void flash_write(flash_data_t *data)
{
    __disable_irq();

    const uint32_t sector = (uint32_t)&defaultFlashData / 1024;
    const uint32_t page = (uint32_t)&defaultFlashData / 64;
    
    struct sIAP iap;
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
    iap.par[0] = (uintptr_t)&defaultFlashData;
    iap.par[1] = (uintptr_t)data;
    iap.par[2] = sizeof(flash_data_t);
    IAP_Call(&iap.cmd, &iap.stat);
    
    __enable_irq();
}

