from pathlib import Path
from obspy import read
import pandas as pd
import math


# ============================================================
# PATH TO SAC FILES
# ============================================================

sac_folder = Path(
    r"D:\Arduino_Projects\earthquake_detection_and_alarm_system\seismic_datasets\Mexico_M8.1\Mexico_M8.1_SAC"
)


# ============================================================
# EARTHQUAKE LOCATION — CALTECH
# ============================================================

EVENT_LAT = 15.0680
EVENT_LON = -93.7150
EVENT_DEPTH_KM = 69.7


# ============================================================
# HAVERSINE DISTANCE FUNCTION
# ============================================================

def haversine_distance(lat1, lon1, lat2, lon2):
    """
    Calculate the horizontal distance between two
    latitude/longitude coordinates in kilometres.
    """

    # Mean radius of Earth in kilometres
    R = 6371.0

    # Convert degrees to radians
    lat1 = math.radians(lat1)
    lon1 = math.radians(lon1)

    lat2 = math.radians(lat2)
    lon2 = math.radians(lon2)

    # Differences
    dlat = lat2 - lat1
    dlon = lon2 - lon1

    # Haversine formula
    a = (
        math.sin(dlat / 2) ** 2
        + math.cos(lat1)
        * math.cos(lat2)
        * math.sin(dlon / 2) ** 2
    )

    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))

    return R * c


# ============================================================
# FIND SAC FILES
# ============================================================

files = sorted(sac_folder.glob("*.sac"))

print(f"Found {len(files)} SAC files.")


# ============================================================
# READ SAC FILE INFORMATION
# ============================================================

rows = []

for i, file in enumerate(files, start=1):

    try:

        # Read only the SAC header
        # This avoids loading the actual waveform data
        tr = read(str(file), headonly=True)[0]

        sac = getattr(tr.stats, "sac", {})

        # ----------------------------------------------------
        # Get station coordinates
        # ----------------------------------------------------

        station_lat = sac.get("stla", None)
        station_lon = sac.get("stlo", None)

        # ----------------------------------------------------
        # Calculate distance from station to earthquake
        # ----------------------------------------------------

        distance = None

        if (
            station_lat is not None
            and station_lon is not None
            and station_lat != -12345
            and station_lon != -12345
        ):

            distance = haversine_distance(
                station_lat,
                station_lon,
                EVENT_LAT,
                EVENT_LON
            )

        # ----------------------------------------------------
        # Store information
        # ----------------------------------------------------

        rows.append({
            "File": file.name,
            "Station": tr.stats.station,
            "Channel": tr.stats.channel,
            "Sampling_Rate_Hz": tr.stats.sampling_rate,
            "Samples": tr.stats.npts,
            "Duration_s": tr.stats.endtime - tr.stats.starttime,
            "Start_Time": str(tr.stats.starttime),
            "End_Time": str(tr.stats.endtime),
            "Distance_km": distance
        })

        # ----------------------------------------------------
        # Progress indicator
        # ----------------------------------------------------

        if i % 50 == 0 or i == len(files):
            print(f"Processed {i}/{len(files)} files...")


    except Exception as e:

        print(f"Error reading {file.name}: {e}")


# ============================================================
# CREATE DATAFRAME
# ============================================================

df = pd.DataFrame(rows)


# ============================================================
# SAVE INVENTORY
# ============================================================

output_file = sac_folder.parent / "sac_inventory_2.csv"

df.to_csv(output_file, index=False)


# ============================================================
# SUMMARY
# ============================================================

print("\n======================================")
print("SAC INVENTORY CREATED")
print("======================================")

print(f"Total SAC files: {len(df)}")
print(f"Unique stations: {df['Station'].nunique()}")


# ============================================================
# DISTANCE SUMMARY
# ============================================================

print("\n===== DISTANCE INFORMATION =====")

if df["Distance_km"].notna().any():

    print(
        f"Minimum distance: "
        f"{df['Distance_km'].min():.2f} km"
    )

    print(
        f"Maximum distance: "
        f"{df['Distance_km'].max():.2f} km"
    )

    stations_with_distance = (
        df.groupby("Station")["Distance_km"]
        .first()
        .notna()
        .sum()
    )

    print(
        f"Stations with distance data: "
        f"{stations_with_distance}"
    )

else:

    print("No distance information was calculated.")


# ============================================================
# FIRST 10 ENTRIES
# ============================================================

print("\n===== FIRST 10 ENTRIES =====")

print(
    df.head(10).to_string(index=False)
)


# ============================================================
# FINAL MESSAGE
# ============================================================

print("\n======================================")
print("Inventory saved to:")
print(output_file)
print("======================================")