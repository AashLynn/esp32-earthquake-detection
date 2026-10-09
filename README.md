# ESP32-Based Earthquake Detection and Alarm System

## Overview

This project explores a low-cost earthquake detection system using an ESP32 microcontroller and an MPU6050 accelerometer. Reference earthquake waveforms from the Caltech Community Seismic Network are used to investigate signal behaviour and develop a suitable detection pipeline.

## Hardware

* ESP32 development board
* MPU6050 accelerometer
* Arduino IDE

## Development Progress

### 1. Sensor Calibration

* Collected stationary accelerometer data.
* Calculated axis-wise baseline offsets and noise statistics.
* Implemented baseline correction in the ESP32 code.

### 2. Dynamic Acceleration

* Converted raw accelerometer readings into g.
* Subtracted calibrated offsets from the three axes.
* Calculated the three-axis dynamic acceleration magnitude.

### 3. Reference Dataset Analysis

* Inspected SAC waveform files from the La Cañada M1.7, Indio M4.9, and Mexico M8.1 events.
* Selected two nearby stations per event for initial analysis.
* Compared 0.5 s, 1.0 s, and 1.5 s analysis windows.
* Selected 1.5 s as the preferred candidate based on the observed RMS behaviour.

### 4. Initial FIR Filtering

* Implemented a five-tap moving-average FIR filter on the three corrected acceleration axes.
* Compared raw and filtered acceleration magnitudes using the Arduino Serial Plotter.
* Tested the response to stationary conditions, movement, and sharp taps.

## Current Status

The calibration, dynamic acceleration, timing-window analysis, and initial moving-average filtering stages have been explored. The next stage is to design and evaluate a frequency-selective FIR filter using the reference seismic waveforms.

## Limitations

This is an experimental student project, not a certified earthquake early-warning system. Performance and detection thresholds require further validation.
