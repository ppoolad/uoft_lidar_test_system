/**
 * @file tdc_test.c
 * @author Pooya Poolad ppoolad@outlook.com
 *      based on the driver by Jason Gutel jason.gutel@gmail.com
 * resets DSP and starts recording, stores them into a file
 * 
 **/

//todo: clean up commands somehow

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include <fcntl.h>              // Flags for open()
#include <sys/stat.h>           // Open() system call
#include <sys/types.h>          // Types for open()
#include <unistd.h>             // Close() system call
#include <string.h>             // Memory setting and copying
#include <getopt.h>             // Option parsing
#include <errno.h>              // Error codes
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <gpiod.h>

#include "axis-fifo.h"
#include "asic.h"
#include "asic_control.h"
#include "conf.h"
#include "simple_rx.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_VALUE 65535
/*----------------------------------------------------------------------------

 * Internal Definitions
 *----------------------------------------------------------------------------*/
//#define DEF_DEV_TX "/dev/axis_fifo_0x00000000a0070000"
#define DEF_DEV_RX "/dev/axis_fifo_0x00000000a0030000"
#define MAX_BUF_SIZE_BYTES 4096

#define DEBUG_PRINT(fmt, args...) printf("DEBUG %s:%d(): " fmt, \
        __func__, __LINE__, ##args)

#ifndef DEBUG 
    #define DEBUG 1
#endif
struct thread_data {
    int rc;
};

pthread_t read_from_fifo_thread;

static volatile bool running = true;
static char _opt_dev_rx[255];
static int readFifoFd;
static char output_file[255] = "dsp_values.txt";

static void signal_handler(int signal);
static void *read_from_fifo_thread_fn(void *data);
static int process_options(int argc, char * argv[]);
static void print_opts();
static void display_help(char * progName);
static void quit(void);
void frame_process(char* packets, int size);

//globlas
//gpio stuff
struct gpiod_chip *chip;// = gpiod_chip_open_by_name(HPC1_CHIP_NAME);
struct gpiod_chip *chipled;// = gpiod_chip_open_by_name(LED_CHIP_NAME);
struct gpiod_line_bulk gpios;
struct gpiod_line_bulk leds;
float rolling_avg[6] = {0};
float alpha_max = 0.001;
int queue_processed = 0;
//large array to store the values
char rx_values[MAX_BUF_SIZE_BYTES+16] = {0};
//file handle to save the values
FILE *fp;
FILE *fprnd;
//led values indicating running
int led_values[8] = {0,0,0,0,0,0,0,1};
//shift 8 bit array;

int runtime = 20; //run for n secs
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

//generate random number between 0 to 2^16 with mean of n and std of s
static int rand_initiated = 0;
int generate_random_number(double mean, double std_dev, int seed) {
    if (!rand_initiated) {
        srand(seed);
        rand_initiated = 1;
    }

    // Box-Muller transform
    double u1 = (double)rand() / RAND_MAX;
    double u2 = (double)rand() / RAND_MAX;
    double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    double random_number = mean + z0 * std_dev;

    // Clamp the value between 0 and MAX_VALUE
    if (random_number < 0) {
        random_number = 0;
    } else if (random_number > MAX_VALUE) {
        random_number = MAX_VALUE;
    }

    return (int) random_number;
}

