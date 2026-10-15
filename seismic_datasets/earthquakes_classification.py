import pandas as pd
import numpy as np

# 1. Load dataset
df = pd.read_csv('earthquakes_2023_global.csv')

# 2. Filter missing entries
clean_df = df.dropna(subset=['mag', 'rms'])

# 3. Categorize rms values by magnitude
mild_rms = clean_df[clean_df['mag'] < 5.0]['rms']
mod_rms  = clean_df[(clean_df['mag'] >= 5.0) & (clean_df['mag'] < 6.0)]['rms']
sev_rms  = clean_df[clean_df['mag'] >= 6.0]['rms']

# 4. Extract Medians
med_mild = np.median(mild_rms)
med_mod  = np.median(mod_rms)
med_sev  = np.median(sev_rms)

# 5. Calculate Midpoint Thresholds
T_mild_to_mod = (med_mild + med_mod) / 2
T_mod_to_sev  = (med_mod + med_sev) / 2

print(f"Median Mild     : {med_mild:.4f}")
print(f"Median Moderate : {med_mod:.4f}")
print(f"Median Severe   : {med_sev:.4f}")
print(f"Cutoff Mild->Mod: {T_mild_to_mod:.4f}")
print(f"Cutoff Mod->Sev : {T_mod_to_sev:.4f}")