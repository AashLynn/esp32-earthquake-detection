from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from obspy import read


# ============================================================
# 1. DATASET PATHS
# ============================================================

DATASETS = {
    "LaCanada_M1.7": Path(
        r"D:\Arduino_Projects\earthquake_detection_and_alarm_system\seismic_datasets\LaCanada_M1.7\LaCanada_selected"
    ),

    "Indio_M4.9": Path(
        r"D:\Arduino_Projects\earthquake_detection_and_alarm_system\seismic_datasets\Indio_M4.9\Indio_selected"
    ),

    "Mexico_M8.1": Path(
        r"D:\Arduino_Projects\earthquake_detection_and_alarm_system\seismic_datasets\Mexico_M8.1\Mexico_selected"
    )
}


# Candidate analysis windows
WINDOWS = [0.5, 1.0, 1.5]


# Output folder
OUTPUT_DIR = Path("timing_window_analysis")
OUTPUT_DIR.mkdir(exist_ok=True)


# ============================================================
# 2. FUNCTION: LOAD THREE COMPONENTS
# ============================================================

def load_station_components(folder):

    """
    Groups HNE, HNN and HNZ files belonging to the same station.
    """

    files = list(folder.glob("*.SAC"))

    stations = {}

    for file in files:

        trace = read(str(file))[0]

        station = trace.stats.station
        channel = trace.stats.channel

        if station not in stations:
            stations[station] = {}

        stations[station][channel] = trace

    return stations


# ============================================================
# 3. FUNCTION: CALCULATE 3-COMPONENT ACCELERATION
# ============================================================

def get_dynamic_acceleration(station_data):

    """
    Creates 3-axis dynamic acceleration magnitude.

    Each component is centred by subtracting its mean.
    """

    required_channels = ["HNE", "HNN", "HNZ"]

    if not all(channel in station_data for channel in required_channels):
        return None

    traces = [
        station_data["HNE"],
        station_data["HNN"],
        station_data["HNZ"]
    ]

    # Make sure all components have the same length
    min_length = min(len(trace.data) for trace in traces)

    x = traces[0].data[:min_length].astype(float)
    y = traces[1].data[:min_length].astype(float)
    z = traces[2].data[:min_length].astype(float)

    # Simple centering for this timing-window experiment
    x = x - np.mean(x)
    y = y - np.mean(y)
    z = z - np.mean(z)

    # 3-axis acceleration magnitude
    magnitude = np.sqrt(
        x**2 +
        y**2 +
        z**2
    )

    sampling_rate = traces[0].stats.sampling_rate

    return magnitude, sampling_rate


# ============================================================
# 4. FUNCTION: CALCULATE WINDOW RMS
# ============================================================

def calculate_window_rms(data, sampling_rate, window_seconds):

    samples_per_window = int(
        sampling_rate * window_seconds
    )

    rms_values = []
    times = []

    number_of_windows = len(data) // samples_per_window

    for i in range(number_of_windows):

        start = i * samples_per_window
        end = start + samples_per_window

        window = data[start:end]

        rms = np.sqrt(
            np.mean(window ** 2)
        )

        rms_values.append(rms)

        # Time at centre of window
        centre_time = (
            start + samples_per_window / 2
        ) / sampling_rate

        times.append(centre_time)

    return np.array(times), np.array(rms_values)


# ============================================================
# 5. STORE SUMMARY RESULTS
# ============================================================

summary_results = []
file_results = []


# ============================================================
# 6. PROCESS EACH EARTHQUAKE DATASET
# ============================================================

