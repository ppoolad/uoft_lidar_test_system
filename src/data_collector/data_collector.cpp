/**
 * @file data_collector.cpp
 * @author Pooya Poolad
 *      based on the driver by Jason Gutel
 * Resets TDC and starts recording, stores them into a file
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

/* global variables */
static volatile bool running = false;
int runtime = 0;
int debug_log = 0;
struct gpiod_chip *chip;
struct gpiod_chip *chipled;
struct gpiod_line_bulk gpios;
struct gpiod_line_bulk leds;

std::string config_file = "";
std::string output_file = "tdc_values.txt";
std::string output_file_forced = "";

Config config;
std::vector<int> rx_values;
float rolling_avg[6] = {0.0};
int led_values[8] = {0,0,0,0,0,0,0,1};

static void signal_handler(int signal);
void read_from_fifo_thread_fn(std::ofstream& output_fp, int readFifoFd);
static void display_help(char * progName);
static void quit(void);
void frame_process(std::vector<int> packets);

static int process_options(int argc, char * argv[])
{
    for (;;) {
        int option_index = 0;
        static const char *short_options = "hnco:t:";
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
                runtime = atoi(optarg);
                break;
            case 'c':
                config_file = std::string(optarg);
                break;
            case 'o':
                output_file_forced = std::string(optarg);
                break;
            default:
                display_help(argv[0]);
                exit(1);
        }
    }

    display_help(argv[0]);
    return 0;
}

static void display_help(char * progName)
{
    std::cout << "Usage : " << progName << " [OPTIONS]\n"
              << "\n"
              << "  -h, --help     Print this menu\n"
              << "  -n, --nseconds Number of seconds to run\n"
              << "  -c, --config   Config file\n"
              << "  -o, --output   Output file\n";
}

int main(int argc, char** argv)
{
    process_options(argc, argv);
    if (config_file.empty()) {
        std::cerr << "Config file not set" << std::endl;
        return -1;
    }
    config = parse_config(config_file);
    output_file = output_file_forced.empty() ? config.output_file : output_file_forced;
    debug_log = config.debug_log;

    if (config.runtime == 0) {
        std::cerr << "Runtime not set" << std::endl;
        return -1;
    }

    runtime = runtime == 0 ? config.runtime / 1000 : runtime;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGQUIT, signal_handler);

    chip = gpiod_chip_open_by_name(config.io_dev_config.hpc1_chip_name.c_str());
    if (!chip) {
        perror("Open chip failed");
        exit(EXIT_FAILURE);
    }

    chipled = gpiod_chip_open_by_name(config.io_dev_config.led_chip_name.c_str());
    if (!chipled) {
        perror("Open chip failed");
        exit(EXIT_FAILURE);
    }

    int readFifoFd = open(config.io_dev_config.rx_dev_fifo.c_str(), O_RDONLY | O_NONBLOCK);
    if (readFifoFd < 0) {
        std::cerr << "Open read failed with error: " << std::strerror(errno) << std::endl;
        return -1;
    }

    if (ioctl(readFifoFd, AXIS_FIFO_RESET_IP) < 0) {
        perror("ioctl");
        return -1;
    }

    if (init_rx() < 0) {
        perror("init_rx");
        return -1;
    }

// Start TDC
    int hpc1_values[40] = {0};
    std::cout << "setting ASIC GPIOs to 0" << std::endl;
    set_gpio_array(chip, &gpios, hpc1_values);

    tdc_reset(chip);
    tdc_unreset(chip);

    int chain_data[4] = {0};
    create_tdc_chain(config.tdc_config.channel_enables, config.tdc_config.channel_offsets, chain_data);
    configure_chain(chain_data, config.tdc_config.tdc_chain_num_words, config.tdc_config.tdc_chain_num_bits, config.tdc_config.tdc_chain_timeout);

    std::cout << "initialize the TDC" << std::endl;
    tdc_test(chip, &gpios);

    std::cout << "setting leds to 0x01" << std::endl;
    set_gpio_array(chipled, &leds, led_values);

    std::ofstream output_fp(output_file, std::ios::out | std::ios::trunc);
    if (!output_fp) {
        perror("Failed to open output file");
        return -1;
    }

    set_rx_nbits(config.io_dev_config.nbits_rx);
    enable_rx();

    std::thread read_from_fifo_thread(read_from_fifo_thread_fn, std::ref(output_fp), readFifoFd);

