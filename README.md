# QUIKS
[![license Apache 2.0](https://img.shields.io/badge/license-Apache%202-blue.svg)](http://www.apache.org/licenses/LICENSE-2.0)

QUIKS unifies inexpensive kinetic sensors



## About

QUIKS is an inexpensive motion tracking system.

It is consists of one terminal node and multiple nodes.

Each node has a sensor and the terminal node bridges PC and them.

![](https://gist.github.com/mtkrtk/2a9c5ed3c2d6123b80474edbda02dfcf/raw/fa596fe01ddd5416cb127aeafefc4b4c1cec0332/block.png)

## QUIKS is ...

### 1. Inexpensive

QUIKS is designed to construct motion tracking system at low cost.

### 2. Extendable

New node can be added by just connecting to RS485 bus.

It is easy to develop new kind of node to extend the system.

### 3. Scalable

More nodes you connect, more data you get.

So you can omit any nodes to reduce costs, areas, times, etc.

If you do not need to track the lower half of your body, you can only upper half nodes for example.



## Currently available node

### IMUTracker

The IMUTracker is a kind of node that captures rotation of one bone.

It is consists of IMU sensor ICM20948 controlled by a tiny microcontroller LPC802.

### USB terminal node

USB terminal node is a kind of terminal node that communicates with PC through USB.

It acts as virtual serial port and bridges USB and RS485 bus.

IMUTracker and USB terminal node shares the same design of circuit to reduce manufacturing costs.



## Other nodes to be added (hopefully)

### Wireless terminal node

Wireless terminal node is a kind of teminal node that communicates with PC wirelessly (maybe in 900 MHz band).

### Absolute position tracking node

If the position of one bone (typically waist) can be detected, it is possible to move around in virtual world.
