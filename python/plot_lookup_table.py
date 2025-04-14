import math
import numpy as np
import pandas as pd
import matplotlib
import matplotlib.pyplot as plt
import seaborn as sns
import pathlib

module_dir = pathlib.Path(__file__).parent.resolve()
root_dir = module_dir.parent
dir_results = root_dir.joinpath("results_range_table")

radio_frequencies = [2400.0, 5200.0] # MHz
antenna_hts = [0.5, 1.5, 3.0] # m
wave_hts = [0.5, 2.5, 5.0, 7.5, 10.] # m
distances = np.arange(100.0, 5100.0, 100.0)

# Load results
df = pd.read_csv(dir_results.joinpath("diffraction.csv"))
df = df[df["wave_ht(m)"].isin(wave_hts)]
df = df[df["ant_ht(m)"].isin(antenna_hts)]

# plt.clf()
# sns.lineplot(df, x="total_loss(db)", y="dist(m)", hue="freq(MHz)", estimator="mean", errorbar="sd")
# plt.show()


# Plot diffraction_loss and free space path loss
# ----------------------------------------------
plt.clf()

num_rows = len(radio_frequencies)
num_cols = len(antenna_hts)

fig, axes = plt.subplots(num_rows, num_cols, sharex=True, sharey=True, figsize=(12, 8))  # Increased figure size
plt.subplots_adjust(left=0.08, right=0.92, top=0.92, bottom=0.08, hspace=0.2, wspace=0.1)  # Adjusted layout

# Setting the values for all axes
handles, labels = None, None

for i in range(num_rows):
    for j in range(num_cols):
        freq = radio_frequencies[i]
        ant_ht = antenna_hts[j]
        data = df.loc[(df["ant_ht(m)"] == ant_ht) & (df["freq(MHz)"] == freq)]
        
        # lineplt = sns.lineplot(
        #     data=data, x="dist(m)", y="total_loss(db)", linestyle="--", marker="o", markersize=5, linewidth=1.5,
        #     hue="wave_ht(m)", estimator="mean", errorbar="ci", err_kws={"alpha": 0.1}, ax=axes[i][j]
        # )

        scatterplt = sns.scatterplot(
            data=data, x="dist(m)", y="diff_loss(db)", marker="o", hue="wave_ht(m)",  ax=axes[i][j]
        )
        
        scatterplt.set(xlabel=None, ylabel=None)
        axes[i][j].grid(True, linestyle="--", alpha=0.5)  # Added grid for clarity
        handles, labels = axes[i][j].get_legend_handles_labels()
        axes[i][j].get_legend().remove()

# Global Legend
fig.legend(handles=handles, title="Significant Wave Height (m)", labels=wave_hts, loc="lower right", ncols=5, fontsize=8, title_fontsize=8)

# Title and Axis Labels
fig.suptitle("Impact of Sea State, Antenna Height, and Radio Frequency on Diffraction loss", fontsize=12, fontweight="bold")

fig.text(0.5, 0.02, 'X - Communication Range (m)', ha='center', fontsize=8, fontweight="bold")
fig.text(0.02, 0.5, 'Y - Diffraction Loss (dB)', va='center', rotation='vertical', fontsize=8, fontweight="bold")

# Antenna Height Labels
x_start = 0.22
x_step = 0.29
y_antenna = 0.94
for i in range(num_cols):
    fig.text(x_start + (i * x_step), y_antenna, f'Antenna Height = {antenna_hts[i]} m', ha='center', fontsize=8, fontweight="bold")

# Radio Frequency Labels
x_freq = 0.94
y_start = 0.8
y_step = 0.3
for i in range(num_rows):
    fig.text(x_freq, y_start - (i * y_step), f'Radio Frequency = {radio_frequencies[i]} MHz', va='center', rotation='vertical', fontsize=8, fontweight="bold")

# fig.tight_layout()

# Save figure
file_fig = dir_results.joinpath("diffraction.png")
plt.savefig(file_fig, dpi=300, bbox_inches="tight")
# plt.show()



# Load FSPL data
df = pd.read_csv(dir_results.joinpath("fspl.csv"))
plt.clf()
for freq, group in df.groupby('freq(MHz)'):
    plt.scatter(group['dist(m)'], group['fspl(db)'], marker='o', label=f'{freq} MHz')
plt.title('Distance vs FSPL Grouped by Frequency')
plt.xlabel('Distance (m)')
plt.ylabel('Free Space Path Loss (dB)')
plt.legend(title='Frequency')
plt.grid(True)
plt.tight_layout()
file_fig = dir_results.joinpath("fspl.png")
plt.savefig(file_fig, dpi=300, bbox_inches="tight")
# plt.show()


# Load rain attenuation data
df = pd.read_csv(dir_results.joinpath("rain_attenuation.csv"))
df = df.loc[(df["dist(m)"].isin([100, 1000, 2000, 3000, 4000, 5000]))]
plt.clf()
df.boxplot(column='rain_attenuation(db)', by='dist(m)', grid=False, showfliers=False)
plt.title('Rain Attenuation by Distance')
plt.suptitle('')  # Remove the default subtitle
plt.xlabel('Distance (m)')
plt.ylabel('Rain Attenuation (dB)')
plt.tight_layout()
file_fig = dir_results.joinpath("rain_attenuation.png")
plt.savefig(file_fig, dpi=300, bbox_inches="tight")
# plt.show()

# Load sea spray attenuation data
df = pd.read_csv(dir_results.joinpath("sea_spray_attenuation.csv"))
df = df.loc[(df["dist(m)"].isin([100, 1000, 2000, 3000, 4000, 5000]))]
plt.clf()
df.boxplot(column='sea_spray_attenuation(db)', by='dist(m)', grid=False, showfliers=False)
plt.title('Sea Spray Attenuation by Distance')
plt.suptitle('')  # Remove the default subtitle
plt.xlabel('Distance (m)')
plt.ylabel('Sea Spray Attenuation (dB)')
plt.tight_layout()
file_fig = dir_results.joinpath("sea_spray_attenuation.png")
plt.savefig(file_fig, dpi=300, bbox_inches="tight")
# plt.show()