/*----------------------------------------------------------------------------
 * Main
 *----------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
    process_options(argc, argv);

    int rc;

    // Listen to ctrl+c and assert
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGQUIT, signal_handler);


    chip = gpiod_chip_open_by_name(HPC1_CHIP_NAME);
    if (!chip) {
        perror("Open chip failed");
        exit(EXIT_FAILURE);
    }
    chipled = gpiod_chip_open_by_name(LED_CHIP_NAME);
    if (!chipled) {
        perror("Open chip failed");
        exit(EXIT_FAILURE);
    }
    /*************/
    /* open FIFO */
    /*************/
    readFifoFd = open(_opt_dev_rx, O_RDONLY | O_NONBLOCK);
    if (readFifoFd < 0) {
        printf("Open read failed with error: %s\n", strerror(errno));
        return -1;
    }

    /*****************************/
    /* initialize the fifo core  */
    /*****************************/
    rc = ioctl(readFifoFd, AXIS_FIFO_RESET_IP);
    if (rc) {
        perror("ioctl");
        return -1;
    }

    // Initialize the RX core
    rc = init_rx();
    if (rc<0) {
        perror("init_rx");
        return -1;
    }

    /*****************/
    /*   start dsp   */
    /*****************/

    int hpc1_values[40] = {0};
    printf("setting gpios to 0\n");
    set_gpio_array(chip, &gpios, hpc1_values);

    //reset/unreset dsp
    dsp_test(chip, &gpios);
    dsp_reset(chip);
    dsp_unreset(chip);

    printf("setting leds to 0x55\n");
    
    set_gpio_array(chipled, &leds, led_values);
    
    /*****************/
    /* start threads */
    /*****************/

    //open file to save values
    fp = fopen(output_file,"w");
    if (fp == NULL) {
        perror("Failed to open file");
        return -1;
    }
    fprnd = fopen("random_values.txt","w");
    if (fprnd == NULL) {
        perror("Failed to open file");
        return -1;
    }
    /* start thread listening for fifo receive packets */
    rc = pthread_create(&read_from_fifo_thread, NULL, read_from_fifo_thread_fn,
            (void *)NULL);

    // Enable the RX core
    //set number of serial bits
    set_rx_nbits(24); //16 data + 8 header
    //enable rx 
    enable_rx();
    dsp_serializer(1,chip);
    /* perform noops */
    // start the clock
    clock_t start = clock();
    int chain_data[4] = {0,0,0,0};
    while (running && (int)(clock() - start)/CLOCKS_PER_SEC < runtime) {
        //send random number to DSP
        int random_number;
        // generate a uniform random between 0 and 1
        double u = (double)rand() / RAND_MAX;
        if (u < 0.5) {
            // generate a random number between 0 and 2^16 with mean of 0 and std of 1
            if (u < 0.20)
                random_number = generate_random_number(8500, 100, 1);
            else
                random_number = generate_random_number(2500, 100, 1);
        } else {
            // generate a random number between 0 and 2^16 with mean of 0 and std of 1
            random_number = (int)(u*MAX_VALUE);
        }
        fprintf(fprnd,"%d\n",random_number);
        printf("random number %d\n",0xFFFF&random_number);
        chain_data[3] = 0xFFFF&(random_number);
        configure_chain_dsp(chain_data, 4, 16, 1000);
        // just give it a random number and wait for it to converge
    }

    quit();

    printf("turn off serializer\n");
    dsp_serializer(0,chip);

    printf("reset DSP\n");
    dsp_reset(chip);
    
    gpiod_chip_close(chip);
    gpiod_chip_close(chipled); //run leds to know it's done

    //close file
    fclose(fp);
    fclose(fprnd);
    cleanup_rx();
    printf("SHUTTING DOWN, wait for thread\n");
    pthread_join(read_from_fifo_thread, NULL);
    close(readFifoFd);
    return rc;
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
    /* shup up compiler */
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


        // empty fifo
        while (packets_rx < rx_occupancy)
        {
            bytesFifo = read(readFifoFd, buf, MAX_BUF_SIZE_BYTES);
            if(bytesFifo < 0){
                printf("read error\n");
                break;
            }
            if (bytesFifo > 0) {
                if(DEBUG){
                    DEBUG_PRINT("bytes from fifo %d\n",bytesFifo);
                    DEBUG_PRINT("Read : \n\r");
                }
                for (int memidx = 0; memidx < bytesFifo/4; memidx++)
                {
                    for(int nbytes = 0; nbytes < 4; nbytes++){
                        if(DEBUG){
                            if(nbytes == 0)
                                printf("0x%02x",0);
                            else
                                printf("%02x",buf[memidx*4+3-nbytes]);
                        }
                    //dsp data packet is 24 bits
                    if (nbytes > 0)
                        rx_values[packets_rx] = buf[memidx*4+3-nbytes];
                    packets_rx = packets_rx + 1;
                    }
                    //read and print 4 bytes in little endian
                    if(DEBUG)
                        printf("\n\r");
                }
            }
        }
        if(DEBUG)
            DEBUG_PRINT("%d packets read\n\r", packets_rx-4);
        //write to file
        //frame_process(rx_values,packets_rx-4);
        fwrite(rx_values,sizeof(char),packets_rx-4,fp);
        packets_rx = 0;
        rx_occupancy = 0;

        enable_rx();    
        usleep(1000); //necessary to let the fifo fill up        
        shift_array(led_values,8,!led_values[7]);
        set_gpio_array(chipled, &leds, led_values);
    }
    return (void *)0;
}

