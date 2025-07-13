import math
import numpy as np
import pandas as pd
import matplotlib
import matplotlib.pyplot as plt
import seaborn as sns
import pathlib
import matplotlib.image as mpimg

# ==============================
# Global Plot Settings
# ==============================

# Using British English spelling for comments and plot elements where appropriate.
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
# Ensure the results directory exists
dir_results.mkdir(exist_ok=True)


# Inputs
radio_frequencies = [2400.0, 5200.0]  # MHz
antenna_hts = [0.5, 1.5, 3.0]  # m
wave_hts = [0.5, 2.5, 5.0, 7.5, 10.]  # m
distances = np.arange(100.0, 5100.0, 100.0)
selected_distances = [100, 1000, 2000, 3000, 4000, 5000]

# Palettes and Styles for improved visual clarity, as per supervisor feedback.
palette_freq = sns.color_palette("Set2", len(radio_frequencies))
palette_wave = sns.color_palette("viridis", len(wave_hts))
palette_ant_ht = sns.color_palette("viridis", len(antenna_hts))

# --- NEW: Styles to improve accessibility and clarity ---
line_styles = ['-', '--', ':', '-.', (0, (3, 1, 1, 1))]
marker_styles_ant = ['o', 's', '^', 'D', 'v', 'p'] 
marker_styles_dist = ['o', 'X', 's', '^', 'D', 'v']

# ==============================
# Plot 1: Diffraction Loss 
# ==============================

df = pd.read_csv(dir_results.joinpath("diffraction.csv"))
df = df[df["wave_ht(m)"].isin(wave_hts)]
df = df[df["ant_ht(m)"].isin(antenna_hts)]

fig, axes = plt.subplots(len(radio_frequencies), len(antenna_hts), sharex=True, sharey=True, figsize=(12, 8))
plt.subplots_adjust(left=0.08, right=0.92, top=0.92, bottom=0.08, hspace=0.2, wspace=0.1)

# --- FIX: Replaced problematic seaborn call with a direct matplotlib loop ---
# This bypasses the library conflict and gives explicit control over styles to fix the TypeError.
for i, freq in enumerate(radio_frequencies):
    for j, ant_ht in enumerate(antenna_hts):
        ax = axes[i][j]
        
        for k, wave_ht in enumerate(wave_hts):
            # Filter data for each specific line
            data_line = df[(df["ant_ht(m)"] == ant_ht) & (df["freq(MHz)"] == freq) & (df["wave_ht(m)"] == wave_ht)]
            
            # Plot directly using matplotlib's plot function
            ax.plot(data_line["dist(m)"], data_line["diff_loss(db)"],
                    color=palette_wave[k],
                    linestyle=line_styles[k % len(line_styles)],
                    marker='.',
                    markersize=4,
                    label=f"{wave_ht:.1f} m")
        
        ax.set(xlabel=None, ylabel=None)

# Create a single legend for the figure from the last subplot
handles, labels = axes[-1][-1].get_legend_handles_labels()
fig.legend(handles=handles, title="Wave Height (m)", labels=labels,
           loc="lower right", ncol=len(wave_hts), fontsize=8, title_fontsize=8)

fig.text(0.5, 0.04, 'Distance (m)', ha='center', fontsize=10)
fig.text(0.02, 0.5, 'Diffraction Loss (dB)', va='center', rotation='vertical', fontsize=10)

# Add column titles for Antenna Height
for j, ant_ht in enumerate(antenna_hts):
    axes[0][j].set_title(f'Antenna Height = {ant_ht} m', fontsize=10, fontweight="bold")

# Add row titles for Frequency
for i, freq in enumerate(radio_frequencies):
    axes[i][-1].text(1.05, 0.5, f'Freq = {freq} MHz', va='center', rotation=-90,
                     transform=axes[i][-1].transAxes, fontsize=10, fontweight="bold")

plt.savefig(dir_results.joinpath("diffraction.png"), bbox_inches="tight")
plt.close()

