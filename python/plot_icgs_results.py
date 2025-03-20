import math
import pathlib
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from sklearn.tree import DecisionTreeClassifier, plot_tree
from sklearn.preprocessing import LabelEncoder
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report

# Directories
module_dir = pathlib.Path(__file__).parent.resolve()
root_dir = module_dir.parent
result_dir = root_dir.joinpath('results_icgs')


# deployment_result_file_path = result_dir.joinpath(f"asv_2_path.csv")
# df = pd.read_csv(deployment_result_file_path)
# plt.plot(df['x(m)'], df['y(m)'])
# plt.show()

for i in range(9):
    deployment_result_file_path = result_dir.joinpath(f"asv_{i}_path.csv")
    df = pd.read_csv(deployment_result_file_path)
    plt.scatter(df['x(m)'].to_list(), df['y(m)'].to_list())
plt.show()


deployment_result_file_path = result_dir.joinpath(f"formation_values.csv")
df = pd.read_csv(deployment_result_file_path)
values = []
values.append(df["current_value"].to_list()[0])
values = values + df["new_value"].to_list()
plt.plot(values)
plt.show()