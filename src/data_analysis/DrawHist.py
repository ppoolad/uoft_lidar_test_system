import matplotlib.pyplot as plt
import numpy as np

def read_memory_words(filename):
    #n=0
    with open(filename, 'r') as file:
        lines = file.readlines()
        n = len(lines)
    return [[int(line.strip()) for line in lines], n]

def draw_histogram(data, n, nbins=31*2):
    counts, bins, patches = plt.hist(data, bins=range(0,nbins), edgecolor='black')
    histogram_array = counts
    for i in range (0,nbins):
        print(f'{bins[i]}: {counts[i]}')
    plt.axhline(y=n//nbins, color='r', linestyle='-')
    plt.title('Histogram of The TDC Channel 1 Codes')
    plt.xlabel('TDC Code')
    plt.ylabel('# of Occurrences')
    plt.savefig('histogram.pdf')

    ##counts, bins = np.histogram(data, bins=nbins)
    print('n:', n)
    print('sum bins', sum(counts))
    expected_count = n // nbins
    differences = (histogram_array - expected_count)/expected_count

    plt.figure()
    plt.bar(bins[:-1], differences, width=np.diff(bins), edgecolor='black', align='edge')
    plt.axhline(y=0, color='r', linestyle='-')
    plt.title('(DNL) Difference Histogram of The TDC Channel 1 Codes')
    plt.xlabel('TDC Code')
    plt.ylabel('LSB')
    plt.savefig('difference_histogram.pdf')

    cumulative_sum = np.cumsum(differences)
    plt.figure()
    plt.plot(bins[:-1], cumulative_sum, marker='o')
    plt.axhline(y=0, color='r', linestyle='-')
    plt.title('Cumulative Sum of Differences (INL)')
    plt.xlabel('TDC Code')
    plt.ylabel('LSB')
    plt.savefig('cumulative_sum_histogram.pdf')


def main():
    filename = 'extracted_memory_words.txt'
    memory_words = read_memory_words(filename)
    draw_histogram(memory_words[0], memory_words[1])

if __name__ == '__main__':
    main()