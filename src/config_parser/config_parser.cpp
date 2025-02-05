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
                config.io_dev_config.hpc1_chip_name = value;
            } else if (key == "num_hpc1") {
                config.io_dev_config.num_hpc1 = std::stoi(value);
            } else if (key == "hpc0_chip_name") {
                config.io_dev_config.hpc0_chip_name = value;
            } else if (key == "num_hpc0") {
                config.io_dev_config.num_hpc0 = std::stoi(value);
            } else if (key == "led_chip_name") {
                config.io_dev_config.led_chip_name = value;
            } else if (key == "num_led") {
                config.io_dev_config.num_led = std::stoi(value);
            } else if (key == "pb_chip_name") {
                config.io_dev_config.pb_chip_name = value;
            } else if (key == "num_pb") {
                config.io_dev_config.num_pb = std::stoi(value);
            } else if (key == "output_file") {
                config.output_file = value;
            } else if (key == "rx_dev_fifo") {
                config.io_dev_config.rx_dev_fifo = value;
            } else if (key == "nbits_rx") {
                config.io_dev_config.nbits_rx = std::stoi(value);
            } else if (key == "uio_device") {
                config.io_dev_config.uio_device = value;
            } else if (key == "runtime"){
                config.runtime = std::stoi(value);
            } else if (key == "snr"){
                config.dsp_config.snr = std::stof(value);
            } else if (key == "kernel_means") {
                std::istringstream iss(value);
                for (int i = 0; i < 4; i++) {
                    iss >> config.dsp_config.kernel_means[i];
                }
            } else if (key == "kernel_std_devs") {
                std::istringstream iss(value);
                for (int i = 0; i < 4; i++) {
                    iss >> config.dsp_config.kernel_std_devs[i];
                }
            } else if (key == "kernel_weights") {
                std::istringstream iss(value);
                for (int i = 0; i < 4; i++) {
                    iss >> config.dsp_config.kernel_weights[i];
                }
            }   else if (key == "dsp_debug_log") {
                config.dsp_config.debug_log = std::stoi(value);
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