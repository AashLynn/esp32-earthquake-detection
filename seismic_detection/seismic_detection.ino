#include <Wire.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// OLED CONFIGURATION
// =====================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDR     0x3C  // Default I2C Address for SSD1306

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =====================================================
// RGB LED & BUZZER HARDWARE CONFIGURATION
// =====================================================
const uint8_t LED_RED_PIN   = 17; // Red channel on TX2 (GPIO 17)
const uint8_t LED_GREEN_PIN = 16; // Green channel moved to D19 (GPIO 19)
const uint8_t LED_BLUE_PIN  = 5;  // Blue channel on D5 (GPIO 5)

const uint8_t BUZZER_PIN    = 18; // Piezo Buzzer on D18 (GPIO 18)

// =====================================================
// MPU6050 CONFIGURATION
// =====================================================
const uint8_t MPU_ADDR = 0x68;
const uint8_t REG_PWR_MGMT_1 = 0x6B;
const uint8_t REG_SMPLRT_DIV = 0x19;
const uint8_t REG_CONFIG = 0x1A;
const uint8_t REG_ACCEL_CONFIG = 0x1C;
const uint8_t REG_ACCEL_XOUT_H = 0x3B;

const uint32_t SAMPLE_INTERVAL_US = 10000;
const float ACCEL_SENSITIVITY = 16384.0f;

// Calibrated stationary baselines, in g
const float BASELINE_X = 0.06287075f;
const float BASELINE_Y = 0.01594543f;
const float BASELINE_Z = 1.02508970f;

// 128 samples at nominal 100 Hz = 1.28 seconds
const int WINDOW_SAMPLES = 128;
const float SAMPLING_FREQUENCY = 100.0f;

// Positive-frequency bins 1..19 cover 0.78125 Hz through 14.84375 Hz
const int FIRST_FFT_BIN = 1;
const int LAST_FFT_BIN = 19;

// =====================================================
// SEISMIC THRESHOLDS (SPECTRAL RMS SCALE)
// =====================================================
const float SPECTRAL_T_MILD     = 0.12f;  // Noise floor ceiling
const float SPECTRAL_T_MODERATE = 2.00f;  // Mild to Moderate boundary
const float SPECTRAL_T_EXTREME  = 5.00f;  // Moderate to Extreme boundary

int windowCount = 0;

float rawSquareSum = 0.0f;
float filteredSquareSum = 0.0f;

float filteredXBuffer[WINDOW_SAMPLES];
float filteredYBuffer[WINDOW_SAMPLES];
float filteredZBuffer[WINDOW_SAMPLES];

// FFT work arrays
float realX[WINDOW_SAMPLES], imagX[WINDOW_SAMPLES];
float realY[WINDOW_SAMPLES], imagY[WINDOW_SAMPLES];
float realZ[WINDOW_SAMPLES], imagZ[WINDOW_SAMPLES];

// =====================================================
// BUTTERWORTH FILTER: 0.5-15 Hz at 100 Hz
// =====================================================
struct BiquadState {
  float x1 = 0.0f, x2 = 0.0f;
  float y1 = 0.0f, y2 = 0.0f;
};

struct AxisFilterState {
  BiquadState section1;
  BiquadState section2;
};

AxisFilterState filterX, filterY, filterZ;

const float b0_1 =  0.12416982f;
const float b1_1 =  0.24833964f;
const float b2_1 =  0.12416982f;
const float a1_1 = -0.78647598f;
const float a2_1 =  0.29590836f;

const float b0_2 =  1.00000000f;
const float b1_2 = -2.00000000f;
const float b2_2 =  1.00000000f;
const float a1_2 = -1.95569325f;
const float a2_2 =  0.95671787f;

// =====================================================
// I2C HELPERS
// =====================================================
bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readAccelerometer(int16_t &ax, int16_t &ay, int16_t &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_ACCEL_XOUT_H);

  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)6) != 6) return false;

  ax = (int16_t)((Wire.read() << 8) | Wire.read());
  ay = (int16_t)((Wire.read() << 8) | Wire.read());
  az = (int16_t)((Wire.read() << 8) | Wire.read());

  return true;
}

// =====================================================
// BIQUAD FILTER & FFT FUNCTIONS
// =====================================================
float processBiquad(float input, BiquadState &s, float b0, float b1, float b2, float a1, float a2) {
  float output = b0 * input + b1 * s.x1 + b2 * s.x2 - a1 * s.y1 - a2 * s.y2;
  s.x2 = s.x1; s.x1 = input; s.y2 = s.y1; s.y1 = output;
  return output;
}

