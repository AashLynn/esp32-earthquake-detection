import pandas as pd

# Load the SAC inventory
data = pd.read_csv("sac_inventory.csv")

print("\n===== BASIC INFORMATION =====")
print(f"Total SAC files: {len(data)}")
print(f"Unique stations: {data['Station'].nunique()}")

print("\n===== SAMPLING RATES =====")
print(data["Sampling_Rate_Hz"].value_counts().sort_index())

print("\n===== CHANNELS =====")
print(data["Channel"].value_counts().sort_index())

print("\n===== DURATIONS =====")
print(data["Duration_s"].value_counts().sort_index())

print("\n===== FILES PER STATION =====")
print(data["Station"].value_counts().sort_index())

print("\n===== COMPONENTS PER STATION =====")

components = (
    data.groupby("Station")["Channel"]
    .apply(lambda x: ", ".join(sorted(x.unique())))
)

print(components.to_string())