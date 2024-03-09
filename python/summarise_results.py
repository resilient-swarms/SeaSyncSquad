import math
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns


dir_results = "../results"
dir_report = dir_results + "/report"
dir_fig = dir_results + "/fig"

def summarise():
    f = open(dir_report + "/summary.csv", "w")
    f.write("dist(m),ant_ht(m),hs(m),wave_dp(deg),peak_wave_len(m),dist_wave_len_ratio,seed,med_clr_win(sec),mean_clr_win(sec),std_clr_win(sec),med_ob_win(sec),mean_ob_win(sec),std_ob_win(sec)\n")
    for dist in np.arange(100.0, 300.0, 10.0):
        for ant_ht in np.arange(1, 5, 1):
            for wave_hs in np.arange(0.5, 11.0, 1.0):
                for wave_dp in np.arange(0.0, math.pi, math.pi/6.0):
                    for rand_seed in range(1, 6, 1):
                        file_name = dir_results + "/log_{}_{}_{:0.1f}_{:0.1f}_{}_.csv".format(int(dist), int(ant_ht), wave_hs, wave_dp, rand_seed)
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
                            wave_dp=round(wave_dp*180/math.pi),
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


def report_coeff_variation():
    f_report = open(dir_report + "/report.md", "w")
    f_coeff_variation = open(dir_report + "/coeff_variation.csv", "w")
    f_coeff_variation.write("dist(m),ant_ht(m),hs(m),wave_dp(rad),cv_med_clr_win(%),cv_mean_clr_win(%),cv_med_ob_win(%),cv_mean_ob_win(%)\n")
    file_name = dir_report + "/summary.csv"
    df_summary = pd.read_csv(file_name)
    groups = df_summary.groupby(["dist(m)", "ant_ht(m)", "hs(m)", "wave_dp(rad)"])
    # Compute the coefficient of variations
    for (dist, ant_ht, hs, wave_dp), group in groups:
        cv_med_clr_win  = group["med_clr_win(sec)"].std()/group["med_clr_win(sec)"].mean() * 100
        cv_mean_clr_win = group["mean_clr_win(sec)"].std()/group["mean_clr_win(sec)"].mean() * 100
        cv_med_ob_win   = group["med_ob_win(sec)"].std()/group["med_ob_win(sec)"].mean() * 100
        cv_mean_ob_win  = group["mean_ob_win(sec)"].std()/group["mean_ob_win(sec)"].mean() * 100
        f_coeff_variation.write("{dist},{ant_ht},{hs},{wave_dp},{cv_med_clr_win},{cv_mean_clr_win},{cv_med_ob_win},{cv_mean_ob_win}\n".format(
            dist=dist,
            ant_ht=ant_ht,
            hs=hs,
            wave_dp=wave_dp,
            cv_med_clr_win=cv_med_clr_win,
            cv_mean_clr_win=cv_mean_clr_win,
            cv_med_ob_win=cv_med_ob_win,
            cv_mean_ob_win=cv_mean_ob_win
        ))
    f_coeff_variation.close()
    # Box plots
    plt.clf()
    df = pd.read_csv(dir_report + "/coeff_variation.csv")
    ax = df.boxplot(column=["cv_med_clr_win(%)", "cv_mean_clr_win(%)", "cv_med_ob_win(%)", "cv_mean_ob_win(%)"])
    plt.xticks(fontsize=8) # set the new x-axis labels
    plt.yticks(fontsize=8) # set the new y-axis label size
    plt.savefig(dir_fig + "/coefficient_of_variations.png", dpi=200)
    f_report.write("## Variations in different runs of the experiment\n")
    f_report.write("![text1](../fig/coefficient_of_variations.png)")
    # TODO: https://scales.arabpsychology.com/stats/seaborn-how-to-create-a-boxplot-of-multiple-columns/


def report_hs_effect_on_communication():
    f_report = open(dir_report + "/report.md", "w")
    df_summary = pd.read_csv(dir_report + "/summary.csv")
    df_summary["wave_dp(deg)"] = df_summary["wave_dp(rad)"] * 180.0/math.pi
    # Scatter plot
    plt.clf()
    sns.relplot(data=df_summary, x="hs(m)", y="med_clr_win(sec)", col="wave_dp(deg)", kind="scatter")
    plt.xticks(fontsize=8) # set the new x-axis labels
    plt.yticks(fontsize=8) # set the new y-axis label size
    plt.savefig(dir_fig + "/hs_effects_med_clr_win.png", dpi=200)
    plt.clf()
    sns.relplot(data=df_summary, x="hs(m)", y="mean_clr_win(sec)", col="wave_dp(deg)", kind="scatter")
    plt.xticks(fontsize=8) # set the new x-axis labels
    plt.yticks(fontsize=8) # set the new y-axis label size
    plt.savefig(dir_fig + "/hs_effects_mean_clr_win.png", dpi=200)



