ParkingHelper - an ultrasonic distance detector-based visual parking aid

This project implements a small device that can be mounted on the wall of your garage
in order to help you park your vehicle a desired distance from the wall. I created
this because my Ford F-150 pickup truck just barely fits in the garage, and I wanted
something to help me park it correctly consistently. 

The device consists of an ATTiny85 (Atmel AVR 8-bit) microcontroller that drives a
Parallax Ping))) distance sensor (part #28015-RT), and a small, 4-LED LPD-8806 driven
RGB LED strip

The LPD-8806 code is a modified version of the AdaFruit library. It has been modified
to work with the ATTiny85 microcontroller, to minimize the memory footprint, and to
incoporate some nice features that I found in another LED driver library (FastSPI_LED2).

This repository contains:

  - Source code for the ATTiny85 microcontroller that drives the device
  - Schematic for the hardware
  - Eagle PCB layout for the hardware