# ==============================
# Plot 2: FSPL
# ==============================

df_fspl = pd.read_csv(dir_results.joinpath("fspl.csv"))
plt.figure(figsize=(6, 4))
sns.scatterplot(data=df_fspl, x='dist(m)', y='fspl(db)', hue='freq(MHz)',
                palette=palette_freq, s=20, alpha=0.8, edgecolor='black', linewidth=0.2)

plt.xlabel('Distance (m)')
plt.ylabel('FSPL (dB)')
plt.legend(title='Frequency')
plt.tight_layout()
plt.savefig(dir_results.joinpath("fspl.png"), bbox_inches="tight")
plt.close()

# ==============================
# Plot 3: Rain Attenuation
# ==============================

df_rain = pd.read_csv(dir_results.joinpath("rain_attenuation.csv"))
df_rain = df_rain[df_rain["dist(m)"].isin(selected_distances)]

plt.figure(figsize=(8, 5))
sns.boxplot(x="dist(m)", y="rain_attenuation(db)", hue="freq(MHz)", data=df_rain,
            showfliers=False, palette=palette_freq)
plt.xlabel('Distance (m)')
plt.ylabel('Rain Attenuation (dB)')
plt.legend(title="Frequency (MHz)")
plt.tight_layout()
plt.savefig(dir_results.joinpath("rain_attenuation.png"), bbox_inches="tight")
plt.close()

# ==============================
# Plot 4: Sea Spray Attenuation
# ==============================

df_spray = pd.read_csv(dir_results.joinpath("sea_spray_attenuation.csv"))
df_spray = df_spray[df_spray["dist(m)"].isin(selected_distances)]

plt.figure(figsize=(8, 5))
sns.boxplot(x="dist(m)", y="sea_spray_attenuation(db)", hue="freq(MHz)", data=df_spray,
            showfliers=False, palette=palette_freq)
plt.xlabel('Distance (m)')
plt.ylabel('Sea Spray Attenuation (dB)')
plt.legend(title="Frequency (MHz)")
plt.tight_layout()
plt.savefig(dir_results.joinpath("sea_spray_attenuation.png"), bbox_inches="tight")
plt.close()

# ==============================
# Plot 5: Comms Window 
# ==============================

df_comms = pd.read_csv(dir_results.joinpath("comms_window.csv"))
plt.figure(figsize=(7, 5))

# --- CHANGE: Using different marker styles for each antenna height ---
# This makes each series distinct without relying only on colour.
for idx, (ant_ht, group) in enumerate(df_comms.groupby("ant_ht(m)")):
    marker = marker_styles_ant[idx % len(marker_styles_ant)]
    plt.scatter(group['wave_ht(m)'], group['comms_window(%)'],
                label=f"{ant_ht} m", marker=marker, 
                edgecolor='black', linewidth=0.2, alpha=0.8, s=40)

plt.xlabel('Wave Height (m)')
plt.ylabel('Obstruction-Free Link Time (%)')
plt.legend(title='Antenna Height', bbox_to_anchor=(1.05, 1), loc='upper left')
plt.tight_layout()
plt.savefig(dir_results.joinpath("comms_window.png"), bbox_inches="tight")
plt.close()


# =================================
# Plot 6: Comms Window - Case study 
# =================================

df_case_study = pd.read_csv(dir_results.joinpath("comms_window_case_study.csv"))
plt.figure(figsize=(7, 5))

# --- CHANGE: Using different marker styles for each distance ---
# This makes each series distinct without relying only on colour.
for idx, (dist, group) in enumerate(df_case_study.groupby("dist(m)")):
    marker = marker_styles_dist[idx % len(marker_styles_dist)]
    plt.scatter(group['wave_ht(m)'], group['comms_window(%)'],
                label=f"{dist} m", marker=marker,
                edgecolor='black', linewidth=0.2, alpha=0.8, s=40)

