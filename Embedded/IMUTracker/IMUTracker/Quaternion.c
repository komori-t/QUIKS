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

#include "Quaternion.h"
#include "Q30.h"

void quaternion_copy(const quaternion_t *src, quaternion_t *dst)
{
    dst->w.value = src->w.value;
    dst->x.value = src->x.value;
    dst->y.value = src->y.value;
    dst->z.value = src->z.value;
}

void quaternion_inverse(const quaternion_t *quat, quaternion_t *inverse)
{
    inverse->w.value = quat->w.value;
    inverse->x.value = -quat->x.value;
    inverse->y.value = -quat->y.value;
    inverse->z.value = -quat->z.value;
}

void quaternion_multiply(const quaternion_t *left, const quaternion_t *right, quaternion_t *ans)
{
    ans->w.value = multiplyQ30ByParts(left->w.upper, left->w.lower, right->w.upper, right->w.lower)
                 - multiplyQ30ByParts(left->x.upper, left->x.lower, right->x.upper, right->x.lower)
                 - multiplyQ30ByParts(left->y.upper, left->y.lower, right->y.upper, right->y.lower)
                 - multiplyQ30ByParts(left->z.upper, left->z.lower, right->z.upper, right->z.lower);
    ans->x.value = multiplyQ30ByParts(left->w.upper, left->w.lower, right->x.upper, right->x.lower)
                 + multiplyQ30ByParts(left->x.upper, left->x.lower, right->w.upper, right->w.lower)
                 + multiplyQ30ByParts(left->y.upper, left->y.lower, right->z.upper, right->z.lower)
                 - multiplyQ30ByParts(left->z.upper, left->z.lower, right->y.upper, right->y.lower);
    ans->y.value = multiplyQ30ByParts(left->w.upper, left->w.lower, right->y.upper, right->y.lower)
                 - multiplyQ30ByParts(left->x.upper, left->x.lower, right->z.upper, right->z.lower)
                 + multiplyQ30ByParts(left->y.upper, left->y.lower, right->w.upper, right->w.lower)
                 + multiplyQ30ByParts(left->z.upper, left->z.lower, right->x.upper, right->x.lower);
    ans->z.value = multiplyQ30ByParts(left->w.upper, left->w.lower, right->z.upper, right->z.lower)
                 + multiplyQ30ByParts(left->x.upper, left->x.lower, right->y.upper, right->y.lower)
                 - multiplyQ30ByParts(left->y.upper, left->y.lower, right->x.upper, right->x.lower)
                 + multiplyQ30ByParts(left->z.upper, left->z.lower, right->w.upper, right->w.lower);
}

void quaternion_left_mutable_multiply(quaternion_t *left, const quaternion_t *right)
{
    quaternion_t leftCopy;
    quaternion_copy(left, &leftCopy);
    quaternion_multiply(&leftCopy, right, left);
}

void quaternion_right_mutable_multiply(const quaternion_t *left, quaternion_t *right)
{
    quaternion_t rightCopy;
    quaternion_copy(right, &rightCopy);
    quaternion_multiply(left, &rightCopy, right);
}