// this needs to be updated for the new data format
#define DSP_FRAME_SIZE 18
void frame_process(char* packets, int size){
    //process the packets
    if (size % 4 != 0) {
        printf("Invalid packet size\n");
        return;
    }

    int num_packets = size / 4;
    int packet_index = 0;
    int correct = 0;

    while (packet_index < num_packets) {
        int packet = 0;
        packet = __builtin_bswap32(*(int*)&packets[4*packet_index])&0x00FFFFFF;
        // Check for Start of Frame (SOF) and End of Frame (EOF)
        if (packet == 0x00AAAAAA) {
            //printf("Start of Frame detected\n");
            // Check for End of Frame (EOF)
            if (4*packet_index + 4*(DSP_FRAME_SIZE-1) >= num_packets) {
                //printf("Invalid packet size\n");
                break;
            }
            packet = __builtin_bswap32(*(int*)&packets[4*packet_index + 4*(DSP_FRAME_SIZE-1)]) & 0x00FFFFFF;
            printf("Packet[%d]: %08x\n", DSP_FRAME_SIZE-1, packet);
            if (packet == 0x00AAFFFF) {
                //printf("End of Frame detected\n");
                correct = 1;
                packet_index++;
                //continue;
            } else {
                // Skip to next packet
                printf("Invalid End of Frame: %08x\n", packet);
                packet_index++;
                correct = 0;
                continue;
            }
        } 
        if (correct == 0){
            packet_index++;
            continue;
        }
        // if we are here, we have a valid frame
        // Check header
        //correct =
        for (int i = 0; i < 4; i++) {
            packet = (__builtin_bswap32(*(int*)&packets[4*packet_index + 4*i]))&0x00FFFFFF;
            //printf("channel: %d, Packet: 0x%08X\n", i, packet);
            // Calculate rolling average
            rolling_avg[i] =  rolling_avg[i] + ((alpha) * ((float)(packet & 0x0000FFFF) - rolling_avg[i]));
            rolling_avg[i] = rolling_avg[i];
            //rolling_avg[i] = rolling_sum[i] / alpha;
            //printf("channel: %d, Rolling Average: %3f\n", i, rolling_avg[i]);
        }
        packet_index += DSP_FRAME_SIZE;
        correct = 0;
        //printf("Packet: 0x%08X, Rolling Average: %d\n", packet, rolling_avg);
    }
    // print all averages so far in single line
    printf("\rCH0: %3f, CH1: %3f, CH2: %3f, CH3: %3f, CH4: %3f, CH5: %3f", rolling_avg[0], rolling_avg[1], rolling_avg[2], rolling_avg[3], rolling_avg[4], rolling_avg[5]);

}

static void signal_handler(int signal)
{
    switch (signal) {
        case SIGINT:
            running = false;
            break;
        case SIGTERM:
            running = false;
            break;
        case SIGQUIT:
            running = false;
            break;

        default:
            break;
    }
}

static void display_help(char * progName)
{
    printf("Usage : %s [OPTIONS]\n"
           "\n"
           "  -h, --help     Print this menu\n"
           "  -t, --devTx    Device to use ... /dev/axis_fifo_0x43c10000\n"
           "  -r, --devRx    Device to use ... /dev/axis_fifo_0x43c10000\n"
           ,
           progName
          );
}

static void print_opts()
{
    printf("Options : \n"
            "DevRx          : %s\n"
           ,
           _opt_dev_rx
          );
}

static int process_options(int argc, char * argv[])
{
        strcpy(_opt_dev_rx,DEF_DEV_RX);

        for (;;) {
            int option_index = 0;
            static const char *short_options = "hrno:t:";
            static const struct option long_options[] = {
                    {"help", no_argument, 0, 'h'},
                    {"devRx", required_argument, 0, 'r'},
                    {"nseconds", required_argument, 0, 'n'},
                    {"output", required_argument, 0, 'o'},
                    {0,0,0,0},
                    };

            int c = getopt_long(argc, argv, short_options,
            long_options, &option_index);

            if (c == EOF) {
            break;
            }

            switch (c) {

                case 'r':
                    strcpy(_opt_dev_rx, optarg);
                    break;

                default:
                case 'h':
                    display_help(argv[0]);
                    exit(0);
                    break;

                case 'n':
                    runtime = atoi(optarg);
                    break;

                case 'o':
                    strcpy(output_file, optarg);
                    break;
                    }
            }

        print_opts();
        return 0;
}
