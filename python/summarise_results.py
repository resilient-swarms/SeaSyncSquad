import math
import numpy as np
import pandas as pd
import matplotlib
import matplotlib.pyplot as plt
import seaborn as sns
import pathlib

module_dir = pathlib.Path(__file__).parent.resolve()
root_dir = module_dir.parent
dir_results = root_dir.joinpath("results")

distances = [10.0, 50.0, 100.0, 150.0, 200.0] # m
wave_hts = [3.0, 5.0, 7.5, 10.0, 12.0, 15.0] # m
wind_speeds = [50.0, 100.0, 150.0, 200.0, 250.0, 300.0, 350.0] # km/hr
rainfall_rates = [2.0, 4.0, 6.0, 8.0, 10.0, 12.0, 15.0, 20.0, 30.0, 40.0, 50.0] # mm/hr
antenna_hts = [1.0, 2.0, 3.0, 4.0, 5.0] # m
radio_frequencies = [2400.0, 5200.0] # MHz

# Load results
df = pd.read_csv(dir_results.joinpath("radio_attenuation_data.csv"))
df["hydrometeor_attenuation(dB)"] = df["rain_attenuation(dB)"] + df["sea_spray_attenuation(dB)"]


plt.clf()
sns.lineplot(df, x="distance(km)", y="diffraction_loss(dB)", hue="radio_freq(MHz)", estimator="mean", errorbar="sd")
plt.show()


# Plot diffraction_loss and free space path loss
# ----------------------------------------------
plt.clf()
num_rows = len(wave_hts)
num_cols = len(antenna_hts)
fig, axes = plt.subplots(num_rows, num_cols, sharex=True, sharey=True)
plt.subplots_adjust(left=0.04, right=0.97, top=0.95, bottom=0.06, hspace=0.15, wspace=0.05,)
# Setting the values for all axes.
handles = None
labels = None
for i in range(num_rows):
    for j in range(num_cols):
        wave_ht = wave_hts[i]
        ant_ht = antenna_hts[j]
        data = df.loc[(df["ant_ht(m)"] == ant_ht) & (df["sig_wave_ht(m)"] == wave_ht)]
        lineplt = sns.lineplot(data=data, x="distance(km)", y="diffraction_loss(dB)", linestyle="--", marker="o",
                               hue="radio_freq(MHz)", estimator="mean", errorbar="ci", err_kws={"alpha":0.1}, ax=axes[i][j])
        lineplt = sns.lineplot(data=data, x="distance(km)", y="free_space_path_loss(dB)", linestyle=":", marker="o",
                               hue="radio_freq(MHz)", estimator="mean", errorbar="ci", err_kws={"alpha": 0.1}, ax=axes[i][j])
        lineplt.set(xlabel=None, ylabel=None)
        handles, labels = axes[i][j].get_legend_handles_labels()
        axes[i][j].get_legend().remove()
# Add text
fig.legend(handles=handles, labels=["Diffraction Loss(2.4GHz)", "Diffraction Loss(5.2GHz)", "Path Loss(2.4GHz)", "Path Loss(5.2GHz)"], loc="upper right", ncols=2, fontsize=8)
fig.suptitle("Impact of Wave Height, Antenna height, Antenna Distance, and Radio Frequency on Diffraction Loss", fontsize=10)
# Add text
fig.text(0.5, 0.04, 'X - Antenna Distance (km)', ha='center', fontsize=8)
fig.text(0.01, 0.5, 'Y - Diffraction Loss and Free Space Path Loss (dB)', va='center', rotation='vertical', fontsize=8)
x = 0.145
y = 0.935
dx = 0.190
for i in range(num_cols):
    fig.text(x+(i*dx), y, 'Antenna Height = {} m'.format(antenna_hts[i]), ha='center', fontsize=8)
x = 0.98
y = 0.84
dy = 0.174
for i in range(num_rows):
    fig.text(x, y-(i*dy), 'Wave Height = {} m'.format(wave_hts[i]), va='center', rotation='vertical', fontsize=8)
fig.tight_layout()
file_fig = dir_results.joinpath("diffraction_loss.png")
# plt.savefig(file_fig, dpi=300)
plt.show()


# Plot hydrometeor_attenuation
# ----------------------------
plt.clf()
num_rows = len(wind_speeds)
num_cols = len(rainfall_rates)
fig, axes = plt.subplots(num_rows, num_cols, sharex=True, sharey=True)
plt.subplots_adjust(left=0.04, right=0.97, top=0.95, bottom=0.06, hspace=0.15, wspace=0.05,)
# Setting the values for all axes.
handles = None
labels = None
for i in range(num_rows):
    for j in range(num_cols):
        wind_speed = wind_speeds[i]
        rainfall_rate = rainfall_rates[j]
        data = df.loc[(df["wind_speed(km/hr)"] == wind_speed) & (df["rainfall(mm/hr)"] == rainfall_rate)]
        lineplt = sns.lineplot(data=data, x="distance(km)", y="hydrometeor_attenuation(dB)", marker="o", hue="radio_freq(MHz)", estimator="mean", errorbar="ci", err_kws={"alpha":0.1}, ax=axes[i][j])
        lineplt.set(xlabel=None, ylabel=None)
        handles, labels = axes[i][j].get_legend_handles_labels()
        axes[i][j].get_legend().remove()
# Add text
fig.legend(title="Radio frequency (MHz)", handles=handles, labels=labels, loc="upper right", ncols=13, fontsize=8)
fig.suptitle("Impact of Rainfall Rate, Wind Speed, Antenna Distance, and Radio Frequency on Hydrometeor Attenuation", fontsize=10)
# Add text
fig.text(0.5, 0.04, 'X - Antenna Distance (km)', ha='center', fontsize=8)
fig.text(0.01, 0.5, 'Y - Hydrometeor Attenuation (dB)', va='center', rotation='vertical', fontsize=8)
x = 0.100
y = 0.935
dx = 0.118
for i in range(num_cols):
    fig.text(x+(i*dx), y, 'Rainfall = {} mm/hr'.format(rainfall_rates[i]), ha='center', fontsize=8)
x = 0.98
y = 0.88
dy = 0.132
for i in range(num_rows):
    fig.text(x, y-(i*dy), 'Wind = {} km/hr'.format(wind_speeds[i]), va='center', rotation='vertical', fontsize=8)
fig.tight_layout()
file_fig = dir_results.joinpath("hydrometeor_attenuation.png")
# plt.savefig(file_fig, dpi=300)
plt.show()