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

#include "Q30.h"

static const uint8_t clz_table[256] = {
    8,
    7, 6, 6, 5, 5, 5, 5, 4, 4, 4,
    4, 4, 4, 4, 4, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0
};

INLINE uint32_t count_leading_zeros(uint32_t x)
{
    uint32_t ans = 0;
    if (x & 0xFFFF0000) {
        x >>= 16;
    } else {
        ans += 16;
    }
    if (x & 0xFF00) {
        x >>= 8;
    } else {
        ans += 8;
    }
    return ans + clz_table[x];
}

typedef union {
    uint32_t raw;
    float value;
} raw_float_t;

INLINE float convertQ30ToFloat(int32_t q30)
{
    raw_float_t result;
    if (q30 == 0) {
        result.raw = 0;
        return result.value;
    }
    uint8_t sign;
    if (q30 & (1 << 31)) {
        q30 = -q30;
        sign = 1;
    } else {
        sign = 0;
    }
    const uint8_t clz = count_leading_zeros(q30);
    const uint8_t exponent = 128 - clz;
    uint32_t frac;
    if (clz < 8) {
        frac = (q30 >> (8 - clz));
    } else if (clz > 8) {
        frac = (q30 << (clz - 8));
    } else {
        frac = q30;
    }
    result.raw = (sign << 31) | (exponent << 23) | (frac & 0x007FFFFF);
    return result.value;
}

INLINE int32_t convertFloatToQ30(float v)
{
    raw_float_t raw = {.value = v};
    uint32_t result;
    const uint8_t sign = raw.raw & (1 << 31) ? 1 : 0;
    const int8_t exponent = ((raw.raw & 0x7F800000) >> 23) - 127;
    if (exponent >= -7) {
        result = (0x800000 | (raw.raw & 0x7FFFFF)) << (7 + exponent);
    } else {
        result = (0x800000 | (raw.raw & 0x7FFFFF)) >> (-exponent - 7);
    }
    if (sign) {
        result = (~result) + 1;
    }
    return result;
}

INLINE int32_t multiplyQ30ByParts(int32_t upperA, int32_t lowerA, int32_t upperB, int32_t lowerB)
{
    return ((upperA * upperB) << 2) + (((upperA * lowerB) + (lowerA * upperB)) >> 14);
}

INLINE int32_t multiplyQ30ByPart(int32_t upperA, int32_t lowerA, int32_t b)
{
    return multiplyQ30ByParts(upperA, lowerA, b >> 16, b & 0xFFFF);
}

INLINE int32_t multiplyQ30(int32_t a, int32_t b)
{
    /* slow but most accurate version */
//    return (uint32_t)(((int64_t)a * (int64_t)b) >> 30);
    /* very fast but less accurate version (2^(-14) ~ 10^(-4.2) order) */
//    return (a >> 15) * (b >> 15);
    
    /* fast and accurate version (2^(-27) ~ 10^(-8.1) order) */
    
//    return ((upperA * upperB) << 2) + ((upperA * lowerB) >> 14) + ((lowerA * upperB) >> 14) + ((lowerA * lowerB) >> 30);
    
    /* little more fast version (accuracy order is the same as above) */
//    return ((upperA * upperB) << 2) + (((upperA * lowerB) + (lowerA * upperB)) >> 14);
    
    return multiplyQ30ByPart(a >> 16, a & 0xFFFF, b);
}

INLINE uint32_t sqrtQ30(uint32_t x)
{
    if (x >= 0x40000000 /* 1.0 */) {
        return x;
    }
    const uint8_t clz = count_leading_zeros(x);
    uint32_t root;
    uint32_t gain;
    if (clz & 1) {
        /* a = 0.3685, 0.25 <= x < 0.5 */
        gain = clz - 3;
        const int32_t xMinusA = (x << gain) - 0x17958106;
        const int32_t upperXMinusA = xMinusA >> 16;
        const int32_t lowerXMinusA = xMinusA & 0xFFFF;
        root = 0x26D9C6B9 + multiplyQ30ByPart(upperXMinusA, lowerXMinusA, 0x34B6F28B + multiplyQ30ByPart(upperXMinusA, lowerXMinusA, 0xDC3CAD15 + multiplyQ30ByPart(upperXMinusA, lowerXMinusA, 0x30866BA6 + multiplyQ30ByPart(upperXMinusA, lowerXMinusA, 0xADB2C40A))));
    } else {
        /* a = 0.737, 0.5 <= x < 1.0 */
        gain = clz - 2;
        const int32_t xMinusA = (x << gain) - 0x2F2B020C;
        const int32_t upperXMinusA = xMinusA >> 16;
        const int32_t lowerXMinusA = xMinusA & 0xFFFF;
        root = 0x36F173A1 + multiplyQ30ByPart(upperXMinusA, lowerXMinusA, 0x25465E6C + multiplyQ30ByPart(upperXMinusA, lowerXMinusA, 0xF35B1AAD + multiplyQ30ByPart(upperXMinusA, lowerXMinusA, 0x893FE93 + multiplyQ30ByPart(upperXMinusA, lowerXMinusA, 0xF8B9B9A7))));
    }
    return (uint32_t)(root >> (gain >> 1));
}

INLINE uint32_t squareQ30(int32_t x)
{
    const int32_t upper = x >> 16;
    const int32_t lower = x & 0xFFFF;
    return multiplyQ30ByParts(upper, lower, upper, lower);
}
