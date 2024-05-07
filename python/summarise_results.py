import math
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import pathlib
import multiprocessing

module_dir = pathlib.Path(__file__).parent.resolve()
root_dir = module_dir.parent
dir_results = root_dir.joinpath("results")
dir_sim_log = dir_results.joinpath("simulation_logs")
dir_sim_log.mkdir(parents=True, exist_ok=True)
dir_report = dir_results.joinpath("report")
dir_report.mkdir(parents=True, exist_ok=True)
dir_comms = dir_results.joinpath("comms")
dir_comms.mkdir(parents=True, exist_ok=True)
dir_fig = dir_results.joinpath("fig")
dir_fig.mkdir(parents=True, exist_ok=True)

class Report:
    def __init__(self, file_path) -> None:
        self.f = open(file_path, "w")
    
    def write_heading(self, heading):
        self.f.write('''\n## {}\n'''.format(heading))

    def write_para(self, para):
        self.f.write("{}\n".format(para))
    
    def write_image(self, image_path):
        self.f.write("![text1]({})\n".format(image_path))

dist_range = np.arange(100.0, 601.0, 25.0) # m
ant_ht_range = np.arange(1, 6, 1) # m
wave_hs_range = np.arange(1.0, 19.0, 1.0) # m
wave_dp_range = np.arange(0, 91, 30) # deg   
rand_seed_range = range(1, 6, 1)
message_length_range = np.arange(1, 11, 1)
message_header_length = 0.1 # sec

def summarise_1():
    f = open(dir_report.joinpath("summary_1.csv"), "w")
    f.write("dist(m),ant_ht(m),hs(m),wave_dp(deg),peak_wave_len(m),dist_wave_len_ratio,seed,med_clr_win(sec),mean_clr_win(sec),std_clr_win(sec),med_ob_win(sec),mean_ob_win(sec),std_ob_win(sec)\n")
    for dist in dist_range:
        for ant_ht in ant_ht_range:
            for wave_hs in wave_hs_range:
                for wave_dp in wave_dp_range:
                    for rand_seed in rand_seed_range:
                        file_name = str(dir_sim_log) + "/log_{}_{}_{}_{}_{}_.csv".format(int(dist), int(ant_ht), int(wave_hs), round(wave_dp), rand_seed)
                        print("Summarising - ", file_name)
                        df = pd.read_csv(file_name)
                        # Calculate the communication window patterns
                        clear_window_lengths = [] # sec
                        obstructed_window_lengths = [] # sec
                        window_length = 0
                        fresnel_min_required_clearence = 60.0
                        previous_clearence_diff = None
                        is_this_the_first_window = True
                        for index, row in df.iterrows():
                            clearence = row["clearence(%)"]
                            clearence_diff = clearence - fresnel_min_required_clearence
                            if previous_clearence_diff == None:
                                previous_clearence_diff = clearence_diff
                                continue
                            else:
                                time_step_size  = 40.0/1000 # sec
                                window_length += time_step_size 
                                if previous_clearence_diff * clearence_diff < 0.0:
                                    # Clearence window changing
                                    if is_this_the_first_window:
                                        # Ignore the first window since it is partial
                                        is_this_the_first_window = False
                                    else:
                                        if previous_clearence_diff > 0.0:
                                            clear_window_lengths.append(window_length)
                                        elif previous_clearence_diff < 0.0:
                                            obstructed_window_lengths.append(window_length)
                                        else:
                                            # This can never be the case since "previous_clearence_diff * clearence_diff < 0.0"
                                            # i.e both previous_clearence_diff and clearence_diff are non-zero values. 
                                            pass
                                    previous_clearence_diff = clearence_diff
                                    window_length = 0.0
                                else:
                                    previous_clearence_diff = clearence_diff
                        # # Enter the final reading too
                        # if(window_length != 0.0):
                        #     if previous_clearence_diff > 0.0:
                        #         clear_window_lengths.append(window_length)
                        #     else:
                        #         obstructed_window_lengths.append(window_length)
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


