import math
import numpy as np
import pandas as pd
import matplotlib
import matplotlib.pyplot as plt
import seaborn as sns
import pathlib

# ==============================
# Global Plot Settings
# ==============================

matplotlib.rcParams.update({
    'figure.dpi': 300,
    'savefig.dpi': 300,
    'font.size': 10,
    'axes.labelsize': 10,
    'axes.titlesize': 12,
    'xtick.labelsize': 9,
    'ytick.labelsize': 9,
    'legend.fontsize': 8,
    'figure.titlesize': 12,
    'lines.linewidth': 1.5,
    'lines.markersize': 5,
    'pdf.fonttype': 42,
    'ps.fonttype': 42,
    'axes.grid': True,
    'grid.linestyle': '--',
    'grid.alpha': 0.5
})
plt.rc('font', family='serif')
sns.set_style("whitegrid")

# ==============================
# Common Parameters
# ==============================

# Paths
module_dir = pathlib.Path(__file__).parent.resolve()
root_dir = module_dir.parent
dir_results = root_dir.joinpath("results_range_table")

# Inputs
radio_frequencies = [2400.0, 5200.0]  # MHz
antenna_hts = [0.5, 1.5, 3.0]  # m
wave_hts = [0.5, 2.5, 5.0, 7.5, 10.]  # m
distances = np.arange(100.0, 5100.0, 100.0)
selected_distances = [100, 1000, 2000, 3000, 4000, 5000]

# Colors
palette_freq = sns.color_palette("Set2", len(radio_frequencies))
palette_wave = sns.color_palette("viridis", len(wave_hts))
palette_ant_ht = sns.color_palette("viridis", len(antenna_hts))

# ==============================
# Plot 1: Diffraction Loss
# ==============================

df = pd.read_csv(dir_results.joinpath("diffraction.csv"))
df = df[df["wave_ht(m)"].isin(wave_hts)]
df = df[df["ant_ht(m)"].isin(antenna_hts)]

fig, axes = plt.subplots(len(radio_frequencies), len(antenna_hts), sharex=True, sharey=True, figsize=(12, 8))
plt.subplots_adjust(left=0.08, right=0.92, top=0.92, bottom=0.08, hspace=0.2, wspace=0.1)

handles, labels = None, None

for i, freq in enumerate(radio_frequencies):
    for j, ant_ht in enumerate(antenna_hts):
        ax = axes[i][j]
        data = df[(df["ant_ht(m)"] == ant_ht) & (df["freq(MHz)"] == freq)]
        sns.scatterplot(data=data, x="dist(m)", y="diff_loss(db)", hue="wave_ht(m)", edgecolor='black', linewidth=0.2,
                        ax=ax, s=25, palette=palette_wave, legend=True)
        ax.set(xlabel=None, ylabel=None)
        if i == 0 and j == 0:
            handles, labels = ax.get_legend_handles_labels()

fig.legend(handles=handles, title="Wave Height (m)", labels=wave_hts,
           loc="lower right", ncols=5, fontsize=8, title_fontsize=8)
fig.suptitle("Impact of Sea State, Antenna Height, and Radio Frequency on Diffraction Loss",
             fontsize=12, fontweight="bold")
fig.text(0.5, 0.02, 'Distance (m)', ha='center', fontsize=10)
fig.text(0.02, 0.5, 'Diffraction Loss (dB)', va='center', rotation='vertical', fontsize=10)

x_start = 0.22
x_step = 0.29
y_antenna = 0.94
for i in range(len(antenna_hts)):
    fig.text(x_start + (i * x_step), y_antenna, f'Antenna Height = {antenna_hts[i]} m', ha='center', fontsize=10, fontweight="bold")

x_freq = 0.94
y_start = 0.75
y_step = 0.5
for i in range(len(radio_frequencies)):
    fig.text(x_freq, y_start - (i * y_step), f'Freq = {radio_frequencies[i]} MHz', va='center', rotation='vertical', fontsize=10, fontweight="bold")

