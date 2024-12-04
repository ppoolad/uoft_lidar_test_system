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
#include <sstream>
#include <gpiod.h>

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

//led values indicating running
int led_values[8] = {0,0,0,0,0,0,0,1};

Config parse_config(std::string filename);
static void signal_handler(int signal);
static void *read_from_fifo_thread_fn(void *data);
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

    // open files to save output
    FILE* fp = fopen(output_file.c_str(), "w");
    if(!fp) {
        perror("Failed to open output file");
        return -1;
    }

    //record random input values
    FILE* fprnd = fopen("random_values.txt", "w");
    
    // start a thread than listens to rx fifo
    pthread_t read_from_fifo_thread;
    pthread_create(&read_from_fifo_thread, NULL, read_from_fifo_thread_fn, NULL);

    // Enable the RX core
    set_rx_nbits(config.nbits_rx); // 16 data + 8 header
    enable_rx();
    dsp_serializer(1, chip);

    // start the clock
    clock_t start = clock();
    int chain_data[4] = {0, 0, 0, 0};
    int runtime = config.runtime;
    while (running && (int)(clock() - start)/CLOCKS_PER_SEC < runtime) {
        // send random number to DSP
        int random_number;
        // generate a uniform random between 0 and 1
        double u = (double)rand() / RAND_MAX;
        if (u < 0.7) {
            // generate a random number between 0 and 2^16 with mean of 0 and std of 1
            random_number = generate_random_number(2500, 100, 1);
        } else {
            // generate a random number between 0 and 2^16 with mean of 0 and std of 1
            random_number = generate_random_number(8500, 100, 1);
        }
        fprintf(fprnd, "%d\n", random_number);
        printf("random number %d\n", 0xFFff & random_number);
        chain_data[3] = 0xFFFF & (random_number);
        configure_chain_dsp(chain_data, 4, 16, 1000);
    }

    // Quit
    quit();

    std::cout << "turn off serializer" << std::endl;
    dsp_serializer(0, chip);

    gpio_chip_close(chip);
    gpio_chip_close(chipled);

    // close files
    fclose(fp);
    fclose(fprnd);

    //cleanup rx device
    cleanup_rx();

    //close threads
    std::cout << "SHUTTING DOWN, wait for thread" << std::endl;
    pthread_join(read_from_fifo_thread, NULL);
    close(readFifoFd);
    
    
    return 0;

}

// reads config.text and sets the global configuration struct
Config parse_config(std::string filename) {
    
    Config config;

    // Open the file
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: could not open file " << filename << std::endl;
        return;
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
            } else {
                std::cerr << "Error: unknown key " << key << std::endl;
            }
        }
    }

    // Close the file
    file.close();
    return config;
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

static void *read_from_fifo_thread_fn(void *data)
{
    ssize_t bytesFifo;
    int packets_rx;
    uint8_t buf[MAX_BUF_SIZE_BYTES];
    int rx_occupancy = 0;
    /* shut up compiler */
    (void)data;

    packets_rx = 0;
    sleep(1);
    while (running) {
        //wait for fifo to fill up
        while(rx_occupancy < MAX_BUF_SIZE_BYTES && running){
            ioctl(readFifoFd, AXIS_FIFO_GET_RX_OCCUPANCY, &rx_occupancy);
            if(DEBUG) DEBUG_PRINT("rx_occupancy %d\n",rx_occupancy);
            usleep(100);
        }

        // stop rx
        disable_rx();

}