float filterAxis(float input, AxisFilterState &state) {
  float stage1 = processBiquad(input, state.section1, b0_1, b1_1, b2_1, a1_1, a2_1);
  return processBiquad(stage1, state.section2, b0_2, b1_2, b2_2, a1_2, a2_2);
}

float hammingWindow(int n) {
  return 0.54f - 0.46f * cosf(2.0f * PI * n / (WINDOW_SAMPLES - 1));
}

void calculateFFT(float real[], float imag[]) {
  int j = 0;
  for (int i = 1; i < WINDOW_SAMPLES; i++) {
    int bit = WINDOW_SAMPLES >> 1;
    while (j & bit) { j ^= bit; bit >>= 1; }
    j ^= bit;
    if (i < j) {
      float temp = real[i]; real[i] = real[j]; real[j] = temp;
      temp = imag[i]; imag[i] = imag[j]; imag[j] = temp;
    }
  }

  for (int length = 2; length <= WINDOW_SAMPLES; length <<= 1) {
    float angle = -2.0f * PI / length;
    float wLenReal = cosf(angle), wLenImag = sinf(angle);
    for (int start = 0; start < WINDOW_SAMPLES; start += length) {
      float wReal = 1.0f, wImag = 0.0f;
      int half = length / 2;
      for (int k = 0; k < half; k++) {
        int even = start + k; int odd = even + half;
        float oddReal = real[odd] * wReal - imag[odd] * wImag;
        float oddImag = real[odd] * wImag + imag[odd] * wReal;
        float evenReal = real[even], evenImag = imag[even];

        real[even] = evenReal + oddReal; imag[even] = evenImag + oddImag;
        real[odd] = evenReal - oddReal;   imag[odd] = evenImag - oddImag;

        float nextWReal = wReal * wLenReal - wImag * wLenImag;
        float nextWImag = wReal * wLenImag + wImag * wLenReal;
        wReal = nextWReal; wImag = nextWImag;
      }
    }
  }
}

void prepareFFT(const float input[], float real[], float imag[]) {
  for (int i = 0; i < WINDOW_SAMPLES; i++) {
    real[i] = input[i] * hammingWindow(i);
    imag[i] = 0.0f;
  }
  calculateFFT(real, imag);
}

float fftMagnitude(float real[], float imag[], int bin) {
  return sqrtf(real[bin] * real[bin] + imag[bin] * imag[bin]);
}

// =====================================================
// COMMON ANODE RGB LED CONTROL (Active-LOW)
// =====================================================
void setRGBColor(bool redState, bool greenState, bool blueState) {
  digitalWrite(LED_RED_PIN,   redState   ? LOW : HIGH);
  digitalWrite(LED_GREEN_PIN, greenState ? LOW : HIGH);
  digitalWrite(LED_BLUE_PIN,  blueState  ? LOW : HIGH);
}

// =====================================================
// OLED, LED & BUZZER ALARM DISPLAY LOGIC
// =====================================================
void updateUIAndIndicators(float specRMS) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SEISMIC MONITOR");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  // Spectral RMS Value
  display.setCursor(0, 16);
  display.print("Spec RMS: ");
  display.print(specRMS, 3);

  // Event Severity Banner, RGB LED & Buzzer Control
  display.setCursor(0, 32);
  display.setTextSize(2);

  if (specRMS < SPECTRAL_T_MODERATE) {
    // Normal / Ambient Noise & Mild Earthquakes -> GREEN, Buzzer OFF
    display.println(specRMS < SPECTRAL_T_MILD ? "NORMAL" : "MILD");
    setRGBColor(false, true, false); // Green ON
    noTone(BUZZER_PIN);             // Silence Buzzer
  } 
  else if (specRMS < SPECTRAL_T_EXTREME) {
    // Moderate Earthquake -> YELLOW (Red + Green), Soft Short Beep
    display.println("MODERATE");
    setRGBColor(true, true, false);  // Red ON + Green ON = Yellow
    tone(BUZZER_PIN, 800, 80);       // Soft 800Hz beep on GPIO 18
  } 
  else {
    // Extreme / Severe Earthquake -> RED, High-Pitched Continuous Alarm
    display.println("EXTREME!!");
    setRGBColor(true, false, false); // Red ON
    tone(BUZZER_PIN, 2800, 500);     // Sharp 2.8kHz warning tone on GPIO 18
  }

  display.display();
}