for dataset_name, folder in DATASETS.items():

    print("\n========================================")
    print(f"Processing: {dataset_name}")
    print("========================================")

    stations = load_station_components(folder)

    print(f"Stations found: {len(stations)}")

    # --------------------------------------------------------
    # Process each station
    # --------------------------------------------------------

    for station, station_data in stations.items():

        result = get_dynamic_acceleration(station_data)

        if result is None:
            print(f"Skipping {station}: missing component")
            continue

        magnitude, sampling_rate = result

        print(
            f"{station}: "
            f"{sampling_rate:.1f} Hz, "
            f"{len(magnitude)} samples"
        )

        # ----------------------------------------------------
        # Candidate windows
        # ----------------------------------------------------

        for window in WINDOWS:

            times, rms_values = calculate_window_rms(
                magnitude,
                sampling_rate,
                window
            )

            if len(rms_values) == 0:
                continue

            # Summary statistics
            mean_rms = np.mean(rms_values)
            median_rms = np.median(rms_values)
            std_rms = np.std(rms_values)
            percentile_95 = np.percentile(
                rms_values,
                95
            )
            maximum_rms = np.max(rms_values)

            rms_ratio = (
                maximum_rms / median_rms
                if median_rms > 0
                else np.nan
            )

            summary_results.append({
                "Dataset": dataset_name,
                "Station": station,
                "Window_s": window,
                "Mean_RMS": mean_rms,
                "Median_RMS": median_rms,
                "Std_RMS": std_rms,
                "95th_Percentile_RMS": percentile_95,
                "Maximum_RMS": maximum_rms,
                "RMS_Ratio_Max_Median": rms_ratio
            })

            file_results.append({
                "Dataset": dataset_name,
                "Station": station,
                "Window_s": window,
                "Sampling_Rate_Hz": sampling_rate,
                "Number_of_Windows": len(rms_values)
            })

            # ------------------------------------------------
            # SAVE INDIVIDUAL RMS DATA
            # ------------------------------------------------

            rms_dataframe = pd.DataFrame({
                "Time_s": times,
                "RMS": rms_values
            })

            output_filename = (
                f"{dataset_name}_{station}_"
                f"{window:.1f}s_RMS.csv"
            )

            rms_dataframe.to_csv(
                OUTPUT_DIR / output_filename,
                index=False
            )

            # ------------------------------------------------
            # PLOT RMS VS TIME
            # ------------------------------------------------

            plt.figure(figsize=(12, 5))

            plt.plot(
                times,
                rms_values,
                linewidth=1
            )

            plt.xlabel("Time (s)")
            plt.ylabel("Window RMS")
            plt.title(
                f"{dataset_name} | "
                f"Station {station} | "
                f"{window:.1f} s window"
            )

            plt.grid(True, alpha=0.3)

            plt.tight_layout()

            plot_filename = (
                f"{dataset_name}_{station}_"
                f"{window:.1f}s_RMS.png"
            )

            plt.savefig(
                OUTPUT_DIR / plot_filename,
                dpi=200
            )

            plt.close()


# ============================================================
# 7. SAVE SUMMARY CSV FILES
# ============================================================

summary_df = pd.DataFrame(summary_results)

summary_df.to_csv(
    OUTPUT_DIR / "timing_window_summary.csv",
    index=False
)

file_df = pd.DataFrame(file_results)

file_df.to_csv(
    OUTPUT_DIR / "timing_window_file_results.csv",
    index=False
)


# ============================================================
# 8. OVERALL WINDOW COMPARISON
# ============================================================

grouped = (
    summary_df
    .groupby("Window_s")
    .agg({
        "Mean_RMS": "mean",
        "Median_RMS": "mean",
        "Std_RMS": "mean",
        "95th_Percentile_RMS": "mean",
        "Maximum_RMS": "mean",
        "RMS_Ratio_Max_Median": "mean"
    })
    .reset_index()
)


# ------------------------------------------------------------
# Plot maximum RMS vs window
# ------------------------------------------------------------

plt.figure(figsize=(8, 5))

plt.plot(
    grouped["Window_s"],
    grouped["Maximum_RMS"],
    marker="o"
)

plt.xlabel("Window length (s)")
plt.ylabel("Average maximum RMS")
plt.title("Maximum RMS vs Window Length")

plt.grid(True, alpha=0.3)

plt.tight_layout()

plt.savefig(
    OUTPUT_DIR / "maximum_rms_vs_window.png",
    dpi=200
)

plt.close()


# ------------------------------------------------------------
# Plot RMS variability vs window
# ------------------------------------------------------------

plt.figure(figsize=(8, 5))

plt.plot(
    grouped["Window_s"],
    grouped["Std_RMS"],
    marker="o"
)

plt.xlabel("Window length (s)")
plt.ylabel("Average RMS standard deviation")
plt.title("RMS Variability vs Window Length")

plt.grid(True, alpha=0.3)

plt.tight_layout()

plt.savefig(
    OUTPUT_DIR / "rms_variability_vs_window.png",
    dpi=200
)

plt.close()


# ------------------------------------------------------------
# Plot contrast ratio vs window
# ------------------------------------------------------------

plt.figure(figsize=(8, 5))

plt.plot(
    grouped["Window_s"],
    grouped["RMS_Ratio_Max_Median"],
    marker="o"
)

plt.xlabel("Window length (s)")
plt.ylabel("Maximum RMS / Median RMS")
plt.title("RMS Contrast vs Window Length")

plt.grid(True, alpha=0.3)

plt.tight_layout()

plt.savefig(
    OUTPUT_DIR / "rms_contrast_vs_window.png",
    dpi=200
)

plt.close()


# ============================================================
# 9. PRINT RESULTS
# ============================================================

print("\n========================================")
print("TIMING WINDOW ANALYSIS COMPLETE")
print("========================================")

print("\nOverall comparison:")
print(grouped.to_string(index=False))

print("\nOutput folder:")
print(OUTPUT_DIR.resolve())

print("\nGenerated:")
print("- timing_window_summary.csv")
print("- timing_window_file_results.csv")
print("- Individual RMS CSV files")
print("- Individual RMS-vs-time plots")
print("- maximum_rms_vs_window.png")
print("- rms_variability_vs_window.png")
print("- rms_contrast_vs_window.png")