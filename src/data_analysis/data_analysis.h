// data_analysis.h
#ifndef DATA_ANALYSIS_H
#define DATA_ANALYSIS_H

#include <string>
#include <vector>

using namespace std;

vector<int> extract_memory_words(const string &filename,
                                 const string &output_name, int packet_number,
                                 int adjust_start = 0, int nbins = 31 * 2);

int save_histogram(const vector<int> &memory_words, const string &output_name,
                   int nbins);

void run_histogram_script(const vector<int> &memory_words,
                          const string &output_name, int nbins);

#endif // DATA_ANALYSIS_H