def summarise_2():
    summary_1 = dir_report.joinpath("summary_1.csv")
    df = pd.read_csv(summary_1)
    f = open(dir_report.joinpath("summary_2.csv"), "w")
    f.write("dist(m),ant_ht(m),hs(m),wave_dp(deg),peak_wave_len(m),dist_wave_len_ratio,med_clr_win(sec),mean_clr_win(sec),med_ob_win(sec),mean_ob_win(sec)\n")
    groups = df.groupby(["dist(m)", "ant_ht(m)", "hs(m)", "wave_dp(deg)"])
    for (dist, ant_ht, hs, wave_dp), group in groups:
        peak_wave_len = group["peak_wave_len(m)"].mean()
        dist_wave_len_ratio = group["dist_wave_len_ratio"].mean()
        med_clr_win  = np.nan if np.isnan(group["med_clr_win(sec)"]).any() else group["med_clr_win(sec)"].mean()
        mean_clr_win = np.nan if np.isnan(group["mean_clr_win(sec)"]).any() else group["med_clr_win(sec)"].mean()
        med_ob_win   = np.nan if np.isnan(group["med_ob_win(sec)"]).any() else group["med_clr_win(sec)"].mean()
        mean_ob_win  = np.nan if np.isnan(group["mean_ob_win(sec)"]).any() else group["med_clr_win(sec)"].mean()
        f.write("{dist},{ant_ht},{hs},{wave_dp},{peak_wave_len},{dist_wave_len_ratio},{med_clr_win},{mean_clr_win},{med_ob_win},{mean_ob_win}\n".format(
            dist=dist,
            ant_ht=ant_ht,
            hs=hs,
            wave_dp=wave_dp,
            peak_wave_len=peak_wave_len,
            dist_wave_len_ratio=dist_wave_len_ratio,
            med_clr_win=med_clr_win,
            mean_clr_win=mean_clr_win,
            med_ob_win=med_ob_win,
            mean_ob_win=mean_ob_win
        ))
    f.close()


def report_coeff_variation(report):
    report.write_heading("Variations in different runs of the experiment")
    f_coeff_variation = open(dir_report.joinpath("coeff_variation.csv"), "w")
    f_coeff_variation.write("dist(m),ant_ht(m),hs(m),wave_dp(deg),med_clr_wins,cv_med_clr_win(%),cv_mean_clr_win(%),cv_med_ob_win(%),cv_mean_ob_win(%)\n")
    file_name = dir_report.joinpath("summary_1.csv")
    df_summary = pd.read_csv(file_name)
    groups = df_summary.groupby(["dist(m)", "ant_ht(m)", "hs(m)", "wave_dp(deg)"])
    # Compute the coefficient of variations
    for (dist, ant_ht, hs, wave_dp), group in groups:
        cv_med_clr_win  = np.nan if np.isnan(group["med_clr_win(sec)"]).any() else group["med_clr_win(sec)"].std()/group["med_clr_win(sec)"].mean() * 100
        cv_mean_clr_win = np.nan if np.isnan(group["mean_clr_win(sec)"]).any() else group["mean_clr_win(sec)"].std()/group["mean_clr_win(sec)"].mean() * 100
        cv_med_ob_win   = np.nan if np.isnan(group["med_ob_win(sec)"]).any() else group["med_ob_win(sec)"].std()/group["med_ob_win(sec)"].mean() * 100
        cv_mean_ob_win  = np.nan if np.isnan(group["mean_ob_win(sec)"]).any() else group["mean_ob_win(sec)"].std()/group["mean_ob_win(sec)"].mean() * 100
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
    df = pd.read_csv(dir_report.joinpath("coeff_variation.csv"))
    columns=["cv_med_clr_win(%)", "cv_mean_clr_win(%)", "cv_med_ob_win(%)", "cv_mean_ob_win(%)"]
    ax = sns.boxplot(data=df[columns], showfliers=False)
    ax.set(ylabel="coefficient of variation (%)")
    plt.xticks(fontsize=8) # set the new x-axis labels
    plt.yticks(fontsize=8) # set the new y-axis label size
    plt.savefig(dir_fig.joinpath("coefficient_of_variations.png"), dpi=200)
    report.write_image(dir_fig.joinpath("coefficient_of_variations.png"))


