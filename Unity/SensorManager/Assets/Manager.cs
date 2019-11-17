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
using UnityEngine;
using UnityEngine.UI;

public class Manager : MonoBehaviour
{
    enum State {
        waitConnecting,
        waitFlashing,
    };

    private InputField idField;
    private Dropdown bonePopUp;
    private Dropdown pathPopUp;
    private Button button;
    private Text buttonTitle;
    private Text statusLabel;
    private SerialPort serial;
    private State state = State.waitConnecting;
    private byte id;

    void Start() {
        idField = GameObject.FindWithTag("idField").GetComponent<InputField>();
        bonePopUp = GameObject.FindWithTag("bonePopUp").GetComponent<Dropdown>();
        pathPopUp = GameObject.FindWithTag("pathPopUp").GetComponent<Dropdown>();
        button = GameObject.FindWithTag("button").GetComponent<Button>();
        buttonTitle = GameObject.FindWithTag("buttonTitle").GetComponent<Text>();
        bonePopUp.onValueChanged.AddListener(delegate { boneDidChange(); });
        button.onClick.AddListener(buttonDidClick);
        statusLabel = GameObject.FindWithTag("statusLabel").GetComponent<Text>();

        foreach (string path in SerialPort.GetPortNames()) {
            if (path.IndexOf("tty.usb") >= 0) {
                pathPopUp.options.Add(new Dropdown.OptionData(path.Replace("tty", "cu")));
            }
        }
        if (pathPopUp.options.Count > 0) {
            pathPopUp.captionText.text = pathPopUp.options[0].text;
        }
    }

    void boneDidChange() {
        if (state == State.waitFlashing) {
            byte newID;
            HumanBodyBones bone;
            switch (bonePopUp.value) {
                case 0:
                    bone = HumanBodyBones.Neck;
                    break;

                case 1:
                    bone = HumanBodyBones.LeftUpperArm;
                    break;

                case 2:
                    bone = HumanBodyBones.LeftLowerArm;
                    break;

                case 3:
                    bone = HumanBodyBones.RightUpperArm;
                    break;

                case 4:
                    bone = HumanBodyBones.RightLowerArm;
                    break;

                case 5:
                    bone = HumanBodyBones.Spine;
                    break;

                case 6:
                    bone = HumanBodyBones.LeftUpperLeg;
                    break;

                case 7:
                    bone = HumanBodyBones.LeftLowerLeg;
                    break;

                case 8:
                    bone = HumanBodyBones.RightUpperLeg;
                    break;

                case 9:
                    bone = HumanBodyBones.RightLowerLeg;
                    break;

                default:
                    return;
            }
            newID = (byte)((byte)bone + 1);

            byte[] idTxPacket = new byte[] {0xFF, id, 7, newID};
            serial.Write(idTxPacket, 0, idTxPacket.Length);
            byte[] rxPacket = new byte[4];
            serial.Read(rxPacket, 0, rxPacket.Length);
            id = newID;
            idField.text = id.ToString();

            int xSign, xIndex, ySign, yIndex, zSign;
            switch (bone) {
                case HumanBodyBones.Neck:
                    xSign  = 0;
                    xIndex = 0;
                    ySign  = 0;
                    yIndex = 1;
                    zSign  = 0;
                    break;

                case HumanBodyBones.LeftUpperArm:
                case HumanBodyBones.LeftLowerArm:
                case HumanBodyBones.RightUpperArm:
                case HumanBodyBones.RightLowerArm:
                    /* x' = -y, y' = -z, z' = x */
                    xSign  = 1;
                    xIndex = 1;
                    ySign  = 1;
                    yIndex = 2;
                    zSign  = 0;
                    break;

                case HumanBodyBones.Spine:
                    /* x' = -y, y' = x, z' = z */
                    xSign  = 1;
                    xIndex = 1;
                    ySign  = 0;
                    yIndex = 0;
                    zSign  = 0;
                    break;

                case HumanBodyBones.LeftUpperLeg:
                case HumanBodyBones.LeftLowerLeg:
                    /* x' = z, y' = -y, z' = x */
                    xSign  = 0;
                    xIndex = 2;
                    ySign  = 1;
                    yIndex = 1;
                    zSign  = 0;
                    break;

                case HumanBodyBones.RightUpperLeg:
                case HumanBodyBones.RightLowerLeg:
                    /* x' = -z, y' = -y, z' = -x */
                    xSign  = 1;
                    xIndex = 2;
                    ySign  = 1;
                    yIndex = 1;
                    zSign  = 1;
                    break;

                default:
                    return;
            }

            int axisData = (xSign  << 0)
                         | (xIndex << 1)
                         | (ySign  << 3)
                         | (yIndex << 4)
                         | (zSign  << 6);
            byte[] axisTxPacket = new byte[] {0xFF, id, 6, (byte)axisData};
            serial.Write(axisTxPacket, 0, axisTxPacket.Length);
            serial.Read(rxPacket, 0, rxPacket.Length);
            statusLabel.text = "ID, axis changed";
        }
    }

    void buttonDidClick() {
        if (state == State.waitConnecting) {
            serial = new SerialPort(pathPopUp.captionText.text, 460800, Parity.None, 8, StopBits.One);
            serial.ReadTimeout = 50;
            serial.Open();
            state = State.waitFlashing;
            id = byte.Parse(idField.text);
            if (id == 0) {
                for (id = 1; id < 0xFF; ++id) {
                    byte[] pingPacket = new byte[] {0xFF, id, 0};
                    serial.Write(pingPacket, 0, pingPacket.Length);
                    try {
                        byte[] ackPacket = new byte[4];
                        serial.Read(ackPacket, 0, ackPacket.Length);
                    }
                    catch (TimeoutException) {
                        continue;
                    }
                    break;
                }
                if (id == 0xFF) {
                    throw new Exception("No device found");
                }
            }
            buttonTitle.text = "Flash";
            statusLabel.text = "Connected";
            boneDidChange();
        } else {
            byte[] txPacket = new byte[] {0xFF, id, 8};
            serial.Write(txPacket, 0, txPacket.Length);
            byte[] rxPacket = new byte[4];
            serial.Read(rxPacket, 0, rxPacket.Length);
            statusLabel.text = "Flashed";
        }
    }
}
