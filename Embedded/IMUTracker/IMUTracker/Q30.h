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

#ifndef __Q30__
#define __Q30__

#include <stdint.h>

#ifndef INLINE_ALL
uint32_t count_leading_zeros(uint32_t x);
float convertQ30ToFloat(int32_t q30);
int32_t convertFloatToQ30(float v);
int32_t multiplyQ30(int32_t a, int32_t b);
int32_t multiplyQ30ByPart(int32_t upperA, int32_t lowerA, int32_t b);
int32_t multiplyQ30ByParts(int32_t upperA, int32_t lowerA, int32_t upperB, int32_t lowerB);
uint32_t sqrtQ30(uint32_t x);
uint32_t squareQ30(int32_t x);
#endif

#endif
