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

#ifndef __rs485__
#define __rs485__

#include <stdint.h>

void rs485_init(void);
void rs485_send(const void *buf, uint32_t length);
void rs485_receive(void *buf, uint32_t length);
void rs485_continous_receive(void *buf, uint32_t length);
/* void rs485_program_flash(uint8_t numUsedPage); */
#define rs485_program_flash ((void (*)(uint8_t))0x3E41)

extern void rs485_send_callback(void);
extern void rs485_receive_callback(void);

#endif
