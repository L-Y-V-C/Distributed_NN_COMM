import pandas as pd
import numpy as np

def split_dataset(csv_path, parts=3):
    df = pd.read_csv(csv_path)
    splits = np.array_split(df, parts)
    return splits