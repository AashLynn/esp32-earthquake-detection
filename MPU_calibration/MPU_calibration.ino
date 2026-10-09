#include <Wire.h>
#include <math.h>

#define MPU6050_ADDR 0x68

// MPU6050 registers
#define PWR_MGMT_1   0x6B
#define ACCEL_CONFIG 0x1C
#define ACCEL_XOUT_H 0x3B

// Calibration settings
#define CALIBRATION_TIME 120000   // 2 minutes
#define SAMPLE_INTERVAL 10        // 10 ms = approximately 100 Hz

// ±2g range
#define ACCEL_SENSITIVITY 16384.0f


// -----------------------------------------
// Statistics structure
// -----------------------------------------

struct Statistics {
    unsigned long count;
    double mean;
    double M2;
};

Statistics xStats = {0, 0.0, 0.0};
Statistics yStats = {0, 0.0, 0.0};
Statistics zStats = {0, 0.0, 0.0};


// -----------------------------------------
// Update statistics
// -----------------------------------------

void updateStatistics(Statistics &stats, double value)
{
    stats.count++;

    double delta = value - stats.mean;

    stats.mean += delta / stats.count;

    double delta2 = value - stats.mean;

    stats.M2 += delta * delta2;
}


// -----------------------------------------
// Calculate variance
// -----------------------------------------

double getVariance(Statistics &stats)
{
    if (stats.count == 0)
        return 0.0;

    return stats.M2 / stats.count;
}


// -----------------------------------------
// Calculate standard deviation
// -----------------------------------------

double getStandardDeviation(Statistics &stats)
{
    return sqrt(getVariance(stats));
}


// -----------------------------------------
// Read accelerometer
// -----------------------------------------

bool readAccelerometer(int16_t &ax, int16_t &ay, int16_t &az)
{
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(ACCEL_XOUT_H);

    if (Wire.endTransmission(false) != 0)
        return false;

    if (Wire.requestFrom(MPU6050_ADDR, 6) != 6)
        return false;

    ax = (Wire.read() << 8) | Wire.read();
    ay = (Wire.read() << 8) | Wire.read();
    az = (Wire.read() << 8) | Wire.read();

    return true;
}


// -----------------------------------------
// Configure MPU6050
// -----------------------------------------

void setupMPU6050()
{
    // Wake up MPU6050
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(PWR_MGMT_1);
    Wire.write(0x00);
    Wire.endTransmission();

    delay(100);

    // Set accelerometer to ±2g
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(ACCEL_CONFIG);
    Wire.write(0x00);
    Wire.endTransmission();

    delay(100);
}


// -----------------------------------------
// Setup
// -----------------------------------------

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Wire.begin(21, 22);

    setupMPU6050();

    // Countdown
    Serial.println("CALIBRATION_START");

    for (int i = 5; i > 0; i--)
    {
        Serial.print("Starting in ");
        Serial.print(i);
        Serial.println("...");

        delay(1000);
    }

    // CSV header
    Serial.println();
    Serial.println("Sample,Time_ms,Ax_g,Ay_g,Az_g");
}


// -----------------------------------------
// Main loop
// -----------------------------------------

void loop()
{
    static unsigned long startTime = millis();
    static unsigned long lastSampleTime = 0;
    static unsigned long sampleNumber = 0;

    unsigned long currentTime = millis();


    // -----------------------------------------
    // Calibration finished
    // -----------------------------------------

    if (currentTime - startTime >= CALIBRATION_TIME)
    {
        Serial.println();
        Serial.println("CALIBRATION_COMPLETE");

        Serial.println();
        Serial.println("Axis,Mean_g,Variance_g2,StdDev_g");


        // X
        Serial.print("X,");
        Serial.print(xStats.mean, 8);
        Serial.print(",");
        Serial.print(getVariance(xStats), 10);
        Serial.print(",");
        Serial.println(getStandardDeviation(xStats), 8);


        // Y
        Serial.print("Y,");
        Serial.print(yStats.mean, 8);
        Serial.print(",");
        Serial.print(getVariance(yStats), 10);
        Serial.print(",");
        Serial.println(getStandardDeviation(yStats), 8);


        // Z
        Serial.print("Z,");
        Serial.print(zStats.mean, 8);
        Serial.print(",");
        Serial.print(getVariance(zStats), 10);
        Serial.print(",");
        Serial.println(getStandardDeviation(zStats), 8);


        Serial.println();
        Serial.print("Total samples,");
        Serial.println(xStats.count);

        Serial.println("CALIBRATION_FINISHED");


        // Stop here
        while (true)
        {
            delay(1000);
        }
    }


    // -----------------------------------------
    // Take sample every 10 ms
    // -----------------------------------------

    if (currentTime - lastSampleTime >= SAMPLE_INTERVAL)
    {
        lastSampleTime = currentTime;

        int16_t axRaw;
        int16_t ayRaw;
        int16_t azRaw;


        if (readAccelerometer(axRaw, ayRaw, azRaw))
        {
            sampleNumber++;

            // Convert raw → g
            double ax = (double)axRaw / ACCEL_SENSITIVITY;
            double ay = (double)ayRaw / ACCEL_SENSITIVITY;
            double az = (double)azRaw / ACCEL_SENSITIVITY;


            // Update statistics
            updateStatistics(xStats, ax);
            updateStatistics(yStats, ay);
            updateStatistics(zStats, az);


            // -----------------------------------------
            // CSV DATA ROW
            // -----------------------------------------

            Serial.print(sampleNumber);
            Serial.print(",");

            Serial.print(currentTime - startTime);
            Serial.print(",");

            Serial.print(ax, 6);
            Serial.print(",");

            Serial.print(ay, 6);
            Serial.print(",");

            Serial.println(az, 6);
        }
    }
}