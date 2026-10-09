import serial
import csv
import time
import os

PORT = "COM5"
BAUD_RATE = 115200

DATA_FILE = "calibration_data.csv"
SUMMARY_FILE = "calibration_summary.csv"

print("Connecting to ESP32...")

ser = serial.Serial(
    PORT,
    BAUD_RATE,
    timeout=1
)

time.sleep(2)

print("Connected.")
print("Waiting for calibration data...")
print()

with open(DATA_FILE, "w", newline="") as data_file:

    data_writer = csv.writer(data_file)

    data_writer.writerow([
        "Sample",
        "Time_ms",
        "Ax_g",
        "Ay_g",
        "Az_g"
    ])

    calibration_finished = False

    while not calibration_finished:

        line = ser.readline().decode(
            "utf-8",
            errors="ignore"
        ).strip()

        if not line:
            continue

        if line == "Sample,Time_ms,Ax_g,Ay_g,Az_g":
            print("Receiving calibration data...")
            continue

        if line == "CALIBRATION_COMPLETE":
            print()
            print("Calibration data collection complete.")
            calibration_finished = True
            continue

        parts = line.split(",")

        if len(parts) == 5:

            try:
                sample = int(parts[0])
                time_ms = int(parts[1])
                ax = float(parts[2])
                ay = float(parts[3])
                az = float(parts[4])

                data_writer.writerow([
                    sample,
                    time_ms,
                    ax,
                    ay,
                    az
                ])

                if sample % 1000 == 0:
                    print(f"Received {sample} samples...")

            except ValueError:
                pass

print()
print("Waiting for calibration statistics...")

summary_rows = []

while True:

    line = ser.readline().decode(
        "utf-8",
        errors="ignore"
    ).strip()

    if not line:
        continue

    if line == "Axis,Mean_g,Variance_g2,StdDev_g":
        continue

    if line == "CALIBRATION_FINISHED":
        break

    parts = line.split(",")

    if len(parts) == 4:

        if parts[0] in ["X", "Y", "Z"]:

            try:
                axis = parts[0]
                mean = float(parts[1])
                variance = float(parts[2])
                std_dev = float(parts[3])

                summary_rows.append([
                    axis,
                    mean,
                    variance,
                    std_dev
                ])

            except ValueError:
                pass

with open(SUMMARY_FILE, "w", newline="") as summary_file:

    summary_writer = csv.writer(summary_file)

    summary_writer.writerow([
        "Axis",
        "Mean_g",
        "Variance_g2",
        "StdDev_g"
    ])

    summary_writer.writerows(summary_rows)

ser.close()

print()
print("===================================")
print(" CALIBRATION CAPTURE COMPLETE")
print("===================================")

print()
print("Sensor data saved to:")
print(os.path.abspath(DATA_FILE))

print()
print("Calibration statistics saved to:")
print(os.path.abspath(SUMMARY_FILE))

print()
print("Done.")