// perform noops
    while (running) {
        sleep(runtime);
        quit();
    }

    gpiod_chip_close(chip);

    int led_values_off[8] = {1,1,1,1,1,1,1,1};
    set_gpio_array(chipled, &leds, led_values_off);
    gpiod_chip_close(chipled);

    output_fp.close();
    cleanup_rx();
    std::cout << "SHUTTING DOWN, wait for thread" << std::endl;
    read_from_fifo_thread.join();
    close(readFifoFd);

    return 0;
}

void read_from_fifo_thread_fn(std::ofstream& output_fp, int readFifoFd)
{
    ssize_t bytesFifo;
    int packets_rx = 0;
    char buf[MAX_BUF_SIZE_BYTES];
    int rx_occupancy = 0;

    usleep(100);
    while (running) {
        while (rx_occupancy < MAX_BUF_SIZE_BYTES && running) {
            ioctl(readFifoFd, AXIS_FIFO_GET_RX_OCCUPANCY, &rx_occupancy);
            usleep(10);
        }

        disable_rx();

        while (packets_rx < rx_occupancy) {
            bytesFifo = read(readFifoFd, buf, MAX_BUF_SIZE_BYTES);
            if (bytesFifo < 0) {
                std::cerr << "read error" << std::endl;
                break;
            }
            if (bytesFifo > 0) {
                if (debug_log) {
                    std::cout << "bytes from fifo " << bytesFifo << std::endl;
                    std::cout << "Read : " << std::endl;
                }
                for (int memidx = 0; memidx < bytesFifo / 4; memidx++) {
                    int value;
                    std::memcpy(&value, &buf[memidx * 4], 4);
                    if (value != 0xAA0AAAAA) {
                        if (debug_log) {
                            std::cout << "skipping 0x" << std::hex << value << std::endl;
                        }
                        continue;
                    }
                    if (debug_log) {
                        std::cout << "0x" << std::hex << value << std::endl;
                    }
                    rx_values.push_back(value);
                    output_fp << value << std::endl;
                    packets_rx += 4;
                }
            }
        }

        if (debug_log) {
            std::cout << packets_rx - 4 << " packets read" << std::endl;
        }

        frame_process(rx_values);
        packets_rx = 0;
        rx_occupancy = 0;
    }
}

int queue_processed = 0;
const float alpha_min = 0.000000001;

void frame_process(std::vector<int> packets)
{
    int num_packets = packets.size();
    float alpha = 1.0f / (10 * queue_processed);
    queue_processed++;
    if (alpha < alpha_min) {
        alpha = alpha_min;
    }

    while (!packets.empty()) {
        int packet = packets.back();
        packets.pop_back();
        std::cout << "Packet: 0x" << std::hex << packet << std::endl;
// we are reading in reverse order
        if (packet != 0xAAFFFFFF) {
            continue;
        }
        if (packets.size() < 7) {
            continue;
        }
        std::cout << "HEADER Packet: 0x" << std::hex << packets[packets.size() - 6] << std::endl;
        if (packets[packets.size() - 6] != 0xAA0AAAAA) {
            continue;
        }

        for (int i = 0; i < 6; i++) {
            int tof = packets.back();
            packets.pop_back();
            rolling_avg[5 - i] = (1 - alpha) * rolling_avg[5 - i] + alpha * (tof & 0x00FFFFFF);
        }

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

