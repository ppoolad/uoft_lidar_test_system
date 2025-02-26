/**
 * @file data_collector.cpp
 * @brief Resets TDC and starts recording, stores them into a file
 * @author Pooya Poolad
 *      based on the driver by Jason Gutel
 */

#include <iostream>
#include <fstream>
#include <cmath>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <ctime>
#include <chrono>
#include <thread>
#include <atomic>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "configs.h"
#include "helpers.h"

extern "C" {
  #include <gpiod.h>
  #include "axis-fifo.h"
  #include "asic.h"
  #include "asic_control.h"
  #include "conf.h"
  #include "simple_rx.h"
}

/* Global variables */
static volatile bool running = false;
int runtime_seconds = 100;
int debug_log_enabled = 0;
struct gpiod_chip *gpio_chip;
struct gpiod_chip *led_chip;
struct gpiod_line_bulk gpio_lines;
struct gpiod_line_bulk led_lines;

std::string config_file_path;//("");
std::string output_file_path;//("tdc_values.txt");
std::string forced_output_file_path;//("");

Config config;
std::vector<int> rx_values;
double rolling_avg[6] = {0.0};
int led_values[8] = {0,0,0,0,0,0,0,1};

static void signal_handler(int signal);
void read_from_fifo_thread_fn(std::ofstream& output_fp, int read_fifo_fd);
static void display_help(char *prog_name);
static void quit(void);
void frame_process(std::vector<int> packets);

static int process_options(int argc, char *argv[])
{
    for (;;) {
        int option_index = 0;
        static const char *short_options = "hnco:";
        static const struct option long_options[] = {
            {"help", no_argument, 0, 'h'},
            {"nseconds", required_argument, 0, 'n'},
            {"config", required_argument, 0, 'c'},
            {"output", required_argument, 0, 'o'},
            {0,0,0,0},
        };

        int c = getopt_long(argc, argv, short_options, long_options, &option_index);
        if (c == EOF) {
            break;
        }

        switch (c) {
            case 'h':
                display_help(argv[0]);
                exit(0);
                break;
            case 'n':
                runtime_seconds = atoi(optarg);
                break;
            case 'c':
                config_file_path = std::string(optarg);
                break;
            case 'o':
                forced_output_file_path = (optarg);
                break;
            default:
                display_help(argv[0]);
                exit(1);
        }
    }

    //display_help(argv[0]);
    return 0;
}

static void display_help(char *prog_name)
{
    std::cout << "Usage : " << prog_name << " [OPTIONS]\n"
              << "\n"
              << "  -h, --help     Print this menu\n"
              << "  -n, --nseconds Number of seconds to run\n"
              << "  -c, --config   Config file\n"
              << "  -o, --output   Output file\n";
}

int main(int argc, char** argv)
{
    // Process command-line options
    std::cout << "Processing options" << std::endl;
    process_options(argc, argv);

    std::cout << "reading config file" << std::endl;
    // Validate config file path
    if (config_file_path.empty()) {
        std::cerr << "Config file not set" << std::endl;
        return -1;
    }

    // Parse configuration
    std::cout << "parsing config file" << std::endl;
    config = parse_config(config_file_path);
    output_file_path = forced_output_file_path.empty() ? config.output_file : forced_output_file_path;
    debug_log_enabled = config.debug_log;

    // Validate runtime
    if (config.runtime == 0) {
        std::cerr << "Runtime not set" << std::endl;
        return -1;
    }

    runtime_seconds = runtime_seconds == 0 ? config.runtime / 1000 : runtime_seconds;

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGQUIT, signal_handler);

    // Initialize GPIO chips
    gpio_chip = gpiod_chip_open_by_name(config.io_dev_config.hpc1_chip_name.c_str());
    if (!gpio_chip) {
        perror("Open chip failed");
        exit(EXIT_FAILURE);
    }

    led_chip = gpiod_chip_open_by_name(config.io_dev_config.led_chip_name.c_str());
    if (!led_chip) {
        perror("Open chip failed");
        exit(EXIT_FAILURE);
    }

    // Open FIFO for reading
    int read_fifo_fd = open(config.io_dev_config.rx_dev_fifo.c_str(), O_RDONLY | O_NONBLOCK);
    if (read_fifo_fd < 0) {
        std::cerr << "Open read failed with error: " << std::strerror(errno) << std::endl;
        return -1;
    }

    // Reset FIFO
    if (ioctl(read_fifo_fd, AXIS_FIFO_RESET_IP) < 0) {
        perror("ioctl");
        return -1;
    }

    // Initialize RX
    if (init_rx() < 0) {
        perror("init_rx");
        return -1;
    }

    // Start TDC
    int hpc1_values[40] = {0};
    std::cout << "setting ASIC GPIOs to 0" << std::endl;
    set_gpio_array(gpio_chip, &gpio_lines, hpc1_values);

    tdc_reset(gpio_chip);
    tdc_unreset(gpio_chip);

    int chain_data[4] = {0};
    create_tdc_chain(config.tdc_config.channel_enables, config.tdc_config.channel_offsets, chain_data);
    configure_chain(chain_data, config.tdc_config.tdc_chain_num_words, config.tdc_config.tdc_chain_num_bits, config.tdc_config.tdc_chain_timeout);

    std::cout << "initialize the TDC" << std::endl;
    tdc_test(gpio_chip, &gpio_lines);

    std::cout << "setting leds to 0x01" << std::endl;
    set_gpio_array(led_chip, &led_lines, led_values);

    // Open output file
    std::ofstream output_fp(output_file_path, std::ios::out | std::ios::trunc);
    if (!output_fp) {
        perror("Failed to open output file");
        return -1;
    }

    set_rx_nbits(config.io_dev_config.nbits_rx);
    enable_rx();

    // Start FIFO reading thread
    running = true;
    std::thread read_from_fifo_thread(read_from_fifo_thread_fn, std::ref(output_fp), read_fifo_fd);

    // Main loop
    while (running) {
        sleep(runtime_seconds);
        quit();
    }

    // Cleanup
    // remove rx_
    gpiod_chip_close(gpio_chip);

    int led_values_off[8] = {1,1,1,1,1,1,1,1};
    set_gpio_array(led_chip, &led_lines, led_values_off);
    gpiod_chip_close(led_chip);

    output_fp.close();
    cleanup_rx();
    // remove rx_fifo
    rx_values.clear();
    std::cout << "SHUTTING DOWN, wait for thread" << std::endl;
    read_from_fifo_thread.join();
    close(read_fifo_fd);

    return 0;
}

