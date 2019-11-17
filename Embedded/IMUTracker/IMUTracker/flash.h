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

#ifndef __flash__
#define __flash__

#include <stdint.h>

typedef union {
    uint8_t raw[64];
    struct {
        uint8_t id;
        int8_t  xSign;
        uint8_t xIndex;
        int8_t  ySign;
        uint8_t yIndex;
        int8_t  zSign;
        uint8_t zIndex;
    };
} flash_data_t;

void flash_read(flash_data_t *data);
void flash_write(flash_data_t *data);

#endif
