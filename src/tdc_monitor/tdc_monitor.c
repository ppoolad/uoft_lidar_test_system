/**
 * @file tdc_test.c
 * @author Pooya Poolad ppoolad@outlook.com
 *      based on the driver by Jason Gutel jason.gutel@gmail.com
 * resets TDC and starts recording, stores them into a file
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

#include <sys/socket.h>
#include <netinet/in.h>
#include <gpiod.h>

#include "axis-fifo.h"
#include "asic.h"
#include "asic_control.h"
#include "conf.h"
#include "simple_rx.h"
/*----------------------------------------------------------------------------
 * Internal Definitions
 *----------------------------------------------------------------------------*/
//#define DEF_DEV_TX "/dev/axis_fifo_0x00000000a0070000"
#define DEF_DEV_RX "/dev/axis_fifo_0x00000000a0030000"
#define MAX_BUF_SIZE_BYTES 4096

#define DEBUG_PRINT(fmt, args...) printf("DEBUG %s:%d(): " fmt, \
        __func__, __LINE__, ##args)

#define DEBUG 0
struct thread_data {
    int rc;
};

pthread_t read_from_fifo_thread;

static volatile bool running = true;
static char _opt_dev_rx[255];
static int readFifoFd;

static void signal_handler(int signal);
static void *read_from_fifo_thread_fn(void *data);
static int process_options(int argc, char * argv[]);
static void print_opts();
static void display_help(char * progName);
static void quit(void);

//globlas
//gpio stuff
struct gpiod_chip *chip;// = gpiod_chip_open_by_name(HPC1_CHIP_NAME);
struct gpiod_chip *chipled;// = gpiod_chip_open_by_name(LED_CHIP_NAME);
struct gpiod_line_bulk gpios;
struct gpiod_line_bulk leds;

//large array to store the values
char rx_values[MAX_BUF_SIZE_BYTES+16] = {0};
//file handle to save the values
FILE *fp;

//led values indicating running
int led_values[8] = {0,0,0,0,0,0,0,1};
//shift 8 bit array;

int runtime = 120; //run for n secs
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
    /*   start tdc   */
    /*****************/

    //lets start tdc until it fills the fifo
    if (!chip) {
        perror("Open chip failed");
        exit(EXIT_FAILURE);
    }
    int hpc1_values[40] = {0};
    printf("setting gpios to 0\n");
    set_gpio_array(chip, &gpios, hpc1_values);

    //unreset tdc
    tdc_reset(chip);
    tdc_unreset(chip);

    //create tdc chain
    unsigned int chain_data[4] = {0};
    int enables[] = {0,0,0,1,1,0};
    int offsets[] = {0,0,0,0,0};
    create_tdc_chain(enables, offsets, chain_data);
    // configure the chain so only tdc channel 0 is enable
    //unsigned int chain_data[4] = {0,0,0x1,0xc0000000}; // binary [0000_/01][00/0000|/0000/00|00/0000|/0000/00|00/0000] -> 0x40000000
    int chain_num_words = 4;
    int chain_num_bits = 36;
    int chain_time_out = 100000;
    configure_chain(chain_data, chain_num_words, chain_num_bits, chain_time_out);

    printf("initiate tdc\n");
    tdc_test(chip, &gpios);

    printf("setting leds to 0x55\n");
    
    set_gpio_array(chipled, &leds, led_values);
    
    //open file
    fp = fopen("tdc_values_ch3exclusives.txt","w");
    if (fp == NULL) {
        perror("Failed to open file");
        return -1;
    }

    //enable rx 
    enable_rx();
    /*****************/
    /* start threads */
    /*****************/

    /* start thread listening for fifo receive packets */
    rc = pthread_create(&read_from_fifo_thread, NULL, read_from_fifo_thread_fn,
            (void *)NULL);

    /* perform noops */
    while (running) {
       sleep(runtime); //run for n minutes
       quit();
    }
    // printf("waiting....\n");
    // sleep(1); //wait a bit until fifo is full

    printf("turn off serializer\n");
    tdc_serializer(0,chip);

    printf("reset tdc\n");
    tdc_reset(chip);
    
    gpiod_chip_close(chip);
    gpiod_chip_close(chipled); //run leds to know it's done

    //close file
    fclose(fp);
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
        while(rx_occupancy < MAX_BUF_SIZE_BYTES && running){
            ioctl(readFifoFd, AXIS_FIFO_GET_RX_OCCUPANCY, &rx_occupancy);
            if(DEBUG) DEBUG_PRINT("rx_occupancy %d\n",rx_occupancy);
            usleep(100);
        }
        // stop serial tx
        // tdc_serializer(0,chip);
        // stop rx
        disable_rx();
        //tdc_reset(chip);
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
                                printf("0x%02x",buf[memidx*4+3-nbytes]);
                            else
                                printf("%02x",buf[memidx*4+3-nbytes]);
                        }
                    // if(DEBUG){
                    //     if(memidx == 0)
                    //         printf("0x%02x",buf[bytesFifo-1-memidx]);
                    //     else
                    //         printf("%02x",buf[bytesFifo-1-memidx]);
                    // }
                    rx_values[packets_rx] = buf[memidx*4+3-nbytes]; //buf[bytesFifo-1-memidx];
                    packets_rx = packets_rx + 1;
                    }
                    if(DEBUG)
                        printf("\n\r");
                }
            }
        }
        if(DEBUG)
            DEBUG_PRINT("%d packets read\n\r", packets_rx-4);
        //write to file
        fwrite(rx_values,sizeof(char),packets_rx-4,fp);
        packets_rx = 0;
        rx_occupancy = 0;
        //restart tdc
        if(DEBUG)
            DEBUG_PRINT("restart tdc\n\r");
        //tdc_unreset(chip);
        //tdc_serializer(1,chip);
        enable_rx();    
        usleep(1000); //necessary to let the fifo fill up        
        shift_array(led_values,8,!led_values[7]);
        set_gpio_array(chipled, &leds, led_values);
    }
    return (void *)0;
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
            static const char *short_options = "hrn:t:";
            static const struct option long_options[] = {
                    {"help", no_argument, 0, 'h'},
                    {"devRx", required_argument, 0, 'r'},
                    {"nseconds", required_argument, 0, 'n'},
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
                    }
            }

        print_opts();
        return 0;
}
