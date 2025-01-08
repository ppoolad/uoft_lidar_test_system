#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "configs.h"

#define MAX_LINE_LENGTH 256
#define MAX_KEY_LENGTH 128
#define MAX_VALUE_LENGTH 128

// reads config.text and sets the global configuration struct
Config parse_config(std::string filename) {
    
    Config config;

    // Open the file
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: could not open file " << filename << std::endl;
        return config;
    }

    // Read the file line by line
    std::string line;
    while (std::getline(file, line)) {
        // Split the line into key and value
        std::string key, value;
        std::istringstream iss(line);
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            // Remove leading and trailing whitespaces
            key.erase(0, key.find_first_not_of(" \t"));
            // Set the configuration
            if (key == "hpc1_chip_name") {
                config.hpc1_chip_name = value;
            } else if (key == "led_chip_name") {
                config.led_chip_name = value;
            } else if (key == "output_file") {
                config.output_file = value;
            } else if (key == "rx_dev_fifo") {
                config.rx_dev_fifo = value;
            } else if (key == "nbits_rx") {
                config.nbits_rx = std::stoi(value);
            } else if (key == "runtime"){
                config.runtime = std::stoi(value);
            } else if (key == "snr"){
                config.snr = std::stof(value);
            } else if (key == "kernel_means") {
                std::istringstream iss(value);
                for (int i = 0; i < 4; i++) {
                    iss >> config.kernel_means[i];
                }
            } else if (key == "kernel_std_devs") {
                std::istringstream iss(value);
                for (int i = 0; i < 4; i++) {
                    iss >> config.kernel_std_devs[i];
                }
            } else if (key == "kernel_weights") {
                std::istringstream iss(value);
                for (int i = 0; i < 4; i++) {
                    iss >> config.kernel_weights[i];
                }
            }
            
            
            else {
                std::cerr << "Error: unknown key " << key << std::endl;
            }
        }
    }

    // Close the file
    file.close();
    return config;
}