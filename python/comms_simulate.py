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
dir_comms = dir_results.joinpath("comms")
dir_comms.mkdir(parents=True, exist_ok=True)
dir_fig = dir_results.joinpath("fig")
dir_fig.mkdir(parents=True, exist_ok=True)

dist_range            = range(500, 5001, 500) # m
ant_ht_range          = range(1, 6, 1) # m
wave_hs_range         = range(1, 10, 2) # m
wave_dp_range         = range(0, 91, 30) # deg   
rand_seed_range       = range(1, 4, 1)
message_length_range  = range(1, 11, 1)
message_header_length = 0.1 # sec
total_simulation_time = 15 * 60 #sec

def evaluate_communication_quality_fresnel_zones(dist, ant_ht, wave_hs, wave_dp):
    simulation_time_step = 40 # milli sec
    fresnel_min_required_clearance = 60.0 # %
    results = []
    for message_length in message_length_range:
        for rand_seed in rand_seed_range:
            file_name = "log_{}_{}_{}_{}_{}_.csv".format(int(dist), int(ant_ht), int(wave_hs), round(wave_dp), rand_seed)
            file_path = dir_sim_log.joinpath(file_name) 
            df = pd.read_csv(str(file_path))
            peak_freq = df["peak_freq(hz)"].unique()[0]
            peak_wave_len = 9.81/(2 * math.pi * peak_freq**2) # m
            wave_dp_sim = round(df["wave_dp(rad)"].unique()[0] * 180/math.pi)
            is_comm_quality_good = True
            count_sim_time_steps_in_current_message = 0
            total_sim_time_steps_per_message = message_length * 1000/ simulation_time_step
            num_messages = 0
            num_uninterrupted_messages = 0
            for index, row in df.iterrows():
                clearance = row["clearance(%)"]
                if count_sim_time_steps_in_current_message == total_sim_time_steps_per_message:
                    num_messages += 1
                    num_uninterrupted_messages += 1 if is_comm_quality_good else 0
                    # reset the counters and flags
                    count_sim_time_steps_in_current_message = 0
                    is_comm_quality_good = True
                is_comm_quality_good = is_comm_quality_good and (clearance >= fresnel_min_required_clearance)
                count_sim_time_steps_in_current_message += 1 
            # Record stats
            uninterrupted_time = ((message_length - message_header_length) * num_uninterrupted_messages) / total_simulation_time * 100 # %
            result = {"dist":dist,
                    "ant_ht":ant_ht,
                    "hs":wave_hs,
                    "wave_dp_input":wave_dp,
                    "wave_dp_sim":wave_dp_sim,
                    "seed":rand_seed,
                    "peak_freq":peak_freq,
                    "peak_wave_len":peak_wave_len,
                    "message_length":message_length,
                    "num_messages":num_messages,
                    "num_uninterrupted_messages":num_uninterrupted_messages,
                    "uninterrupted_msg_prob":num_uninterrupted_messages/num_messages,
                    "uninterrupted_time":uninterrupted_time
                    }
            results.append(result)
    # Write result to file
    file_name = "comms_{}_{}_{}_{}_.csv".format(int(dist), int(ant_ht), int(wave_hs), round(wave_dp))
    file_path = dir_comms.joinpath(file_name)
    f = open(str(file_path), "w")
    f.write("dist(m),ant_ht(m),hs(m),wave_dp_input(deg),wave_dp_sim(deg),seed,peak_freq(hz),peak_wave_len(m),message_length(sec),header_length(sec),num_messages,num_uninterrupted_messages,uninterrupted_message_probability,uninterrupted_time(%)\n")
    for result in results:
        f.write("{dist},{ant_ht},{hs},{wave_dp_input},{wave_dp_sim},{seed},{peak_freq},{peak_wave_len},{message_length},{header_length},{num_messages},{num_uninterrupted_messages},{uninterrupted_msg_prob:0.2f},{uninterrupted_time:0.2f}\n".format(
                dist=result["dist"],
                ant_ht=result["ant_ht"],
                hs=result["hs"],
                wave_dp_input=result["wave_dp_input"],
                wave_dp_sim=result["wave_dp_sim"],
                seed=result["seed"],
                peak_freq=result["peak_freq"],
                peak_wave_len=result["peak_wave_len"],
                message_length=result["message_length"],
                header_length=message_header_length,
                num_messages=result["num_messages"],
                num_uninterrupted_messages=result["num_uninterrupted_messages"],
                uninterrupted_msg_prob=result["uninterrupted_msg_prob"],
                uninterrupted_time=result["uninterrupted_time"]
                ))  
    f.close()


