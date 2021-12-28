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

using UnityEngine;
using UnityEngine.UI;
using System;
using System.IO.Ports;
using System.Collections;
using System.Collections.Generic;

/**
 * The TrackerManager class manages multiple trackers.
 * In most cases, you only need to access to this class.
 */
public class TrackerManager {
    private Animator anim;
    private SerialPort serial;
    private List<Tracker> trackers = new List<Tracker>();

    /**
     * Initialize a manager instance.
     * 
     * @param path The path to the serial port that QUIKS is attatched to.
     * @param _anim An animator instance which represents a humanoid avatar to control.
     */
    public TrackerManager(string path, Animator _anim) {
        anim = _anim;
        serial = new SerialPort(path, 460800, Parity.None, 8, StopBits.One);
        serial.ReadTimeout = 100;
        serial.Open();
    }

    /**
     * Add a tracker.
     *
     * @param bone A HumanBodyBones constant which specifies the bone that tracker will control.
     *
     * @returns A boolean value which describes whether the operation is succeeded.
     */
    public bool AddTracker(HumanBodyBones bone) {
        try {
            trackers.Add(new Tracker(serial, (byte)((byte)bone + 1), anim.GetBoneTransform(bone)));
            Debug.LogFormat("Add tracker: {0}", bone);
            return true;
        }
        catch (TimeoutException) {
            return false;
        }
    }

    /**
     * Launch all trackers.
     * You must call this method before start tracking.
     *
     * @note
     * This method uploads DMP firmware to all trackers.
     * Even if you do not add some tracker, those trackers will receive the firmware.
     */
    public void Launch() {
        Tracker.Launch(serial);
    }

    /**
     * Set the IMU chip frame offset.
     * This offset initializes the position of the human in real world.
     *
     * @note
     * You may call this method multiple times to reset sensor position.
     *
     * @note
     * This method raises an exception if the operation is failed.
     */
    public void SetChipOffsets() {
        foreach (var tracker in trackers) {
            tracker.SetChipOffset();
        }
    }

    /**
     * Set the Unity frame offset.
     * This offset initializes the position of the bone in virtual world.
     * You must call this method once before start tracking.
     *
     * @note
     * A quaternion \f$ q' \f$ is assigned to the rotation of a bone.
     * \f$ q' \f$ is obtained by following formular.
     * \f$ q' = A \bar{q}_\mathrm{chip} q q_\mathrm{unity} \f$
     * where
     * \f$ q \f$ is a quaternion which the IMU sensor measured.
     * \f$ A \f$ is a transformation matrix which represents the transformation from real world to Unity world.
     * \f$ \bar{q}_\mathrm{chip} \f$ is a quaternion which is an inverse of \f$ q_\mathrm{chip} \f$
     * \f$ q_\mathrm{chip} \f$ is set to \f$ q \f$ when SetChipOffsets() is called.
     * \f$ q_\mathrm{unity} \f$ is the initial transform value of bone which is initialized by SetUnityOffsets().
     */
    public void SetUnityOffsets() {
        foreach (var tracker in trackers) {
            tracker.SetUnityOffset();
        }
    }

    /**
     * Communicate with sensors to obtain rotations and put them into the buffer.
     * You should call this method from a background thread,
     * and then the main thread should call SetRotations().
     *
     * @note
     * This method raises an exception if the operation is failed.
     */
    public void PrepareRotations() {
        foreach (var tracker in trackers) {
            tracker.PrepareRotation();
        }
    }

    /**
     * Assign all the rotations of added trackers to the bones.
     * You call this method periodically to achive tracking.
     */
    public void SetRotations() {
        foreach (var tracker in trackers) {
            tracker.SetRotation();
        }
    }

    /**
     * Check if all sensors are calibrated.
     * You should not start tracking before this method returns true.
     *
     * @returns A boolean value which represents whether the sensors are calibrated.
     */
    public bool CheckAccuracies() {
        foreach (var tracker in trackers) {
            if (! tracker.CheckIfCalibrated()) {
                return false;
            }
        }
        return true;
    }
}