def report_hs_effect_on_communication(report):
    report.write_heading("Effect of wave height on communication")
    report.write_para("The figs below shows the variation of communication window w.r.t sea state." + 
                      "The sea state in these figs are expressed as ratio of significant wave height to antenna height.")
    df_summary = pd.read_csv(dir_report.joinpath("summary_2.csv"))
    df_summary["hs/ant_ht"] = df_summary["hs(m)"]/df_summary["ant_ht(m)"]
    bins = pd.IntervalIndex.from_tuples([(0, 1), (1, 2), (2, 3), (3,4), (4, 5), (5, 6), (6, 7), (7, 8), (8, 9)])
    df_summary["bins"] = pd.cut(df_summary["hs/ant_ht"], bins)
    # Plot
    plt.clf()
    ax = sns.boxenplot(data=df_summary, x="bins", y="med_clr_win(sec)", showfliers=False)
    ax.set(xlabel="wave ht / antenna ht", ylabel="median clear window (sec)")
    plt.xticks(fontsize=8) # set the new x-axis labels
    plt.yticks(fontsize=8) # set the new y-axis label size
    file_fig = dir_fig.joinpath("hs_effects_med_clr_win.png")
    plt.savefig(file_fig, dpi=200)
    report.write_image(file_fig)
    plt.clf()
    ax = sns.boxenplot(data=df_summary, x="bins", y="mean_clr_win(sec)", showfliers=False)
    ax.set(xlabel="wave ht / antenna ht", ylabel="mean clear window (sec)")
    plt.xticks(fontsize=8) # set the new x-axis labels
    plt.yticks(fontsize=8) # set the new y-axis label size
    file_fig = dir_fig.joinpath("hs_effects_mean_clr_win.png")
    plt.savefig(file_fig, dpi=200)
    report.write_image(file_fig)



def report_wave_dp_effect_on_communication(report):
    report.write_heading("Effect of wave heading on communication")
    report.write_para("The figs below shows the variation of communication window w.r.t wave heading." + 
                      "The communication window is maximum when the perdominant wave heading is perpendicular to the line joining the two antennas.")
    df_summary = pd.read_csv(dir_report.joinpath("summary_2.csv"))
    df_summary["rel_wave_dp(deg)"] = 90 - df_summary["wave_dp(deg)"]
    # Plot
    plt.clf()
    ax = sns.boxenplot(data=df_summary, y="med_clr_win(sec)", x="rel_wave_dp(deg)", showfliers=False)
    ax.set(xlabel="wave heading relative to the line joining the antennas (deg)", ylabel="median clear window (sec)")
    plt.xticks(fontsize=8) # set the new x-axis labels
    plt.yticks(fontsize=8) # set the new y-axis label size
    file_fig = dir_fig.joinpath("wave_dp_effects_med_clr_win.png")
    report.write_image(file_fig)
    plt.savefig(file_fig, dpi=200)
    plt.clf()
    ax = sns.boxenplot(data=df_summary, y="mean_clr_win(sec)", x="rel_wave_dp(deg)", showfliers=False)
    ax.set(xlabel="wave heading relative to the line joining the antennas (deg)", ylabel="mean clear window (sec)")
    plt.xticks(fontsize=8) # set the new x-axis labels
    plt.yticks(fontsize=8) # set the new y-axis label size
    file_fig = dir_fig.joinpath("wave_dp_effects_mean_clr_win.png")
    report.write_image(file_fig)
    plt.savefig(file_fig, dpi=200)


