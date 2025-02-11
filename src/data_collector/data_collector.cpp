/**
 * @file data_collector.cpp
 * @author Pooya Poolad
 *      based on the driver by Jason Gutel
 * Resets TDC and starts recording, stores them into a file
 */

// todo: clean up commands somehow

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
#include <fcntl.h>              // open flags
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
// External C headers
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
int runtime = 0; //run for n secs
int debug_log = 0;
//gpio stuff
struct gpiod_chip *chip;// = gpiod_chip_open_by_name(HPC1_CHIP_NAME);
struct gpiod_chip *chipled;// = gpiod_chip_open_by_name(LED_CHIP_NAME);
struct gpiod_line_bulk gpios;
struct gpiod_line_bulk leds;

std::string config_file = "";
std::string output_file = "tdc_values.txt";
std::string output_file_forced = "";

Config config;

//global output vector
std::vector<int> rx_values;

//led values indicating running
int led_values[8] = {0,0,0,0,0,0,0,1};
static void signal_handler(int signal);
void read_from_fifo_thread_fn(std::ofstream& output_fp, int readFifoFd);
static void display_help(char * progName);
static void quit(void);
void frame_process(char* packets, int size);
//static void print_opts();

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

            int c = getopt_long(argc, argv, short_options,
            long_options, &option_index);

            if (c == EOF) {
            break;
            }

            switch (c) {

                default:
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
            }
        }

        display_help();
        return 0;
}

static void display_help(char * progName)
{
    std::cout << "Usage : " << progName << " [OPTIONS]\n"
              << "\n"
              << "  -h, --help     Print this menu\n"
              << "  -n, --nseconds Number of seconds to run\n"
              << "  -c, --config   Config file\n"
              << "  -o, --output   Output file\n"
}

// static void print_opts()
// {
//     std::cout << "Options : \n"
//               << "TBD: " << "\n";
// }

int main(int argc, char** argv)
{
    //process_options(argc, argv);
    if (config_file == "") {
        std::cerr << "Config file not set" << std::endl;
        return -1;
    }
    config = parse_config(config_file);
    if (output_file_forced != "") {
        output_file = output_file_forced;
    } else {
        output_file = config.output_file;
    }



    debug_log = config.debug_log;

    if (config.runtime == 0) {
        std::cerr << "Runtime not set" << std::endl;
        return -1;
    }

    if(runtime == 0) {
        runtime = config.runtime/1000;
    }

    // Listen to ctrl+c and assert
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGQUIT, signal_handler);


    // open hpc1 chip
    chip = gpiod_chip_open_by_name(config.io_dev_config.hpc1_chip_name.c_str());
    if (!chip) {
        perror("Open chip failed");
        exit(EXIT_FAILURE);
    }   

    // open led chip
    chipled = gpiod_chip_open_by_name(config.io_dev_config.led_chip_name.c_str());
    if (!chipled) {
        perror("Open chip failed");
        exit(EXIT_FAILURE);
    }

    // open fifo
    int readFifoFd = open(config.io_dev_config.rx_dev_fifo.c_str(), O_RDONLY | O_NONBLOCK);
    if (readFifoFd < 0) {
        std::cout << "Open read failed with error: " << std::strerror(errno) << std::endl;
        return -1;
    }

    int rx_core;
    rx_core = ioctl(readFifoFd, AXIS_FIFO_RESET_IP);
    if (rx_core) {
        perror("ioctl");
        return -1;
    }

    rx_core = init_rx();
    if (rx_core < 0) {
        perror("init_rx");
        return -1;
    }

    // Start TDC
    int hpc1_values[40] = {0};
    std::cout << "setting ASIC GPIOs to 0" << std::endl;
    set_gpio_array(chip, &gpios, hpc1_values);

    //reset/unreset dsp
    tdc_reset(chip);
    tdc_unreset(chip);

    // configure tdc channels
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

    //star threads
    std::thread read_from_fifo_thread(read_from_fifo_thread_fn, std::ref(output_fp), readFifoFd);

    // perform noops
    while (running) {
        sleep(runtime); //run for n minutes
        quit();
    }

    // turn off serializer
    //tdc_serializer(0, chip);
    //tdc_reset(chip);

    gpiod_chip_close(chip); 

    //set leds to all 1
    int led_values[8] = {1,1,1,1,1,1,1,1};
    set_gpio_array(chipled, &leds, led_values);
    gpiod_chip_close(chipled);

    // close file
    output_fp.close();
    cleanup_rx();
    std::cout << "SHUTTING DOWN, wait for thread" << std::endl;
    read_from_fifo_thread.join();
    close(readFifoFd);

    return 0;

}

// read thread
void read_from_fifo_thread_fn(std::ofstream& output_fp, int readFifoFd)
{
    ssize_t bytesFifo;
    int packets_rx;
    char buf[MAX_BUF_SIZE_BYTES];
    int rx_occupancy = 0;

    packets_rx = 0;
    usleep(100); //let the fifo fill up
    while (running){
        while (rx_occupancy < MAX_BUF_SIZE_BYTES && running) {
            ioctl(readFifoFd, AXIS_FIFO_GET_RX_OCCUPANCY, &rx_occupancy);
            usleep(10);
        }

        // hold the fifo
        disable_rx();

        //read all the packets
        while(packets_rx < rx_occupancy) {
            bytesFifo = read(readFifoFd, buf, MAX_BUF_SIZE_BYTES);
            if (bytesFifo < 0) {
                std::cout << "read error" << std::endl;
                break;
            }
            if (bytesFifo > 0) {
                if (debug_log) {
                    std::cout << "bytes from fifo " << bytesFifo << std::endl;
                    std::cout << "Read : " << std::endl;
                }
                for (int memidx = 0; memidx < bytesFifo/4; memidx++) {
                    int value;
                    std::memcpy(&value, &buf[memidx*4], 4);
                    // skip if the value is not 0xAA0AAAAA
                    if (value != 0xAA0AAAAA) {
                        if (debug_log) {
                            std::cout << "skepping 0x" << std::hex << value << std::endl;
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
            std::cout << packets_rx << " packets read" << std::endl;
        }

    }
}

static void signal_handler(int signal)
{
    switch (signal) {
        case SIGINT:
        case SIGTERM:
        case SIGQUIT:
            running = false;
            break;

        default:
            break;
    }
}

static void quit(void)
{
    running = false;
}