void read_from_fifo_thread_fn(std::ofstream& output_fp, int read_fifo_fd)
{
    ssize_t bytes_fifo;
    int packets_rx = 0;
    char buf[MAX_BUF_SIZE_BYTES];
    int rx_occupancy = 0;

    usleep(100);
    while (running) {
        while (rx_occupancy < MAX_BUF_SIZE_BYTES && running) {
            ioctl(read_fifo_fd, AXIS_FIFO_GET_RX_OCCUPANCY, &rx_occupancy);
            usleep(10);
        }

        disable_rx();
        
        int found_header = 0;
        while (packets_rx < rx_occupancy) {
            bytes_fifo = read(read_fifo_fd, buf, MAX_BUF_SIZE_BYTES);
            if (bytes_fifo < 0) {
                std::cerr << "read error" << std::endl;
                break;
            }
            if (bytes_fifo > 0) {
                if (debug_log_enabled) {
                    std::cout << "bytes from fifo " << bytes_fifo << std::endl;
                    std::cout << "Reading : " << std::endl;
                }
                for (int mem_idx = 0; mem_idx < bytes_fifo / 4; mem_idx++) {
                    int value;
                    if (debug_log_enabled){
                        std::cout << "0x" << std::hex << (int) buf[3] 
                                        << std::hex << (int) buf[2] 
                                        << std::hex << (int) buf[1] 
                                        << std::hex << (int) buf[0] << std::endl;
                    }
                    std::memcpy(&value, &buf[mem_idx * 4], 4);
                    // if it's not the header or we haven't found the header yet, skip
                    if (value != 0xAA0AAAAA && (!found_header)) {
                        if (debug_log_enabled) {
                            std::cout << "skipping 0x" << std::hex << value << std::endl;
                        }
                        continue;
                    }
                    // if we found the header, we can start reading the data
                    found_header = 1;
                    if (debug_log_enabled) {
                        std::cout << "0x" << std::hex << value << std::endl;
                    }
                    rx_values.push_back(value);
                    output_fp << std::hex << value << std::endl;
                    packets_rx += 4;
                }
            }
        }

        if (debug_log_enabled) {
            std::cout << (int)(packets_rx - 4) << " packets read" << std::endl;
        }

        frame_process(rx_values);
        packets_rx = 0;
        rx_occupancy = 0;
        //found_header = 0;
        enable_rx(); // enable it again
    }
}

int queue_processed = 1;
const double ALPHA_MIN = 0.000000001;

void frame_process(std::vector<int> packets)
{
    int num_packets = packets.size();
    double alpha = 1.0f / (double)(10 * queue_processed);
    std::cout << "alpha: " << alpha << std::endl;
    if (alpha < ALPHA_MIN) {
        alpha = ALPHA_MIN;
    } else {
	    queue_processed++;    
    }

    while (!packets.empty()) {
        int packet = packets.back();
        packets.pop_back();
        if (debug_log_enabled) std::cout << "Packet: 0x" << std::hex << packet << std::endl;
        if (packet != 0xAAFFFFFF) {
            continue;
        }
        if (packets.size() < 7) {
            continue;
        }
        if (debug_log_enabled) std::cout << "HEADER Packet: 0x" << std::hex << packets[packets.size() - 7] << std::endl;
        if (packets[packets.size() - 7] != 0xAA0AAAAA) {
            continue;
        }

        for (int i = 0; i < 6; i++) {
            int tof = packets.back();
            if (debug_log_enabled) std::cout << "tof" << i << ": " << tof << std::endl;
            packets.pop_back();
            rolling_avg[5 - i] = (1.0 - alpha) * rolling_avg[5 - i] + alpha * (double)(tof & 0x00FFFFFF);
            // print rolling average 
            //std::cout << "rolling_avg" << i << ": " << rolling_avg[5 - i] << std::endl;
        }
        
        std::cout << "\r";
        for (int i = 0; i < 6; i++) {
            std::cout << "avg[" << i << "] = " << rolling_avg[i] << " ";
        }
        std::cout << std::flush;
        
	    //pop the next thing that should be header
        packets.pop_back();
    }
}

static void signal_handler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM || signal == SIGQUIT) {
        running = false;
    }
}

static void quit(void)
{
    running = false;
}