def report_dist_by_wave_len_effect_on_communication(report):
    report.write_heading("Effect of wave length on communication")
    df_summary = pd.read_csv(dir_report.joinpath("summary_2.csv"))
    df_summary["rel_wave_dp(deg)"] = 90 - df_summary["wave_dp(deg)"]
    df_summary["med_clr_win/max_win"] = df_summary["med_clr_win(sec)"]/df_summary["med_clr_win(sec)"].max()
    df_summary["mean_clr_win/max_win"] = df_summary["mean_clr_win(sec)"]/df_summary["mean_clr_win(sec)"].max()
    df_summary["dist/wave_len"] = None
    for i, row in df_summary.iterrows():
        wave_dp_rad = row['wave_dp(deg)']*math.pi/180
        if wave_dp_rad == 0:
            df_summary.loc[i, "dist/wave_len"] = row["dist(m)"]/row["peak_wave_len(m)"]    
        else:
            df_summary.loc[i, "dist/wave_len"] = row["dist(m)"]/(row["peak_wave_len(m)"]/math.sin(wave_dp_rad))
    # Plot
    plt.clf()
    bins = pd.IntervalIndex.from_tuples([(0, 0.2), (0.2, 0.6), (0.6, 0.8), (0.8, 1.0), 
                                         (1.0, 1.2), (1.2, 1.4), (1.4, 1.6), (1.6, 1.8), (1.8, 2.0),
                                         (2.0, 2.2), (2.2, 2.4), (2.4, 2.6), (2.6, 2.8), (2.8, 3.0)])
    df_summary["bins"] = pd.cut(df_summary["dist/wave_len"], bins)
    ax = sns.boxenplot(data=df_summary, hue="bins", y="med_clr_win/max_win", x="rel_wave_dp(deg)", showfliers=False)
    plt.legend(title='distance / wave length')
    sns.move_legend(ax, "upper left", bbox_to_anchor=(1, 1))
    ax.set(xlabel="distance / wave length", ylabel="median clear window (sec)")
    plt.xticks(fontsize=6) # set the new x-axis labels
    plt.yticks(fontsize=6) # set the new y-axis label size
    plt.tight_layout()
    file_fig = dir_fig.joinpath("dist_by_wave_len_effects_med_clr_win.png")
    plt.savefig(file_fig, dpi=200)
    report.write_image(file_fig)
    plt.clf()
    ax = sns.boxenplot(data=df_summary, hue="bins", y="mean_clr_win/max_win", x="rel_wave_dp(deg)", showfliers=False)
    plt.legend(title='distance / wave length')
    sns.move_legend(ax, "upper left", bbox_to_anchor=(1, 1))
    ax.set(xlabel="distance / wave length", ylabel="mean clear window (sec)")
    plt.xticks(fontsize=6) # set the new x-axis labels
    plt.yticks(fontsize=6) # set the new y-axis label size
    plt.tight_layout()
    file_fig = dir_fig.joinpath("dist_by_wave_len_effects_mean_clr_win.png")
    plt.savefig(file_fig, dpi=200)
    report.write_image(file_fig)


