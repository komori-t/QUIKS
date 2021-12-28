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

using System;
using System.IO.Ports;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Net;
using UnityEngine;

public class Tracker {
    private SerialPort serial;
    private byte id;
    private Transform bone;
    private bool isCalibrated = false;
    private const byte PacketHeader = 0xFF;
    private Quaternion quat;

    enum CommandID {
        Ping, /* <Header> <ID> <Command_Ping> (Ack required) */
        Reply_Ack, /* <Header> <ID = 0> <Command_Reply_Ack> <1(Success)/0(Failed)> */
        Read_Quaternion, /* <Header> <ID> <Command_Read_Quaternion> */
        Reply_Quaternion, /* <Header> <ID = 0> <Command_Reply_Quaternion> <w> <x> <y> <z> (In IEEE754) */
        Set_Chip_Offset, /* <Header> <ID = 0> <Command_Set_Chip_Offset> (Ack required) */
        Set_Unity_Offset, /* <Header> <ID> <Command_Set_Offset> <w> <x> <y> <z> (In Q30) (Ack required) */
        Set_Axis, /* <Header> <ID> <Command_Set_Axis> <axis> (Ack required) */
        Set_ID, /* <Header> <ID> <Command_Set_ID> <New ID> (Ack required) */
        Flash, /* <Header> <ID> <Command_Flash> (Ack required) */
        Program, /* <Header> <ID> <Command_Program> <Number of pages> */
        Read_Compass_Accuracy, /* <Header> <ID> <Command_Read_Compass_Accuracy> */
        Reply_Compass_Accuracy, /* <Header> <ID = 0> <Command_Reply_Quaternion> <Accuracy> */
    };

    private void WriteBytesWithMasking(byte[] bytes) {
        List<byte> array = new List<byte>(bytes);
        int searchLocation = 0;
        int searchLength = array.Count;
        while (true) {
            int ffIndex = array.IndexOf(PacketHeader, searchLocation, searchLength);
            if (ffIndex < 0) {
                break;
            }
            array.Insert(ffIndex + 1, 0x00);
            searchLocation = ffIndex + 2;
            searchLength = array.Count - searchLocation;
        }
        serial.Write(array.ToArray(), 0, array.Count);
    }

    private byte ReadByte() {
        return (byte)serial.ReadByte();
    }

    private void ReadBytesWithUnmasking(byte[] bytes) {
        for (int index = 0; index < bytes.Length; ++index) {
            byte aByte = ReadByte();
            if (aByte == PacketHeader) {
                ReadByte();
            }
            bytes[index] = aByte;
        }
    }

    private void ReadHeader() {
        while (true) {
            byte header = ReadByte();
            if (header != PacketHeader) {
                continue;
            }
            byte hostID = ReadByte();
            if (hostID == 0) {
                break;
            }
        }
    }

    public Tracker(SerialPort _serial, byte _id, Transform _bone) {
        serial = _serial;
        id = _id;
        bone = _bone;
        byte[] pingPacket = new byte[] {PacketHeader, id, (byte)CommandID.Ping};
        serial.Write(pingPacket, 0, pingPacket.Length);
        byte[] ackPacket = new byte[4];
        serial.Read(ackPacket, 0, ackPacket.Length);
    }

    public static void Launch(SerialPort serial) {
        serial.Write(DMPFirmware.data, 0, DMPFirmware.data.Length);
    }

    private Quaternion ReadRotation() {
        byte[] txPacket = new byte[] {PacketHeader, id, (byte)CommandID.Read_Quaternion};
        serial.Write(txPacket, 0, txPacket.Length);
        byte[] rxData = new byte[16];
        try {
            ReadHeader();
            if (ReadByte() != (byte)CommandID.Reply_Quaternion) {
                throw new Exception("Read rotation failed");
            }
            ReadBytesWithUnmasking(rxData);
        }
        catch (TimeoutException) {
            return ReadRotation();
        }
        return new Quaternion(BitConverter.ToSingle(rxData, 4),
                              BitConverter.ToSingle(rxData, 8),
                              BitConverter.ToSingle(rxData, 12),
                              BitConverter.ToSingle(rxData, 0));
    }

    private void ReadAcknowledge() {
        ReadHeader();
        byte[] rxPacket = new byte[2];
        serial.Read(rxPacket, 0, rxPacket.Length);
        if (! (rxPacket[0] == (byte)CommandID.Reply_Ack && rxPacket[1] == 1)) {
            throw new Exception("Slave did not send acknowledge");
        }
    }

    public void SetChipOffset() {
        byte[] txPacket = new byte[] {PacketHeader, id, (byte)CommandID.Set_Chip_Offset};
        serial.Write(txPacket, 0, txPacket.Length);
        ReadAcknowledge();
    }

    public void SetUnityOffset() {
        Quaternion offset = bone.rotation;
        byte[] txHead = new byte[] {PacketHeader, id, (byte)CommandID.Set_Unity_Offset};
        serial.Write(txHead, 0, txHead.Length);
        WriteBytesWithMasking(BitConverter.GetBytes((int)(offset.w * Math.Pow(2, 30))));
        WriteBytesWithMasking(BitConverter.GetBytes((int)(offset.x * Math.Pow(2, 30))));
        WriteBytesWithMasking(BitConverter.GetBytes((int)(offset.y * Math.Pow(2, 30))));
        WriteBytesWithMasking(BitConverter.GetBytes((int)(offset.z * Math.Pow(2, 30))));
        ReadAcknowledge();
    }

    public void PrepareRotation() {
        quat = ReadRotation();
    }

    public void SetRotation() {
        bone.rotation = quat;
    }

    public bool CheckIfCalibrated() {
        if (isCalibrated) {
            return true;
        }
        byte[] txPacket = new byte[] {PacketHeader, id, (byte)CommandID.Read_Compass_Accuracy};
        serial.Write(txPacket, 0, txPacket.Length);
        try {
            ReadHeader();
            byte[] rxPacket = new byte[2];
            serial.Read(rxPacket, 0, rxPacket.Length);
            if (rxPacket[0] != (byte)CommandID.Reply_Compass_Accuracy) {
                return false;
            }
            if (rxPacket[1] >= 3) {
                isCalibrated = true;
                Debug.LogFormat("Calibration done {0}", bone);
            }
            return isCalibrated;
        }
        catch (Exception) {
        }
        return false;
    }

    public void ChangeID(byte newID) {
        byte[] txPacket = new byte[] {PacketHeader, id, (byte)CommandID.Set_ID, newID};
        serial.Write(txPacket, 0, txPacket.Length);
        ReadAcknowledge();
    }

    public void Flash() {
        byte[] txPacket = new byte[] {PacketHeader, id, (byte)CommandID.Flash};
        serial.Write(txPacket, 0, txPacket.Length);
        ReadAcknowledge();
    }
}
