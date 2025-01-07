/**
 * @file dsp.cpp
 * @author Pooya Poolad pooya.poolad@isl.utoronto.ca 
 * resets DSP, injects data with known distribution and starts recording, stores them into a file
 * 
 **/

#include <iostream>
#include <fstream>
#include <cmath>
#include <string>
#include <format>
#include <sstream>
#include <gpiod.h>
#include <chrono>
#include <vector>
#include <thread>

#include "dsp.hpp"
#include "configs.h"

//globlas
//gpio stuff
static volatile bool running = true;
struct gpiod_chip *chip;// = gpiod_chip_open_by_name(HPC1_CHIP_NAME);
struct gpiod_chip *chipled;// = gpiod_chip_open_by_name(LED_CHIP_NAME);
struct gpiod_line_bulk gpios;
struct gpiod_line_bulk leds;

std::string config_file = "config.txt";
std::string output_file = "dsp_values.txt";

std::ofstream output_file;

//global output vector
std::vector<int> rx_values;
//led values indicating running
int led_values[8] = {0,0,0,0,0,0,0,1};

Config parse_config(std::string filename);
static void signal_handler(int signal);
static void read_from_fifo_thread_fn(std::ofstream& fp);
static void display_help(char * progName);
static void quit(void);
void frame_process(char* packets, int size);

void process_config(int argc, char** argv) {
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-c") {
            config_file = argv[i + 1];
        } else if (std::string(argv[i]) == "-o") {
            output_file = argv[i + 1];
        } else if (std::string(argv[i]) == "-h") {
            display_help(argv[0]);
            exit(0);
        }
    }
}

void shift_array(int *array, int size, int insert){
    //int temp = array[size-1];
    for (int i = size-1; i > 0; i--)
    {
        array[i] = array[i-1];
    }
    //invert of last bit should rotate to 0
    array[0] = insert;
    //array[0] = !array[0];
}

//generate a random integer with mean and std
int generate_random_number(double mean, double std_dev) {

    // generate a random number between 0 and 1
    double u1 = (double)rand() / RAND_MAX;
    double u2 = (double)rand() / RAND_MAX;

    // Box-Muller transform
    double z = sqrt(-2 * log(u1)) * cos(2 * M_PI * u2);
    return (int) (mean + std_dev * z);
}

int main(int argc, char** argv) {

    process_config(argc, argv);

    // Read configuration file
    Config config = parse_config(config_file);

    // open hpc1 chip
    chip = gpiod_chip_open_by_name(config.hpc1_chip_name.c_str());
    if (!chip) {
        perror("Open HPC1 chip failed");
        exit(EXIT_FAILURE);
    }

    // open led chip
    chipled = gpiod_chip_open_by_name(config.led_chip_name.c_str());
    if (!chipled) {
        perror("Open LED chip failed");
        exit(EXIT_FAILURE);
    }

    // Open fifo
    int readFifoFd = open(rx_dev_fifo, O_RDONLY | O_NONBLOCK);
    if (readFifoFd < 0) {
        printf("Open read failed with error: %s\n", strerror(errno));
        return -1;
    }

    // Initialize the RX core
    int rc;
    rc = ioctl(readFifoFd, AXIS_FIFO_RESET_IP);
    if (rc) {
        perror("ioctl");
        return -1;
    }
    rc = init_rx();
    if (rc<0) {
        perror("init_rx");
        return -1;
    }

    // Start DSP
    int hpc1_values[40] = {0};
    std::cout << "setting ASIC GPIOs to 0" << std::endl;
    set_gpio_array(chip, &gpios, hpc1_values);

    //reset/unreset dsp
    dsp_test(chip, &gpios);
    dsp_reset(chip);
    dsp_unreset(chip);

    std::cout << "setting leds to 0x55" << std::endl;
    set_gpio_array(chipled, &leds, led_values);

    std::ofstream fp(output_file, std::ios::out | std::ios::trunc);
    if(!fp) {
        perror("Failed to open output file");
        return -1;
    }

    //record random input values
    //FILE* fprnd = fopen("random_values.txt", "w");
    std::ofstream fprnd("random_values.txt", std::ios::out | std::ios::trunc);
    
    // start a thread than listens to rx fifo
    //pthread_t read_from_fifo_thread;
    std::thread read_from_fifo_thread(read_from_fifo_thread_fn, fp);

    //pthread_create(&read_from_fifo_thread, NULL, read_from_fifo_thread_fn, NULL);

    // Enable the RX core
    set_rx_nbits(config.nbits_rx); // 16 data + 8 header
    enable_rx();
    dsp_serializer(1, chip);

    // start the clock
    auto start = std::chrono::high_resolution_clock::now();
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
    int chain_data[4] = {0, 0, 0, 0};
    int runtime = config.runtime;
    while (running && duration < runtime) {
        // send random number to DSP
        int random_number;
        // generate a uniform random between 0 and 1
        double u = (double)rand() / RAND_MAX;
 
        if(u < config.snr) {
            // which kernel it is
            int kernel_share[NUM_KERNELS];
            //kernel_share[0] = (1 - config.snr)*config.kernel_weights[0]; do we care?
            kernel_share[1] = (1 - config.snr)*config.kernel_weights[1];
            kernel_share[2] = (1 - config.snr)*config.kernel_weights[2];
            kernel_share[3] = (1 - config.snr)*config.kernel_weights[3];
            if(u > kernel_share[3] + kernel_share[2] + kernel_share[1]) { //kernel 0
                random_number = generate_random_number(config.kernel_means[0], config.kernel_std_devs[0]);
            } else if(u > kernel_share[3] + kernel_share[2]) { //kernel 1
                random_number = generate_random_number(config.kernel_means[1], config.kernel_std_devs[1]);
            } else if(u > kernel_share[3]) { //kernel 2
                random_number = generate_random_number(config.kernel_means[2], config.kernel_std_devs[2]);
            } else { //kernel 3
                random_number = generate_random_number(config.kernel_means[3], config.kernel_std_devs[3]);
            }
        } else {
            random_number = rand(); // its a uniform noise
        }
        
        //record random number
        fprnd << random_number << std::endl;
        std::cout << "random number " << 0xFFFF & random_number << std::endl;

        //prepare for device
        chain_data[3] = 0xFFFF & (random_number);
        configure_chain_dsp(chain_data, 4, 16, 1000);

        // update time
        stop = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
    }

    // Quit
    quit();

    std::cout << "turn off serializer" << std::endl;
    dsp_serializer(0, chip);

    gpio_chip_close(chip);
    gpio_chip_close(chipled);

    // close files
    fp.close();
    fprnd.close();

    //cleanup rx device
    cleanup_rx();

    //close threads
    std::cout << "SHUTTING DOWN, wait for thread" << std::endl;
    //pthread_join(read_from_fifo_thread, NULL);
    read_from_fifo_thread.join();

    close(readFifoFd);
    return 0;

}