def evaluate_communication_quality(dist, ant_ht, wave_hs, wave_dp):
    simulation_time_step = 40 # milli sec
    fresnel_min_required_clearence = 60.0 # %
    results = []
    for message_length in message_length_range:
        for rand_seed in rand_seed_range:
            file_name = "/log_{}_{}_{}_{}_{}_.csv".format(int(dist), int(ant_ht), int(wave_hs), round(wave_dp), rand_seed)
            file_path = str(dir_sim_log) + "/log_{}_{}_{}_{}_{}_.csv".format(int(dist), int(ant_ht), int(wave_hs), round(wave_dp), rand_seed)
            df = pd.read_csv(file_path)
            peak_wave_len = df["peak_wave_len(m)"].unique()[0]
            is_comm_quality_good = True
            count_sim_time_steps_in_current_message = 0
            total_sim_time_steps_per_message = message_length * 1000/ simulation_time_step
            count_messages_transmitted = 0
            count_uninterrupted_messages = 0
            for index, row in df.iterrows():
                clearence = row["clearence(%)"]
                if count_sim_time_steps_in_current_message == total_sim_time_steps_per_message:
                    count_messages_transmitted += 1
                    count_uninterrupted_messages += 1 if is_comm_quality_good else 0
                    # reset the counters and flags
                    count_sim_time_steps_in_current_message = 0
                    is_comm_quality_good = True
                is_comm_quality_good = is_comm_quality_good and (clearence >= fresnel_min_required_clearence)
                count_sim_time_steps_in_current_message += 1 
            # Record stats
            data_transferred = (message_length - message_header_length) * count_uninterrupted_messages
            result = {"dist":dist,
                    "ant_ht":ant_ht,
                    "hs":wave_hs,
                    "wave_dp":wave_dp,
                    "seed":rand_seed,
                    "peak_wave_len":peak_wave_len,
                    "message_length":message_length,
                    "total_messages":count_messages_transmitted,
                    "uninterrupted_messages":count_uninterrupted_messages,
                    "uninterrupted_msg_prob":count_uninterrupted_messages/count_messages_transmitted,
                    "data_transferred":data_transferred
                    }
            results.append(result)
    # Write result to file
    file_name = "/comms_{}_{}_{}_{}_.csv".format(int(dist), int(ant_ht), int(wave_hs), round(wave_dp))
    file_path = str(dir_comms) + file_name
    f = open(file_path, "w")
    f.write("dist(m),ant_ht(m),hs(m),wave_dp(deg),seed,peak_wave_len(m),message_length(sec),header_length(sec),total_messages,uninterrupted_messages,uninterrupted_message_probability,data_transferred(sec)\n")
    for result in results:
        f.write("{dist},{ant_ht},{hs},{wave_dp},{seed},{peak_wave_len},{message_length},{header_length},{total_messages},{uninterrupted_messages},{uninterrupted_msg_prob:0.2f},{data_transferred:0.2f}\n".format(
                dist=result["dist"],
                ant_ht=result["ant_ht"],
                hs=result["hs"],
                wave_dp=result["wave_dp"],
                seed=result["seed"],
                peak_wave_len=result["peak_wave_len"],
                message_length=result["message_length"],
                header_length=message_header_length,
                total_messages=result["total_messages"],
                uninterrupted_messages=result["uninterrupted_messages"],
                uninterrupted_msg_prob=result["uninterrupted_msg_prob"],
                data_transferred=result["data_transferred"]
                ))  
    f.close()


def run_comms():
    jobs = []
    # Create jobs
    for dist in dist_range:
        for ant_ht in ant_ht_range:
            for wave_hs in wave_hs_range:
                for wave_dp in wave_dp_range:
                    # Create all jobs
                    p = multiprocessing.Process(target=evaluate_communication_quality, args=(dist, ant_ht, wave_hs, wave_dp))
                    jobs.append(p)
    # Process jobs
    max_pool_size = multiprocessing.cpu_count()
    jobs_start_index = 0
    jobs_end_index = 0
    while jobs_end_index < len(jobs):
        print("\n")
        count_jobs_remaining = len(jobs) - jobs_end_index
        jobs_end_index = jobs_start_index + min(max_pool_size, count_jobs_remaining)
        process_pool = []
        # Spawn threads
        for i in range(jobs_start_index, jobs_end_index):
            print("proceccsing job {}".format(i))
            process_pool.append(jobs[i])
            jobs[i].start()
        # Join threads
        for i in range(jobs_start_index, jobs_end_index):
            print("joining job {}".format(i))
            jobs[i].join()
        jobs_start_index = jobs_end_index

