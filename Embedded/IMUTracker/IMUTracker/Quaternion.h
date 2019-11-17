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

#ifndef __Quaternion__
#define __Quaternion__

#include <stdint.h>

#define QUATERNION_INITIALIZER {.w.value = 1 << 30, .x.value = 0, .y.value = 0, .z.value = 0}
#define QUATERNION_INIT_COPY(src) {.w.value = (src).w.value, .x.value = (src).x.value, .y.value = (src).y.value, .z.value = (src).z.value}

typedef union {
    int32_t value;
    struct __attribute__((packed)) {
        int16_t lower;
        int16_t upper;
    };
} quaternion_component_t;

typedef struct {
    quaternion_component_t w;
    union {
        quaternion_component_t axis[3];
        struct __attribute__((packed)) {
            quaternion_component_t x;
            quaternion_component_t y;
            quaternion_component_t z;
        };
    };
} quaternion_t;

void quaternion_copy(const quaternion_t *src, quaternion_t *dst);
void quaternion_inverse(const quaternion_t *quat, quaternion_t *inverse);
void quaternion_multiply(const quaternion_t *left, const quaternion_t *right, quaternion_t *ans);
void quaternion_left_mutable_multiply(quaternion_t *left, const quaternion_t *right);
void quaternion_right_mutable_multiply(const quaternion_t *left, quaternion_t *right);

#endif