plt.savefig(dir_results.joinpath("diffraction.png"), bbox_inches="tight")
plt.close()

# ==============================
# Plot 2: FSPL
# ==============================

df = pd.read_csv(dir_results.joinpath("fspl.csv"))
plt.figure(figsize=(6, 4))
for idx, (freq, group) in enumerate(df.groupby('freq(MHz)')):
    plt.scatter(group['dist(m)'], group['fspl(db)'], label=f'{freq} MHz', s=20, alpha=0.8, edgecolor='black', linewidth=0.2,
                color=palette_freq[idx % len(palette_freq)])
plt.title('Free Space Path Loss by Frequency')
plt.xlabel('Distance (m)')
plt.ylabel('FSPL (dB)')
plt.legend(title='Frequency')
plt.tight_layout()
plt.savefig(dir_results.joinpath("fspl.png"), bbox_inches="tight")
plt.close()

# ==============================
# Plot 3: Rain Attenuation
# ==============================

df = pd.read_csv(dir_results.joinpath("rain_attenuation.csv"))
df = df[df["dist(m)"].isin(selected_distances)]

plt.figure(figsize=(8, 5))
sns.boxplot(x="dist(m)", y="rain_attenuation(db)", hue="freq(MHz)", data=df,
            showfliers=False, palette=palette_freq)
plt.title('Rain Attenuation by Distance and Frequency')
plt.xlabel('Distance (m)')
plt.ylabel('Rain Attenuation (dB)')
plt.legend(title="Frequency (MHz)")
plt.tight_layout()
plt.savefig(dir_results.joinpath("rain_attenuation.png"), bbox_inches="tight")
plt.close()

# ==============================
# Plot 4: Sea Spray Attenuation
# ==============================

df = pd.read_csv(dir_results.joinpath("sea_spray_attenuation.csv"))
df = df[df["dist(m)"].isin(selected_distances)]

plt.figure(figsize=(8, 5))
sns.boxplot(x="dist(m)", y="sea_spray_attenuation(db)", hue="freq(MHz)", data=df,
            showfliers=False, palette=palette_freq)
plt.title('Sea Spray Attenuation by Distance and Frequency')
plt.xlabel('Distance (m)')
plt.ylabel('Sea Spray Attenuation (dB)')
plt.legend(title="Frequency (MHz)")
plt.tight_layout()
plt.savefig(dir_results.joinpath("sea_spray_attenuation.png"), bbox_inches="tight")
plt.close()

# ==============================
# Plot 5: Comms Window
# ==============================

df = pd.read_csv(dir_results.joinpath("comms_window.csv"))
plt.figure(figsize=(6, 4))
for idx, (ant_ht, group) in enumerate(df.groupby("ant_ht(m)")):    
    plt.scatter(group['wave_ht(m)'], 100 - group['comms_window(%)'],
                label=f"{ant_ht} m", edgecolor='black', linewidth=0.2, alpha=0.8)
plt.title('Obstruction-Free Link Time vs Wave Height')
plt.xlabel('Wave Height (m)')
plt.ylabel('Obstruction-Free Link Time (%)')
plt.legend(title='Antenna Height')
plt.tight_layout()
plt.savefig(dir_results.joinpath("comms_window.png"), bbox_inches="tight")
plt.close()

# =================================
# Plot 6: Comms Window - Case study
# =================================

df = pd.read_csv(dir_results.joinpath("comms_window_case_study.csv"))
plt.figure(figsize=(6, 4))
for idx, (dist, group) in enumerate(df.groupby("dist(m)")):    
    plt.scatter(group['wave_ht(m)'], 100 - group['comms_window(%)'],
                label=f"{dist} m", edgecolor='black', linewidth=0.2, alpha=0.8)
plt.title('Obstruction-Free Link Time vs Wave Height')
plt.xlabel('Wave Height (m)')
plt.ylabel('Obstruction-Free Link Time (%)')
plt.legend(title='Distance (m)')
plt.tight_layout()
plt.savefig(dir_results.joinpath("comms_window_case_study.png"), bbox_inches="tight")
plt.close()