def evaluate_communication_quality_diffraction_and_path_loss(dist, ant_ht, wave_hs, wave_dp):
    results = []
    for rand_seed in rand_seed_range:
        file_name = "log_{}_{}_{}_{}_{}_.csv".format(int(dist), int(ant_ht), int(wave_hs), round(wave_dp), rand_seed)
        file_path = dir_sim_log.joinpath(file_name) 
        df = pd.read_csv(str(file_path))
        peak_freq = df["peak_freq(hz)"].unique()[0]
        peak_wave_len = 9.81/(2 * math.pi * peak_freq**2) # m
        wave_dp_sim = round(df["wave_dp(rad)"].unique()[0] * 180/math.pi)
        path_loss = df["path_loss(dB)"].mean()
        mean_total_loss = df["total_loss(dB)"].mean()
        max_total_loss = df["total_loss(dB)"].max()
        mean_diffraction_loss = mean_total_loss - path_loss
        max_diffraction_loss = max_total_loss - path_loss
        # Record stats
        result = {"dist":dist,
                "ant_ht":ant_ht,
                "hs":wave_hs,
                "wave_dp_input":wave_dp,
                "wave_dp_sim":wave_dp_sim,
                "seed":rand_seed,
                "peak_freq":peak_freq,
                "peak_wave_len":peak_wave_len,
                "path_loss":path_loss,
                "mean_diffraction_loss":mean_diffraction_loss,
                "max_diffraction_loss":max_diffraction_loss,
                "mean_total_loss":mean_total_loss
                }
        results.append(result)
    # Write result to file
    file_name = "comms_{}_{}_{}_{}_.csv".format(int(dist), int(ant_ht), int(wave_hs), round(wave_dp))
    file_path = dir_comms.joinpath(file_name)
    f = open(str(file_path), "w")
    f.write("dist(m),ant_ht(m),hs(m),wave_dp_input(deg),wave_dp_sim(deg),seed,peak_freq(hz),peak_wave_len(m),path_loss(dB),mean_diffraction_loss(dB),max_diffraction_loss(dB),mean_total_loss(dB)\n")
    for result in results:
        f.write("{dist},{ant_ht},{hs},{wave_dp_input},{wave_dp_sim},{seed},{peak_freq},{peak_wave_len},{path_loss},{mean_diffraction_loss},{max_diffraction_loss},{mean_total_loss}\n".format(
                dist=result["dist"],
                ant_ht=result["ant_ht"],
                hs=result["hs"],
                wave_dp_input=result["wave_dp_input"],
                wave_dp_sim=result["wave_dp_sim"],
                seed=result["seed"],
                peak_freq=result["peak_freq"],
                peak_wave_len=result["peak_wave_len"],
                path_loss=result["path_loss"],
                mean_diffraction_loss=result["mean_diffraction_loss"],
                max_diffraction_loss=result["max_diffraction_loss"],
                mean_total_loss=result["mean_total_loss"],
                ))  
    f.close()


def simulate_comms(method):
    jobs = []
    # Create jobs
    for dist in dist_range:
        for ant_ht in ant_ht_range:
            for wave_hs in wave_hs_range:
                for wave_dp in wave_dp_range:
                    # Create all jobs
                    p = None
                    if method == "fresnel_zones":
                        p = multiprocessing.Process(target=evaluate_communication_quality_fresnel_zones, args=(dist, ant_ht, wave_hs, wave_dp))
                    if method == "diffraction_and_path_loss":
                        p = multiprocessing.Process(target=evaluate_communication_quality_diffraction_and_path_loss, args=(dist, ant_ht, wave_hs, wave_dp))
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
       

# Simulate comms
method = "diffraction_and_path_loss"
simulate_comms(method)