static void display_help(char * progName)
{
    std::cout << "Usage: " << progName << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -c, --config    Configuration file" << std::endl;
    std::cout << "  -o, --output    Output file" << std::endl;

}

static void quit(void)
{
    running = false;
}

static void read_from_fifo_thread_fn(std::ofstream& fp)
{
    ssize_t bytesFifo;
    int packets_rx;
    uint8_t buf[MAX_BUF_SIZE_BYTES];
    int rx_occupancy = 0;


    packets_rx = 0;
    sleep(1);
    while (running) {
        //wait for fifo to fill up
        while(rx_occupancy < MAX_BUF_SIZE_BYTES && running){
            ioctl(readFifoFd, AXIS_FIFO_GET_RX_OCCUPANCY, &rx_occupancy);
            if(DEBUG) std::cout << "rx_occupancy: " << rx_occupancy << std::p(100);
        }

        // fifo is full stop RX
        disable_rx();

        // empty fifo
        while (packets_rx < rx_occupancy)
        {
            bytesFifo = read(readFifoFd, buf, MAX_BUF_SIZE_BYTES);
            if(bytesFifo < 0){
                std::cerr << "read from fifo error" << std::endl;
                break;
            }
            if (bytesFifo > 0) {
                if(DEBUG){
                    std::clog << "bytes from fifo: " << bytesFifo << std::endl;
                    std::clog << "Read : "<< std::endl;
                }
                for (int memidx = 0; memidx < bytesFifo/4; memidx++)
                {
                    // for(int nbytes = 0; nbytes < 4; nbytes++){
                    //     if(DEBUG){
                    //         if(nbytes == 0)
                    //             std::cout << std::format("{:02x}", 0);
                    //         else
                    //             std::cout << std::format("{:02x}", buf[memidx*4+3-nbytes]);
                    //     }
                    // //dsp data packet is 24 bits
                    // if (nbytes > 0)
                    //     //rx_values[packets_rx] = buf[memidx*4+3-nbytes];
                    //     int value;
                    //     packets_rx++;
                    // }
                    // //read and print 4 bytes in little endian
                    // if(DEBUG)
                    //     std::cout << std::endl;
                    int value;
                    value = std::memcpy(&buf[memidx*4], sizeof(int));
                    // print as hex to console
                    std::cout << "0x" << std::format("{:08x}", value) << std::endl;
                    rx_values.push_back(value&0x00FFFFFF); // first byte is header
                    // write to file
                    fp << value << std::endl;

                    packets_rx = packets_rx + 4;
                }       
            }
        }
        if(DEBUG) std::cout << packets_rx-4 << " packets read" << std::endl;
        
        
        //write to file
        

}