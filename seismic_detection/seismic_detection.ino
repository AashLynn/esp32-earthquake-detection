
#include <Wire.h>
#include <math.h>

// ============================================================
// MPU6050 REGISTERS
// ============================================================
#define MPU6050_ADDR  0x68
#define PWR_MGMT_1    0x6B
#define ACCEL_CONFIG  0x1C
#define ACCEL_XOUT_H  0x3B
const float ACCEL_SENSITIVITY = 16384.0;  // +/-2g

// ============================================================
// CALIBRATED BASELINE OFFSETS (g)
// ============================================================

const float BASELINE_X = 0.06287075;
const float BASELINE_Y = 0.01594543;
const float BASELINE_Z = 1.02508970;

// ============================================================
// SAMPLING: APPROXIMATELY 100 Hz
// ============================================================

const unsigned long SAMPLE_INTERVAL_US = 10000;
unsigned long previousSampleTime = 0;

// ============================================================
// 5-TAP MOVING-AVERAGE FIR FILTER
// ============================================================

const int FIR_TAPS = 5;

float bufferX[FIR_TAPS] = {0};
float bufferY[FIR_TAPS] = {0};
float bufferZ[FIR_TAPS] = {0};

float sumX = 0.0;
float sumY = 0.0;
float sumZ = 0.0;

int bufferIndex = 0;
int samplesCollected = 0;

// ============================================================
// WRITE MPU6050 REGISTER
// ============================================================

void writeRegister(byte reg, byte value)
{
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

// ============================================================
// READ RAW ACCELERATION
// ============================================================

bool readAccelerometer(int16_t &ax, int16_t &ay, int16_t &az)
{
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(ACCEL_XOUT_H);

  if (Wire.endTransmission(false) != 0)
    return false;

  if (Wire.requestFrom(MPU6050_ADDR, 6, true) != 6)
    return false;

  ax = (int16_t)((Wire.read() << 8) | Wire.read());
  ay = (int16_t)((Wire.read() << 8) | Wire.read());
  az = (int16_t)((Wire.read() << 8) | Wire.read());

  return true;
}

// ============================================================
// FIR FILTER: UPDATE ONE SAMPLE ON EACH AXIS
// ============================================================

void applyFIR(float x, float y, float z,
              float &filteredX,
              float &filteredY,
              float &filteredZ)
{
  // Remove the oldest sample from the running sums
  sumX -= bufferX[bufferIndex];
  sumY -= bufferY[bufferIndex];
  sumZ -= bufferZ[bufferIndex];

  // Insert the new sample
  bufferX[bufferIndex] = x;
  bufferY[bufferIndex] = y;
  bufferZ[bufferIndex] = z;

  sumX += x;
  sumY += y;
  sumZ += z;

  // Advance circular-buffer index
  bufferIndex = (bufferIndex + 1) % FIR_TAPS;

  if (samplesCollected < FIR_TAPS)
    samplesCollected++;

  // Average available samples during startup.
  // After five samples, this is a full 5-tap FIR.
  filteredX = sumX / samplesCollected;
  filteredY = sumY / samplesCollected;
  filteredZ = sumZ / samplesCollected;
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);
  Wire.begin(21, 22);
  Wire.setClock(400000);

  delay(100);

  // Wake up MPU6050
  writeRegister(PWR_MGMT_1, 0x00);

  // Accelerometer range: +/-2g
  writeRegister(ACCEL_CONFIG, 0x00);

  delay(100);

  Serial.println("MPU6050 FIR FILTER TEST");
  Serial.println("Sampling target: 100 Hz");
  Serial.println("FIR taps: 5");
  Serial.println("Columns are acceleration magnitudes in g");

  // Arduino Serial Plotter reads labelled tab-separated series
  Serial.println("A_raw:0\tA_FIR:0");

  previousSampleTime = micros();
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  unsigned long currentTime = micros();

  if ((unsigned long)(currentTime - previousSampleTime)
      >= SAMPLE_INTERVAL_US)
  {
    // Schedule relative to current time to avoid catch-up bursts
    previousSampleTime = currentTime;

    int16_t rawX, rawY, rawZ;

    if (!readAccelerometer(rawX, rawY, rawZ))
      return;

    // 1. Convert raw readings to g
    float ax_g = rawX / ACCEL_SENSITIVITY;
    float ay_g = rawY / ACCEL_SENSITIVITY;
    float az_g = rawZ / ACCEL_SENSITIVITY;

    // 2. Subtract calibrated offsets
    float ax_dynamic = ax_g - BASELINE_X;
    float ay_dynamic = ay_g - BASELINE_Y;
    float az_dynamic = az_g - BASELINE_Z;

    // 3. Calculate unfiltered dynamic acceleration magnitude
    float a_raw = sqrt(
      ax_dynamic * ax_dynamic +
      ay_dynamic * ay_dynamic +
      az_dynamic * az_dynamic
    );

    // 4. Apply FIR independently to all three axes
    float ax_filtered, ay_filtered, az_filtered;

    applyFIR(
      ax_dynamic, ay_dynamic, az_dynamic,
      ax_filtered, ay_filtered, az_filtered
    );

    // 5. Calculate magnitude AFTER filtering the axes
    float a_filtered = sqrt(
      ax_filtered * ax_filtered +
      ay_filtered * ay_filtered +
      az_filtered * az_filtered
    );

    // 6. Send two labelled series to Serial Plotter
    Serial.print("A_raw:");
    Serial.print(a_raw, 6);

    Serial.print("\tA_FIR:");
    Serial.println(a_filtered, 6);
  }
}