def report_wave_dp_effect_on_communication():
    f_report = open(dir_report + "/report.md", "w")
    df_summary = pd.read_csv(dir_report + "/summary.csv")
    df_summary["wave_dp(deg)"] = df_summary["wave_dp(rad)"] * 180.0/math.pi
    # Scatter plot
    plt.clf()
    sns.relplot(data=df_summary, x="wave_dp(deg)", y="med_clr_win(sec)", col="hs(m)", kind="scatter")
    plt.xticks(fontsize=8) # set the new x-axis labels
    plt.yticks(fontsize=8) # set the new y-axis label size
    plt.savefig(dir_fig + "/wave_dp_effects_med_clr_win.png", dpi=200)
    plt.clf()
    sns.relplot(data=df_summary, x="wave_dp(deg)", y="mean_clr_win(sec)", col="hs(m)", kind="scatter")
    plt.xticks(fontsize=8) # set the new x-axis labels
    plt.yticks(fontsize=8) # set the new y-axis label size
    plt.savefig(dir_fig + "/wave_dp_effects_mean_clr_win.png", dpi=200) 


def report_hs_by_ant_hight_effect_on_communication():
    f_report = open(dir_report + "/report.md", "w")
    df_summary = pd.read_csv(dir_report + "/summary.csv")
    df_summary["wave_dp(deg)"] = df_summary["wave_dp(rad)"] * 180.0/math.pi
    df_summary["hs/ant_ht"] = df_summary["hs(m)"]/df_summary["ant_ht(m)"]
    # Scatter plot
    plt.clf()
    sns.relplot(data=df_summary, x="hs/ant_ht", y="med_clr_win(sec)", col="wave_dp(deg)", kind="scatter")
    plt.xticks(fontsize=8) # set the new x-axis labels
    plt.yticks(fontsize=8) # set the new y-axis label size
    plt.savefig(dir_fig + "/hs_by_ant_ht_effects_med_clr_win.png", dpi=200)
    plt.clf()
    sns.relplot(data=df_summary, x="hs/ant_ht", y="mean_clr_win(sec)", col="wave_dp(deg)", kind="scatter")
    plt.xticks(fontsize=8) # set the new x-axis labels
    plt.yticks(fontsize=8) # set the new y-axis label size
    plt.savefig(dir_fig + "/hs_by_ant_ht_effects_mean_clr_win.png", dpi=200)


def report_dist_by_wave_len_effect_on_communication():
    f_report = open(dir_report + "/report.md", "w")
    df_summary = pd.read_csv(dir_report + "/summary.csv")
    df_summary["wave_dp(deg)"] = df_summary["wave_dp(rad)"] * 180.0/math.pi
    df_summary["dist/wave_len"] = df_summary["dist(m)"]/df_summary["peak_wave_len(m)"]
    # Scatter plot
    plt.clf()
    sns.relplot(data=df_summary, x="dist/wave_len", y="med_clr_win(sec)", col="wave_dp(deg)", kind="scatter")
    plt.xticks(fontsize=8) # set the new x-axis labels
    plt.yticks(fontsize=8) # set the new y-axis label size
    plt.savefig(dir_fig + "/dist_by_wave_len_effects_med_clr_win.png", dpi=200)
    plt.clf()
    sns.relplot(data=df_summary, x="dist/wave_len", y="mean_clr_win(sec)", col="wave_dp(deg)", kind="scatter")
    plt.xticks(fontsize=8) # set the new x-axis labels
    plt.yticks(fontsize=8) # set the new y-axis label size
    plt.savefig(dir_fig + "/dist_by_wave_len_effects_mean_clr_win.png", dpi=200)

def generate_report():
    report_coeff_variation()    
    report_hs_effect_on_communication()
    report_wave_dp_effect_on_communication()
    report_hs_by_ant_hight_effect_on_communication()
    report_dist_by_wave_len_effect_on_communication()



# summarise()
generate_report()