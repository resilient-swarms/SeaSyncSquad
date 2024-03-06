import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

f = open("../results/summary.csv", "w")
f.write("ant_ht,hs,wave_dp,seed,med_clr_win,mean_clr_win,std_clr_win,med_ob_win,mean_ob_win,std_ob_win\n")

ant_hts = [1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5]
for f_ant_ht in ant_hts:
    dfs = pd.read_csv("../results/log_{}_.csv".format(f_ant_ht))
    groups = dfs.groupby(["ant_ht", "hs", "wave_dp", "seed"])
    time_step_size  = 40.0/1000 # sec
    for (ant_ht, hs, wave_dp, seed), df in groups:
        print("Summarising - ({}, {}, {}, {})".format(ant_ht, hs, wave_dp, seed))
        # Calculate the communication window patterns
        clear_window_lengths = [] # sec
        obstructed_window_lengths = [] # sec
        window_length = 0
        previous_clearence_diff = None
        for index, row in df.iterrows():
            clearence = row["clearence"]
            clearence_diff = clearence - 60.0
            if previous_clearence_diff == None:
                previous_clearence_diff = clearence_diff
            else:
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

        f.write("{ant_ht},{hs},{wave_dp},{seed},{med_clr_win:0.2f},{mean_clr_win:0.2f},{std_clr_win:0.2f},{med_ob_win:0.2f},{mean_ob_win:0.2f},{std_ob_win:0.2f}\n".format(
            ant_ht=ant_ht,
            hs=hs,
            wave_dp=wave_dp,
            seed=seed,
            med_clr_win=np.median(clear_window_lengths),
            mean_clr_win=np.mean(clear_window_lengths),
            std_clr_win=np.std(clear_window_lengths),
            med_ob_win=np.median(obstructed_window_lengths),
            mean_ob_win=np.mean(obstructed_window_lengths),
            std_ob_win=np.std(obstructed_window_lengths)
        ))

f.close()
