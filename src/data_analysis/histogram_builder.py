import matplotlib.pyplot as plt
import argparse
import numpy as np

def read_data(file_path):
    # Dictionary to store data for 6 channels.
    channels = {i: [] for i in range(6)}
    with open(file_path, 'r') as f:
        lines = [line.strip() for line in f if line.strip()]
    
    i = 0
    n_lines = len(lines)
    while i < n_lines:
        # Expect HEADER
        if lines[i] != "aa0aaaaa":
            #raise ValueError(f"Expected HEADER at line {i+1}, found: {lines[i]}")
            i += 1
            continue
            
        i += 1
        
        # Read 6 data points (one for each channel)
        start_tof = int(lines[i+5],16) & 0x00FFFFFF
        for ch in range(5):
            if i >= n_lines:
                #raise ValueError(f"Unexpected end of file when reading channel {ch} data.")
                break
            try:
                # make sure header is aa
                if lines[i][:2] != "aa":
                    i += 1
                    continue
                if lines[i] == "aaffffff" or lines[i] == "aa0aaaaa":
                    i += 1
                    break
                data_point = int(lines[i],16) & 0x00FFFFFF
            except ValueError:
                raise ValueError(f"Invalid data at line {i+1}: '{lines[i]}'")
            
            if data_point >= 0:
	            channels[ch].append(data_point-start_tof)
            i += 1

        i += 1
        # Expect FOOTER
        if i >= n_lines or lines[i] != "aaffffff":
            # verify that the footer is aaffffff
            if i >= n_lines:
                # done
                break
            if lines[i] != "aaffffff":
                print(f"Expected FOOTER at line {i+1}, found: '{lines[i]}'")
                #i += 1
                continue
            #raise ValueError(f"Expected FOOTER at line {i+1}, found: '{lines[i] if i < n_lines else 'None'}'")

        i += 1
        
    return channels

def plot_histograms(channels, out_file):
    fig, axes = plt.subplots(2, 3, figsize=(15, 10))
    axes = axes.ravel()
    for ch in range(6):
        axes[ch].hist(channels[ch], bins=range(1,64000,1000), color='skyblue', edgecolor='black')
        axes[ch].set_title(f"Channel {ch}")
        axes[ch].set_xlabel("Value")
        axes[ch].set_ylabel("Frequency")
    plt.tight_layout()
    plt.savefig("./" + out_file[:-4]+"_hist.pdf")

def main():
    parser = argparse.ArgumentParser(description="Display histograms for each data channel from the input file.")
    parser.add_argument("file", help="Path to the data file")
    args = parser.parse_args()
    
    channels = read_data(args.file)
    plot_histograms(channels,args.file)

if __name__ == "__main__":
    main()