def report_comms_quality(report):
    # Read all csv files into a single dataframe
    df_list = []
    for dist in dist_range:
        for ant_ht in ant_ht_range:
            for wave_hs in wave_hs_range:
                for wave_dp in wave_dp_range:
                    file_name = "/comms_{}_{}_{}_{}_.csv".format(int(dist), int(ant_ht), int(wave_hs), round(wave_dp))
                    file = str(dir_comms) + file_name
                    print(file)
                    df = pd.read_csv(str(file), index_col=None)
                    df_list.append(df)
    df = pd.concat(df_list, axis=0, ignore_index=True)
    # Plot
    df["hs/ant_ht"] = df["hs(m)"]/df["ant_ht(m)"]
    bins = pd.IntervalIndex.from_tuples([(0, 0.5), (0.5, 1), (1, 2), (2, 3), (3,4), (4, 5), (5, 6), (6, 7), (7, 8), (8, 9)])
    df["bins"] = pd.cut(df["hs/ant_ht"], bins)
    # Plot the effects of hs on data transfer.
    plt.clf()
    ax = sns.boxplot(data=df, x="bins", y="data_transferred(sec)", hue="message_length(sec)", showfliers=False)
    ax.set(xlabel="wave ht / antenna ht", ylabel="data volume (sec)")
    plt.xticks(fontsize=7) # set the new x-axis labels
    plt.yticks(fontsize=7) # set the new y-axis label size
    file_fig = dir_fig.joinpath("comms_hs_effect_data_vol_hue.png")
    plt.savefig(file_fig, dpi=200)
    report.write_image(file_fig)
    # Plot the effects of hs on data transfer for a given ant_ht.
    plt.clf()
    ax = sns.boxplot(data=df.loc[df["ant_ht(m)"] == 2.0], x="hs(m)", y="data_transferred(sec)", hue="message_length(sec)", showfliers=False)
    ax.set(xlabel="wave height (m)", ylabel="data volume (sec)")
    plt.xticks(fontsize=7) # set the new x-axis labels
    plt.yticks(fontsize=7) # set the new y-axis label size
    file_fig = dir_fig.joinpath("comms_hs_effect_data_vol_hue_2.png")
    plt.savefig(file_fig, dpi=200)
    report.write_image(file_fig)
    # Plot the effects of hs on data transfer for a given ant_ht and dist between ants.
    plt.clf()
    ax = sns.boxplot(data=df.loc[(df["ant_ht(m)"] == 2.0) &  (df["dist(m)"] == 200)], x="hs(m)", y="data_transferred(sec)", hue="message_length(sec)")
    ax.set(xlabel="wave height (m)", ylabel="data volume (sec)")
    plt.xticks(fontsize=7) # set the new x-axis labels
    plt.yticks(fontsize=7) # set the new y-axis label size
    file_fig = dir_fig.joinpath("comms_hs_effect_data_vol_hue_3.png")
    plt.savefig(file_fig, dpi=200)
    report.write_image(file_fig)
    # Plot the effects of hs on data transfer for a given ant_ht and dist between ants.
    plt.clf()
    ax = sns.boxplot(data=df.loc[(df["ant_ht(m)"] == 3.0) &  (df["dist(m)"] == 200)], x="hs(m)", y="data_transferred(sec)", hue="message_length(sec)")
    ax.set(xlabel="wave height (m)", ylabel="data volume (sec)")
    plt.xticks(fontsize=7) # set the new x-axis labels
    plt.yticks(fontsize=7) # set the new y-axis label size
    file_fig = dir_fig.joinpath("comms_hs_effect_data_vol_hue_4.png")
    plt.savefig(file_fig, dpi=200)
    report.write_image(file_fig)
    # Plot the effects of hs on data transfer for a given ant_ht and dist between ants.
    plt.clf()
    ax = sns.boxplot(data=df.loc[(df["ant_ht(m)"] == 3.0)], x="hs(m)", y="data_transferred(sec)", hue="dist(m)", showfliers=False)
    ax.set(xlabel="wave height (m)", ylabel="data volume (sec)")
    plt.xticks(fontsize=7) # set the new x-axis labels
    plt.yticks(fontsize=7) # set the new y-axis label size
    file_fig = dir_fig.joinpath("comms_hs_effect_data_vol_hue_5.png")
    plt.savefig(file_fig, dpi=200)
    report.write_image(file_fig)
        

def generate_report(report):
    # report_coeff_variation(report)    
    # report_hs_effect_on_communication(report)
    # report_wave_dp_effect_on_communication(report)
    # report_dist_by_wave_len_effect_on_communication(report)
    run_comms()
    # report_comms_quality(report)


report = Report(dir_report.joinpath("report.md"))
# summarise_1()
# summarise_2()
generate_report(report)