// =====================================================
// WINDOW PROCESSING & OUTPUT STREAMING
// =====================================================
void processCompletedWindow() {
  float rawRMS = sqrtf(rawSquareSum / WINDOW_SAMPLES);
  float filteredRMS = sqrtf(filteredSquareSum / WINDOW_SAMPLES);

  prepareFFT(filteredXBuffer, realX, imagX);
  prepareFFT(filteredYBuffer, realY, imagY);
  prepareFFT(filteredZBuffer, realZ, imagZ);

  float sumSquaredSpectralMag = 0.0f;
  int binCount = (LAST_FFT_BIN - FIRST_FFT_BIN + 1);

  for (int bin = FIRST_FFT_BIN; bin <= LAST_FFT_BIN; bin++) {
    float magX = fftMagnitude(realX, imagX, bin);
    float magY = fftMagnitude(realY, imagY, bin);
    float magZ = fftMagnitude(realZ, imagZ, bin);

    sumSquaredSpectralMag += (magX * magX + magY * magY + magZ * magZ);
  }

  float spectralRMS = sqrtf(sumSquaredSpectralMag / binCount);

  // Update OLED display, RGB LED color, and Buzzer state
  updateUIAndIndicators(spectralRMS);

  // Stream each bin to Serial Monitor & Plotter
  for (int bin = FIRST_FFT_BIN; bin <= LAST_FFT_BIN; bin++) {
    float magX = fftMagnitude(realX, imagX, bin);
    float magY = fftMagnitude(realY, imagY, bin);
    float magZ = fftMagnitude(realZ, imagZ, bin);

    Serial.print("Raw_RMS:");      Serial.print(rawRMS, 6);
    Serial.print(",Filtered_RMS:"); Serial.print(filteredRMS, 6);
    Serial.print(",Spectral_RMS:"); Serial.print(spectralRMS, 6);
    Serial.print(",FFT_X:");       Serial.print(magX, 6);
    Serial.print(",FFT_Y:");       Serial.print(magY, 6);
    Serial.print(",FFT_Z:");       Serial.println(magZ, 6);
  }

  rawSquareSum = 0.0f;
  filteredSquareSum = 0.0f;
  windowCount = 0;
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  // Initialize RGB LED Pins & Buzzer Pin
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Set default state to GREEN and silence buzzer on boot
  setRGBColor(false, true, false);
  noTone(BUZZER_PIN);

  Wire.begin(21, 22); // I2C SDA=21, SCL=22
  Wire.setClock(400000);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("ERROR, OLED initialization failed");
    while (true) delay(100);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 25);
  display.println("SYSTEM INITIALIZING");
  display.display();

  // Initialize MPU6050
  if (!writeRegister(REG_PWR_MGMT_1, 0x00)) {
    Serial.println("ERROR, MPU6050 not responding");
    while (true) delay(100);
  }

  delay(100);

  if (!writeRegister(REG_CONFIG, 0x04) ||
      !writeRegister(REG_SMPLRT_DIV, 9) ||
      !writeRegister(REG_ACCEL_CONFIG, 0x00)) {
    Serial.println("ERROR, MPU6050 configuration failed");
    while (true) delay(100);
  }

  Serial.println("STATUS,START");
}

// =====================================================
// MAIN LOOP
// =====================================================
void loop() {
  static uint32_t nextSampleUs = micros();
  nextSampleUs += SAMPLE_INTERVAL_US;

  int16_t rawX, rawY, rawZ;

  if (readAccelerometer(rawX, rawY, rawZ)) {
    float axG = rawX / ACCEL_SENSITIVITY;
    float ayG = rawY / ACCEL_SENSITIVITY;
    float azG = rawZ / ACCEL_SENSITIVITY;

    float axDynamic = axG - BASELINE_X;
    float ayDynamic = ayG - BASELINE_Y;
    float azDynamic = azG - BASELINE_Z;

    float axFiltered = filterAxis(axDynamic, filterX);
    float ayFiltered = filterAxis(ayDynamic, filterY);
    float azFiltered = filterAxis(azDynamic, filterZ);

    rawSquareSum += axDynamic * axDynamic + ayDynamic * ayDynamic + azDynamic * azDynamic;
    filteredSquareSum += axFiltered * axFiltered + ayFiltered * ayFiltered + azFiltered * azFiltered;

    filteredXBuffer[windowCount] = axFiltered;
    filteredYBuffer[windowCount] = ayFiltered;
    filteredZBuffer[windowCount] = azFiltered;

    windowCount++;

    if (windowCount >= WINDOW_SAMPLES) {
      processCompletedWindow();
    }
  }

  while ((int32_t)(micros() - nextSampleUs) < 0) {
    // Wait for nominal sample deadline
  }

  if ((int32_t)(micros() - nextSampleUs) >= (int32_t)SAMPLE_INTERVAL_US) {
    nextSampleUs = micros();
  }
}