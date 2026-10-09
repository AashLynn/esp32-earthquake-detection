from pathlib import Path
import pandas as pd
import shutil

# ============================================================
# PATHS
# ============================================================

base_folder = Path(
    r"D:\Arduino_Projects\earthquake_detection_and_alarm_system\seismic_datasets\Mexico_M8.1"
)

sac_folder = base_folder / "Mexico_M8.1_SAC"

inventory_file = base_folder / "sac_inventory_2.csv"

selected_folder = base_folder / "Mexico_selected"


# ============================================================
# NUMBER OF STATIONS TO SELECT
# ============================================================

NUMBER_OF_STATIONS = 2


# ============================================================
# READ INVENTORY
# ============================================================

df = pd.read_csv(inventory_file)


# ============================================================
# GET ONE DISTANCE VALUE PER STATION
# ============================================================

station_distances = (
    df[["Station", "Distance_km"]]
    .drop_duplicates(subset="Station")
    .sort_values("Distance_km")
)


# ============================================================
# SELECT NEAREST STATIONS
# ============================================================

selected_stations = station_distances.head(NUMBER_OF_STATIONS)


# ============================================================
# DISPLAY SELECTED STATIONS
# ============================================================

print("\n======================================")
print("SELECTED STATIONS")
print("======================================")

print(
    selected_stations.to_string(index=False)
)


# ============================================================
# CREATE OUTPUT FOLDER
# ============================================================

selected_folder.mkdir(exist_ok=True)


# ============================================================
# COPY ALL THREE COMPONENTS
# ============================================================

print("\n======================================")
print("COPYING SAC FILES")
print("======================================")

for station in selected_stations["Station"]:

    station_files = df[
        df["Station"] == station
    ]["File"]

    for filename in station_files:

        source = sac_folder / filename
        destination = selected_folder / filename

        shutil.copy2(source, destination)

        print(f"Copied: {filename}")


# ============================================================
# SAVE SELECTED STATION LIST
# ============================================================

selected_csv = base_folder / "selected_stations_2.csv"

selected_stations.to_csv(
    selected_csv,
    index=False
)


# ============================================================
# FINAL SUMMARY
# ============================================================

print("\n======================================")
print("SELECTION COMPLETE")
print("======================================")

print(
    f"Selected stations: "
    f"{len(selected_stations)}"
)

print(
    f"SAC files copied: "
    f"{len(selected_stations) * 3}"
)

print(
    f"\nSelected SAC folder:\n"
    f"{selected_folder}"
)

print(
    f"\nStation list saved to:\n"
    f"{selected_csv}"
)

print("======================================")