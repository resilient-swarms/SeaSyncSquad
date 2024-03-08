import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import math

f = open("../results/summary.csv", "w")
f.write("dist(m),ant_ht(m),hs(m),wave_dp(rad),peak_wave_len(m),dist_wave_len_ratio,seed,med_clr_win(sec),mean_clr_win(sec),std_clr_win(sec),med_ob_win(sec),mean_ob_win(sec),std_ob_win(sec)\n")

for dist in np.arange(100.0, 300.0, 10.0):
    for ant_ht in np.arange(1, 5, 1):
        for wave_hs in np.arange(0.5, 11.0, 1.0):
            for wave_dp in np.arange(0.0, math.pi, math.pi/6.0):
                for rand_seed in range(1, 6, 1):
                    file_name = "../results/log_{}_{}_{:0.1f}_{:0.1f}_{}_.csv".format(int(dist), int(ant_ht), wave_hs, wave_dp, rand_seed)
                    print("Summarising - ", file_name)
                    df = pd.read_csv(file_name)
                    # Calculate the communication window patterns
                    clear_window_lengths = [] # sec
                    obstructed_window_lengths = [] # sec
                    window_length = 0
                    previous_clearence_diff = None
                    for index, row in df.iterrows():
                        fresnel_min_required_clearence = 60.0
                        clearence = row["clearence(%)"]
                        clearence_diff = clearence - fresnel_min_required_clearence
                        if previous_clearence_diff == None:
                            previous_clearence_diff = clearence_diff
                        else:
                            time_step_size  = 40.0/1000 # sec
                            window_length += time_step_size 
                            if previous_clearence_diff * clearence_diff < 0.0:
                                # Clearence window changing
                                if previous_clearence_diff > 0.0:
                                    clear_window_lengths.append(window_length)
                                    previous_clearence_diff = clearence_diff
                                    window_length = 0.0
                                elif previous_clearence_diff < 0.0:
                                    obstructed_window_lengths.append(window_length)
                                    previous_clearence_diff = clearence_diff
                                    window_length = 0.0
                                else:
                                    continue
                            else:
                                previous_clearence_diff = clearence_diff
                    # Enter the final reading too
                    if(window_length != 0.0):
                        if previous_clearence_diff > 0.0:
                            clear_window_lengths.append(window_length)
                        else:
                            obstructed_window_lengths.append(window_length)
                    # Peak wave length
                    peak_wave_len = df["peak_wave_len(m)"].mean()
                    # Write summary to file
                    f.write("{dist},{ant_ht},{hs},{wave_dp},{peak_wave_len},{dist_wave_len_ratio},{seed},{med_clr_win:0.2f},{mean_clr_win:0.2f},{std_clr_win:0.2f},{med_ob_win:0.2f},{mean_ob_win:0.2f},{std_ob_win:0.2f}\n".format(
                        dist=dist,
                        ant_ht=ant_ht,
                        hs=wave_hs,
                        wave_dp=wave_dp,
                        peak_wave_len=peak_wave_len,
                        dist_wave_len_ratio=dist/peak_wave_len,
                        seed=rand_seed,
                        med_clr_win=np.median(clear_window_lengths),
                        mean_clr_win=np.mean(clear_window_lengths),
                        std_clr_win=np.std(clear_window_lengths),
                        med_ob_win=np.median(obstructed_window_lengths),
                        mean_ob_win=np.mean(obstructed_window_lengths),
                        std_ob_win=np.std(obstructed_window_lengths)
                    ))                        

f.close()