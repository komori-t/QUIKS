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

#ifndef __Protocol__
#define __Protocol__

#define PACKET_HEADER 0xFF
#define DMP_UPLOAD_ID 0xFE

typedef enum {
    Command_Ping, /* <Header> <ID> <Command_Ping> (Ack required) */
    Command_Reply_Ack, /* <Header> <ID = 0> <Command_Reply_Ack> <1(Success)/0(Failed)> */
    Command_Read_Quaternion, /* <Header> <ID> <Command_Read_Quaternion> */
    Command_Reply_Quaternion, /* <Header> <ID = 0> <Command_Reply_Quaternion> <w> <x> <y> <z> (In IEEE754) */
    Command_Set_Chip_Offset, /* <Header> <ID = 0> <Command_Set_Chip_Offset> (Ack required) */
    Command_Set_Unity_Offset, /* <Header> <ID> <Command_Set_Offset> <w> <x> <y> <z> (In Q30) (Ack required) */
    Command_Set_Axis, /* <Header> <ID> <Command_Set_Axis> <axis> (Ack required) */
    /* specification of axis */
    /* _____________________________________________________ */
    /* Bit  |    0   |   1, 2  |    3   |   4, 5  |    6   | */
    /* Data | X sign | X index | Y sign | Y index | Z sign | */
    /* ----------------------------------------------------- */
    /* sign: 0 stands for +, 1 stands for - */
    /* Z index = 3 - X index - Y index */
    Command_Set_ID, /* <Header> <ID> <Command_Set_ID> <New ID> (Ack required) */
    Command_Flash, /* <Header> <ID> <Command_Flash> (Ack required) */
    Command_Program, /* <Header> <ID> <Command_Program> <Number of pages> */
    Command_Read_Compass_Accuracy, /* <Header> <ID> <Command_Read_Compass_Accuracy> */
    Command_Reply_Compass_Accuracy, /* <Header> <ID = 0> <Command_Reply_Quaternion> <Accuracy> */
} command_id_t;

#endif
