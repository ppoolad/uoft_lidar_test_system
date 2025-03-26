import matplotlib.pyplot as plt
import numpy as np
import argparse
import os

def read_memory_words(filename, max_value = 2**6):
    #n=0
    def is_valid_number(line):
        number = int(line)
        return 0 <= number <= max_value
    with open(filename, 'r') as file:
        lines = file.readlines()
        n = len(lines)
        #lines_stripped = [int(line.strip('\n').strip()) for line in lines if line.strip()]
        #lines_filtered = [number for number in lines_stripped 
    return [[int(line.strip('\n').strip()) & 0xfffffffe for line in lines if is_valid_number(line.strip())], n]

def draw_histogram(data, n, nbins=31*2,out_file='histogram.pdf'):
    counts, bins, patches = plt.hist(data, bins=range(1,nbins,2), edgecolor='black')
    ##counts, bins, patches = plt.hist(data, bins=range(1,60),edgecolor='black')
    histogram_array = counts
    #for i in range (0,nbins-2):
     #   print(f'{bins[i]}: {counts[i]}')
    plt.axhline(y=n//nbins, color='r', linestyle='-')
    plt.title('Histogram of The TDC Channel 1 Codes')
    plt.xlabel('TDC Code')
    plt.ylabel('# of Occurrences')
    plt.savefig("outputs/" +  'ro_histogramx_' + out_file)

    ##counts, bins = np.histogram(data, bins=nbins)
    print('n:', n)
    print('sum bins', sum(counts))
    expected_count = n // (nbins-1)
    differences = (histogram_array - expected_count)/expected_count

    # plt.figure()
    # plt.bar(bins[:-1], differences, width=np.diff(bins), edgecolor='black', align='edge')
    # plt.axhline(y=0, color='r', linestyle='-')
    # plt.title('(DNL) Difference Histogram of The TDC Channel 1 Codes')
    # plt.xlabel('TDC Code')
    # plt.ylabel('LSB')
    # plt.savefig('ro_difference_' + out_file)

    # cumulative_sum = np.cumsum(differences)
    # plt.figure()
    # plt.plot(bins[:-1], cumulative_sum, marker='o')
    # plt.axhline(y=0, color='r', linestyle='-')
    # plt.title('Cumulative Sum of Differences (INL)')
    # plt.xlabel('TDC Code')
    # plt.ylabel('LSB')
    # plt.savefig('ro_cumulative_sum_' + out_file)


def main():
    parser = argparse.ArgumentParser(description="Display histograms for each data channel from the input file.")
    parser.add_argument("file", help="Path to the data file")
    args = parser.parse_args()
    filename = args.file
    # out_file = filename[:-4] + "_hist.pdf"
    out_file = os.path.basename(filename) + "_hist.pdf"
    memory_words = read_memory_words(filename)
    draw_histogram(memory_words[0], memory_words[1], out_file=out_file)

if __name__ == '__main__':
    main()
