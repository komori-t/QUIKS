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

using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using System;
using System.IO.Ports;
using System.Threading;
using System.Threading.Tasks;

public class Demo : MonoBehaviour {

    enum State {
        waitLaunching,
        calibrating,
        waitOffsetting,
        offsetCounting,
        running,
    };

    private TrackerManager manager;
    private Dropdown pathPopUp;
    private Button button;
    private Text buttonTitle;
    private State state = State.waitLaunching;

    private Transform bone;

    void Start () {
        pathPopUp = FindObjectOfType<Dropdown>();
        foreach (string path in SerialPort.GetPortNames()) {
#if UNITY_STANDALONE_OSX
            if (path.IndexOf("tty.usb") >= 0) {
                pathPopUp.options.Add(new Dropdown.OptionData(path.Replace("tty", "cu")));
            }
#endif
#if UNITY_STANDALONE_WIN
            if (path.IndexOf("COM") >= 0) {
                pathPopUp.options.Add(new Dropdown.OptionData(path));
            }
#endif
#if UNITY_STANDALONE_LINUX
            if (path.IndexOf("ttyUSB") >= 0) {
                pathPopUp.options.Add(new Dropdown.OptionData(path));
            }
#endif
        }
        if (pathPopUp.options.Count > 0) {
            pathPopUp.captionText.text = pathPopUp.options[0].text;
        }

        button = FindObjectOfType<Button>();
        buttonTitle = GameObject.FindWithTag("buttonTitle").GetComponent<Text>();
        button.onClick.AddListener(ButtonDidClick);
    }

    void Update () {
        switch (state) {
            case State.calibrating:
                if (manager.CheckAccuracies()) {
                    buttonTitle.text = "Set Offset";
                    state = State.waitOffsetting;
                }
                break;

            case State.running:
                manager.SetRotations();
                break;
        }
    }

    async void ButtonDidClick() {
        switch (state) {
            case State.waitLaunching:
                manager = new TrackerManager(pathPopUp.captionText.text, FindObjectOfType<Animator>());
                manager.AddTracker(HumanBodyBones.Neck);
                manager.AddTracker(HumanBodyBones.LeftUpperArm);
                manager.AddTracker(HumanBodyBones.LeftLowerArm);
                manager.AddTracker(HumanBodyBones.RightUpperArm);
                manager.AddTracker(HumanBodyBones.RightLowerArm);
                manager.AddTracker(HumanBodyBones.Spine);
                manager.AddTracker(HumanBodyBones.LeftUpperLeg);
                manager.AddTracker(HumanBodyBones.LeftLowerLeg);
                manager.AddTracker(HumanBodyBones.RightUpperLeg);
                manager.AddTracker(HumanBodyBones.RightLowerLeg);
                manager.SetUnityOffsets();
                manager.Launch();
                buttonTitle.text = "Rotate Sensor";
                state = State.calibrating;
                break;

            case State.waitOffsetting:
            case State.running:
                state = State.offsetCounting;
                for (int i = 3; i > 0; --i) {
                    buttonTitle.text = i.ToString();
                    await Task.Delay(1000);
                }
                manager.SetChipOffsets();
                state = State.running;
                buttonTitle.text = "Set Offset";
                break;

            default:
                break;
        }
    }
}
