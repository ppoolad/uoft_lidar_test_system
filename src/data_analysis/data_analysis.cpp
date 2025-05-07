/**
 * @file data_analysis.cpp
 * @brief This file contains functions for extracting memory words from a file
 * based on a specific packet number.
 *
 * The main function in this file reads a file containing hexadecimal values,
 * searches for a start-of-frame (SOF) marker, and extracts memory words from
 * the specified packet number following the SOF.
 *
 * @version 2.0
 * @date 2025-05-06
 * @author Qasim Li
 * @brief Added streaming functionality to read from a file and process the data
 * online/on the fly.
 */

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
// #include <yaml-cpp/yaml.h>
#include <chrono>
#include <mutex>
#include <thread>

#define SOF "aa0aaaaa"
#define HEADER "0xaa"
#define SUBTRACT_HEADER 8 + 2 + 2 + 2
#define MEMORY_WORD_LENGTH 6
#define DELAY                                                                  \
    100 // milliseconds - the search delay if no data is found before trying to
        // read again
#define MAX_DELAY                                                              \
    2500 // milliseconds - the maximum delay (time that data isn't read) before
         // exiting the program
#define HISTOGRAM_UPDATE_FREQUENCY                                             \
    1000 // number of samples before updating the histogram - preserve
         // performance

using namespace std;

// yaml parser that reads config.yaml file

// struct Config {
//     int sof;
//     int eof;
// } cfg;

// Config read_config(const string &config_file) {
//     YAML::Node config = YAML::LoadFile(config_file);
//     Config cfg;
//     cfg.sof = config["sof"].as<int>();
//     cfg.eof = config["eof"].as<int>();
//     return cfg;
// }

// calculate the histogram of the memory words
unordered_map<int, int> calculate_histogram(const vector<int> &memory_words) {
    unordered_map<int, int> histogram;

    for (int word : memory_words) {
        histogram[word]++;
    }

    return histogram;
}

/**
 * @brief Saves the histogram of memory words to a file and prints it to the
 * console.
 * @author Qasim Li, adapted from the original code by Pooya Poolad
 */
int save_histogram(const vector<int> &memory_words, const string &output_name) {

    // Calculate the histogram of the memory words
    unordered_map<int, int> histogram = calculate_histogram(memory_words);
    cout << "Memory word histogram:" << endl;
    for (const auto &entry : histogram) {
        cout << "0x" << hex << entry.first << ": " << dec << entry.second << endl;
    }

    // Store the content of the vector as integers in a text file
    ofstream output_file("outputs/" + output_name +
                         "_extracted_memory_words.txt");
    if (!output_file.is_open()) {
        cerr << "Error opening output file" << endl;
        return 1;
    }

    cout << "Extracted memory words:" << endl;
    for (int word : memory_words) {
        output_file << word << endl;
        // cout << "0x" << hex << word << endl;
    }

    output_file.close();

    return 0;
}

/**
 * @brief Runs the histogram script to generate a histogram from the memory
 * words.
 * @author Qasim Li
 */
void run_histogram_script(const vector<int> &memory_words,
                          const string &output_name) {
    string cmd = "python3 ./DrawHist.py ./outputs/" + output_name +
                 "_extracted_memory_words.txt";

    save_histogram(memory_words, output_name);
    system(cmd.c_str());
}

/**
 * @brief Extracts memory words from a file based on a specific packet number.
 *
 * This function reads a file line by line, searches for a start-of-frame (SOF)
 * marker, and extracts memory words from the specified packet number following
 * the SOF. The extracted memory words are returned as a vector of integers.
 *
 * @param filename The name of the file to read from.
 * @param packet_number The packet number to extract memory words from after the
 * SOF marker.
 * @return A vector of integers containing the extracted memory words.
 */
vector<int> extract_memory_words(const string &filename,
                                 const string &output_name, int packet_number,
                                 int adjust_start = 0) {
    // Read the configuration from the config.yaml file
    // ifstream file(filename);
    // if (!file.is_open()) {
    //     cerr << "Error opening file" << endl;
    //     return {};
    // }

    FILE *file = fopen(filename.c_str(), "r");
    if (!file) {
        cerr << "Error opening file" << endl;
        return {};
    }

    vector<int> memory_words;
    char line_c[256];
    string line;
    bool found = false;
    int line_counter = 0;
    int delay_counter = 0;
    while (true) {
        if (fgets(line_c, sizeof(line_c), file)) {
            delay_counter = 0;
            line = string(line_c);
            if (line.find(SOF) != string::npos) {
                found = true;
                // we found SOF, skip it since we want the n-th next line
                line_counter = 0;
                continue;
            }

            if (found) {
                line_counter++;
                if (line_counter == packet_number) {
                    cout << line << endl;

                    // check if the length is 8 charachters
                    if (line.length() < 8) {
                        cout << "Line length is less than 8 characters" << endl;
                        found = false;
                        continue;
                    }

                    string memory_word = line.substr(2, 8);
                    int start_word = 0;
                    if (adjust_start) {
                        int lines_to_ch6 = 6 - packet_number;
                        if (lines_to_ch6 > 0) {
                            for (int i = 0; i < lines_to_ch6; i++) {
                                // if (getline(file, line)) {
                                if (fgets(line_c, sizeof(line_c), file)) {
                                    line = string(line_c);
                                    // cout << "Skipped line: " << line <<
                                    // endl;
                                    line_counter++;
                                    if (line.length() >= 8)
                                        start_word =
                                            stoi(line.substr(2, 8), nullptr,
                                                 16); // this should be start
                                }
                            }
                        }
                    }

                    cout << "Memory word: " << memory_word << endl;
                    if (memory_word.length() >= 6) {
                        int memory_word_int = stoi(memory_word, nullptr, 16);
                        int start_word_int = start_word;
                        memory_word_int = 62 - start_word_int + memory_word_int;
                        memory_words.push_back(memory_word_int);
                        cout << "STOI: " << memory_words.back() << endl;

                        // here new memory word is added to the vector - update
                        // histogram.
                        if (memory_words.size() % HISTOGRAM_UPDATE_FREQUENCY ==
                            0) {
                            run_histogram_script(memory_words, output_name);
                        }
                    }

                    found = false;
                }
            }
        } else {
            if (feof(file)) {
                // no data right now, clear EOF and wait a bit
                clearerr(file);
                this_thread::sleep_for(chrono::milliseconds(DELAY));

                if (delay_counter > 0) {
                    cout << "No data found for " << dec << delay_counter << " ms"
                         << endl;
                }

                // check if we have reached the maximum delay
                delay_counter += DELAY;
                if (delay_counter > MAX_DELAY) {
                    cout << "Max delay reached, exiting..." << endl;
                    break;
                }

                continue;
            } else {
                // some other error
                cerr << "Error reading file" << endl;
                break;
            }
        }
    }

    // file.close();
    fclose(file);
    return memory_words;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0]
             << " <filename> <packet_number> <output_name> <start_adjust>"
             << endl;
        return 1;
    }

    string filename = argv[1];
    int packet_number = stoi(argv[2]);
    string output_name = argv[3];
    int adjust_start = 0;

    if (argc == 5) {
        adjust_start = stoi(argv[4]);
    }

    // cfg = read_config("config.yaml");

    vector<int> memory_words = extract_memory_words(
        filename, output_name, packet_number, adjust_start);
    if (memory_words.empty()) {
        cerr << "Error extracting memory words" << endl;
        return 1;
    }

    // save_histogram(memory_words, output_name);
    run_histogram_script(memory_words, output_name);

    return 0;
}
