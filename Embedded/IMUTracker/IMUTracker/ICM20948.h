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

#ifndef __ICM20948__
#define __ICM20948__

#include <stdint.h>
#include "Quaternion.h"

#ifndef INLINE_ALL
void ICM20948_init(void);
void ICM20948_download(void);
void ICM20948_rs485_callback(void);
void ICM20948_enable_dmp(void);
void ICM20948_process_fifo(void);

extern void ICM20948_quaternion_callback(const quaternion_t *quaternion);
extern void ICM20948_compass_accuracy_callback(uint8_t accuracy);
#endif

#endif
