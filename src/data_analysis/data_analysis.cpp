/**
 * @file data_analysis.cpp
 * @brief This file contains functions for extracting memory words from a file based on a specific packet number.
 *
 * The main function in this file reads a file containing hexadecimal values, searches for a start-of-frame (SOF) marker,
 * and extracts memory words from the specified packet number following the SOF.
 */


#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <string>
//#include <yaml-cpp/yaml.h>

#define SOF "aaaaaa"
#define HEADER "0xaa"
#define MEMORY_WORD_LENGTH 8

// yaml parser that reads config.yaml file

// struct Config {
//     int sof;
//     int eof;
// } cfg;

// Config read_config(const std::string &config_file) {
//     YAML::Node config = YAML::LoadFile(config_file);
//     Config cfg;
//     cfg.sof = config["sof"].as<int>();
//     cfg.eof = config["eof"].as<int>();
//     return cfg;
// }

 /**
 * @brief Extracts memory words from a file based on a specific packet number.
 *
 * This function reads a file line by line, searches for a start-of-frame (SOF) marker, and extracts memory words
 * from the specified packet number following the SOF. The extracted memory words are returned as a vector of integers.
 *
 * @param filename The name of the file to read from.
 * @param packet_number The packet number to extract memory words from after the SOF marker.
 * @return A vector of integers containing the extracted memory words.
 */

std::vector<int> extract_memory_words(const std::string &filename, int packet_number) {
    // Read the configuration from the config.yaml file
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return {};
    }

    std::vector<int> memory_words;
    std::string line;
    bool found = false;
    int line_counter = 0;
    while (std::getline(file, line)) {
        if (line.find(SOF) != std::string::npos) {
            found = true;
            // we found SOF, skip it since we want the n-th next line
            line_counter = 0;
            continue;
        }
        if (found) {
            line_counter++;
            if(line_counter == packet_number) {
            std::string memory_word = line.substr(12, MEMORY_WORD_LENGTH-2);
            memory_words.push_back(std::stoi(memory_word, nullptr, 16));
            found = false;
            }
        }
    }

    file.close();
    return memory_words;
}

//calculate the histogram of the memory words
std::unordered_map<int, int> calculate_histogram(const std::vector<int> &memory_words) {
    std::unordered_map<int, int> histogram;
    for (int word : memory_words) {
        histogram[word]++;
    }
    return histogram;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <filename> <packet_number> <output_name>" << std::endl;
        return 1;
    }
    std::string filename = argv[1];
    int packet_number = std::stoi(argv[2]);
    std::string output_name = argv[3];

    //cfg = read_config("config.yaml");

    std::vector<int> memory_words = extract_memory_words(filename, packet_number);
    if (memory_words.empty()) {
        std::cerr << "Error extracting memory words" << std::endl;
        return 1;
    }

    // Calculate the histogram of the memory words
    std::unordered_map<int, int> histogram = calculate_histogram(memory_words);
    std::cout << "Memory word histogram:" << std::endl;
    for (const auto &entry : histogram) {
        std::cout << "0x" << std::hex << entry.first << ": " << entry.second << std::endl;
    }

    // Store the content of the vector as integers in a text file
    std::ofstream output_file(output_name+"extracted_memory_words.txt");
    if (!output_file.is_open()) {
        std::cerr << "Error opening output file" << std::endl;
        return 1;
    }

    std::cout << "Extracted memory words:" << std::endl;
    for (int word : memory_words) {
        output_file << word << std::endl;
        //std::cout << "0x" << std::hex << word << std::endl;
    }

    output_file.close();
    return 0;
}