plt.xlabel('Wave Height (m)')
plt.ylabel('Obstruction-Free Link Time (%)')
plt.legend(title='Distance (m)', bbox_to_anchor=(1.05, 1), loc='upper left')
plt.tight_layout()
plt.savefig(dir_results.joinpath("comms_window_case_study.png"), bbox_inches="tight")
plt.close()


# ==============================
# Plot 7: Simulation - swarm path
# ==============================
# List your 4 PNG image file paths here
# List your 8 PNG image file paths here
image_files = [
    "case_study2.000000_1.png",
    "case_study3.000000_1.png",
    "case_study4.000000_1.png",
    "case_study5.000000_1.png",
    "case_study6.000000_2.png",
    "case_study5.000000_2.png",
    "case_study4.000000_2.png",
    "case_study2.000000_2.png",
]

# Create 4x2 subplots to fit on A4 portrait
fig, axes = plt.subplots(
    4, 2,
    figsize=(8.27, 11.69),  # A4 in inches (portrait)
    gridspec_kw={'wspace': 0.02, 'hspace': 0.08}
)

# Flatten for indexing
axes = axes.flatten()

# Wave heights
sea_state = {
    0: 2.0, 1: 3.0, 2: 4.0, 3: 5.0,
    4: 6.0, 5: 5.0, 6: 4.0, 7: 2.0
}

# Load and plot images
for i, img_file in enumerate(image_files):
    img = mpimg.imread(dir_results.joinpath(img_file))
    axes[i].imshow(img)
    axes[i].axis('off')
    axes[i].set_title(f"Significant Wave Height = {sea_state[i]} m", fontsize=9)

# Save to file
plt.savefig(dir_results.joinpath("combined_subplot.png"), dpi=300, bbox_inches='tight')
plt.close()

# ==============================
# Plot 8: Simulation - swarm comms window
# ==============================

# Load data
df_comms = pd.read_csv(dir_results.joinpath("swarm_adaptation_sim.csv"))
df_comms['Index'] = 5 * df_comms.index

# Define wave heights and ensure categorical ordering
wave_hts = [2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0]
df_comms['wave_ht(m)'] = pd.Categorical(df_comms['wave_ht(m)'], categories=wave_hts, ordered=True)

# Create subplots
fig, axes = plt.subplots(nrows=2, ncols=1, figsize=(6, 6), sharex=True)

# First subplot: comms window %
sns.scatterplot(ax=axes[0], data=df_comms, x='Index', y='comms_window(%)',
                hue='wave_ht(m)', style='wave_ht(m)',
                palette=sns.color_palette("rocket_r", len(wave_hts)),
                s=40, alpha=0.8, edgecolor='black', linewidth=0.0)
axes[0].set_ylabel('Obstruction-Free Link Time (%)')
axes[0].legend(title='Wave Height (m)', bbox_to_anchor=(1.05, 1), loc='upper left')

# Second subplot: inter-vehicle distance
sns.scatterplot(ax=axes[1], data=df_comms, x='Index', y='dist(m)',
                hue='wave_ht(m)', style='wave_ht(m)',
                palette=sns.color_palette("rocket_r", len(wave_hts)),
                s=40, alpha=0.8, edgecolor='black', linewidth=0.0, legend=False)
axes[1].set_xlabel('Simulation Time (minutes)')
axes[1].set_ylabel('Max Inter-Vehicle Distance (m)')

# Set x-axis major ticks at multiples of 30
max_x = df_comms['Index'].max()
x_ticks = np.arange(0, max_x + 30, 30)
for ax in axes:
    ax.set_xticks(x_ticks)
    ax.grid(axis='x', which='major', linestyle='--', alpha=0.7)  # Only x-axis grid
    ax.grid(axis='y', which='major', alpha=0)  # Remove y-axis grid

# Final adjustments and save
plt.tight_layout()
plt.savefig(dir_results.joinpath("swarm_adaptation_sim.png"), dpi=300, bbox_inches='tight')
plt.close()


