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

dist_range            = range(100.0, 1001.0, 100.0) # m
ant_ht_range          = range(1, 6, 1) # m
wave_hs_range         = range(1.0, 13.0, 1.0) # m
wave_dp_range         = range(0, 60, 30) + range(60, 91, 10) # deg   
rand_seed_range       = range(1, 4, 1)
message_length_range  = range(1, 11, 1)
message_header_length = 0.1 # sec

def evaluate_communication_quality(dist, ant_ht, wave_hs, wave_dp, rand_seed):
    simulation_time_step = 40 # milli sec
    fresnel_min_required_clearence = 60.0 # %
    results = []
    for message_length in message_length_range:
        file_name = "log_{}_{}_{}_{}_{}_.csv".format(int(dist), int(ant_ht), int(wave_hs), round(wave_dp), rand_seed)
        file_path = dir_sim_log.joinpath(file_name) 
        df = pd.read_csv(str(file_path))
        peak_wave_len = df["peak_wave_len(m)"].unique()[0]
        wave_dp_sim = round(df["wave_dp(rad)"].unique()[0] * 180/math.pi)
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
                "wave_dp_input":wave_dp,
                "wave_dp_sim":wave_dp_sim,
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
    file_name = "comms_{}_{}_{}_{}_.csv".format(int(dist), int(ant_ht), int(wave_hs), round(wave_dp))
    file_path = dir_comms.joinpath(file_name)
    f = open(str(file_path), "w")
    f.write("dist(m),ant_ht(m),hs(m),wave_dp_input(deg),wave_dp_sim(deg),seed,peak_wave_len(m),message_length(sec),header_length(sec),total_messages,uninterrupted_messages,uninterrupted_message_probability,data_transferred(sec)\n")
    for result in results:
        f.write("{dist},{ant_ht},{hs},{wave_dp_input},{wave_dp_sim},{seed},{peak_wave_len},{message_length},{header_length},{total_messages},{uninterrupted_messages},{uninterrupted_msg_prob:0.2f},{data_transferred:0.2f}\n".format(
                dist=result["dist"],
                ant_ht=result["ant_ht"],
                hs=result["hs"],
                wave_dp_input=result["wave_dp_input"],
                wave_dp_sim=result["wave_dp_sim"],
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


def simulate_comms():
    jobs = []
    # Create jobs
    for dist in dist_range:
        for ant_ht in ant_ht_range:
            for wave_hs in wave_hs_range:
                for wave_dp in wave_dp_range:
                    for rand_seed in rand_seed_range:
                        # Create all jobs
                        p = multiprocessing.Process(target=evaluate_communication_quality, args=(dist, ant_ht, wave_hs, wave_dp, rand_seed))
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
            print("processing job {} / {}".format(i, len(jobs)))
            process_pool.append(jobs[i])
            jobs[i].start()
        # Join threads
        for i in range(jobs_start_index, jobs_end_index):
            print("joining job {}".format(i))
            jobs[i].join()
        jobs_start_index = jobs_end_index

def run_all_comms_simulations():
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
        

# Simulate comms and generate report
simulate_comms()
run_all_comms_simulations()