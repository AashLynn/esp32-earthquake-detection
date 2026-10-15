
---

# Real-Time ESP32 Earthquake Detection & Seismic Alarm System

An embedded, real-time digital signal processing (DSP) seismic monitoring system built on the ESP32 microcontroller. The system samples 3-axis acceleration data from an MPU6050 accelerometer, isolates dynamic ground motion via dynamic gravity removal and a 4th-order Butterworth bandpass filter, and converts buffered time-series signals into the frequency domain using a Radix-2 Fast Fourier Transform (FFT).

Event severity is classified using **Spectral RMS** accumulated across positive frequency bins ($0.78\text{ Hz} - 14.84\text{ Hz}$) and mapped to an SSD1306 OLED display, a Common Anode RGB LED, and a high-pitched acoustic alarm buzzer.

---

## Key Features

* **3-Axis Acceleration Processing:** Dynamically extracts dynamic ground acceleration by removing calibrated stationary baselines ($g_x, g_y, g_z$).
* **0.5 – 15 Hz Butterworth Bandpass Filtering:** Implements a cascaded Biquad IIR filter designed at $100\text{ Hz}$ sampling frequency to reject high-frequency mechanical noise and low-frequency DC drift.
* **Radix-2 FFT & Windowing:** Applies a 128-sample Hamming window ($1.28\text{ s}$ buffer) followed by an in-place Radix-2 Real FFT algorithm per axis.
* **Spectral RMS Computation:** Calculates the root-mean-square energy across frequency bins 1 to 19 ($0.78125\text{ Hz}$ to $14.84375\text{ Hz}$), matching structural resonance bands.
* **Visual & Acoustic Alert Pipeline:**
* **0.96" SSD1306 OLED Display:** Shows live Spectral RMS values and severity status.
* **Common Anode RGB LED:** Color-coded status updates (Green, Yellow, Red).
* **Piezo Buzzer:** Variable-frequency tone generator ($800\text{ Hz}$ soft pulse for Moderate; $2800\text{ Hz}$ sharp alarm for Extreme).


* **Serial Plotter Compatible:** Streams real-time raw RMS, filtered RMS, spectral RMS, and per-bin spectral magnitudes (`FFT_X`, `FFT_Y`, `FFT_Z`) formatted for the Arduino Serial Plotter.

---

## Hardware Architecture & Pinout

| Component | Component Pin | ESP32 Pin | Logic / Function |
| --- | --- | --- | --- |
| **MPU6050 Accelerometer** | VCC | **3.3V** | System Power |
|  | GND | **GND** | Ground |
|  | SDA | **GPIO 21** | I2C Data ($400\text{ kHz}$) |
|  | SCL | **GPIO 22** | I2C Clock ($400\text{ kHz}$) |
| **0.96" OLED (SSD1306)** | VCC | **3.3V** | System Power |
|  | GND | **GND** | Ground |
|  | SDA | **GPIO 21** | Shared I2C Bus |
|  | SCL | **GPIO 22** | Shared I2C Bus |
| **Common Anode RGB LED** | Common Anode (+) | **3.3V** | Shared Positive Anode Rail |
|  | Red Cathode (-) | **GPIO 17 (TX2)** | Active-LOW Control ($220\,\Omega$ resistor) |
|  | Green Cathode (-) | **GPIO 16 (RX2)** | Active-LOW Control ($220\,\Omega$ resistor) |
|  | Blue Cathode (-) | **GPIO 5 (D5)** | Active-LOW Control ($220\,\Omega$ resistor) |
| **Piezo Buzzer** | Positive (+) | **GPIO 18 (D18)** | PWM / Tone Frequency Driver |
|  | Negative (-) | **GND** | Ground |

---

## Seismic Severity Scale & Alarm Logic

Classifications are evaluated based on the **Spectral RMS** scalar calculated across the frequency spectrum ($0.78\text{ Hz} - 14.84\text{ Hz}$):

| Severity Level | Spectral RMS Range | OLED Banner | RGB LED Color | Buzzer Tone |
| --- | --- | --- | --- | --- |
| **Normal / Quiet** | $\text{RMS} < 0.12$ | `NORMAL` | **GREEN** | OFF |
| **Mild Event** | $0.12 \le \text{RMS} < 2.00$ | `MILD` | **GREEN** | OFF |
| **Moderate Earthquake** | $2.00 \le \text{RMS} < 5.00$ | `MODERATE` | **YELLOW** (Red + Green) | Soft Beep ($800\text{ Hz}$, $80\text{ ms}$) |
| **Extreme Earthquake** | $\text{RMS} \ge 5.00$ | `EXTREME!!` | **RED** | High Alarm Tone ($2800\text{ Hz}$, $500\text{ ms}$) |

---

## Digital Signal Processing Pipeline

```
[ Raw Accelerometer Input (ax, ay, az) ]
                   │
                   ▼
[ Static Gravity Baseline Subtraction ]
                   │
                   ▼
[ 0.5 - 15 Hz Biquad Butterworth Bandpass Filter ]
                   │
                   ▼
[ 128-Sample Sliding Time Buffer (1.28 s at 100 Hz) ]
                   │
                   ▼
[ Hamming Window Multiplication ]
                   │
                   ▼
[ Radix-2 Complex FFT (Bins 1..19 Extraction) ]
                   │
                   ▼
[ 3-Axis Combined Spectral RMS Calculation ]
                   │
                   ▼
[ Severity Classification -> OLED / RGB LED / Buzzer Alarm ]

```

---

## Software Prerequisites & Dependencies

### Arduino IDE Libraries

Install the following libraries via the **Arduino Library Manager** (`Ctrl+Shift+I`):

1. **Adafruit SSD1306** (by Adafruit)
2. **Adafruit GFX Library** (by Adafruit)
3. **Wire** (Built-in ESP32 core)

### Board Manager

Ensure the **ESP32 Board Package** by Expressif Systems is installed in your Arduino IDE:

* Board: `ESP32 Dev Module`
* CPU Frequency: `240MHz`
* Flash Frequency: `80MHz`
* Upload Speed: `115200` baud

---

## Installation & Setup

1. **Wiring:** Wire the hardware components according to the [Hardware Architecture & Pinout](https://www.google.com/search?q=%23hardware-architecture--pinout) table.
2. **Calibrate Baseline:** Place your MPU6050 flat on a stationary surface. Update the baseline constants in the code if your stationary offsets differ:
```cpp
const float BASELINE_X = 0.06287075f;
const float BASELINE_Y = 0.01594543f;
const float BASELINE_Z = 1.02508970f;

```


3. **Flash Code:** Open the C++ sketch in Arduino IDE, select your ESP32 board and COM port, and click **Upload**.
4. **Monitor Stream:**
* Open **Serial Monitor** at `115200` baud to observe status logging.
* Open **Serial Plotter** (`Ctrl+Shift+L`) to view live real-time graphs for `Raw_RMS`, `Filtered_RMS`, `Spectral_RMS`, and `FFT_X/Y/Z`.