/**
 * @file tdc_test.c
 * @author Pooya Poolad ppoolad@outlook.com
 *      based on the driver by Jason Gutel jason.gutel@gmail.com
 * resets TDC and starts recording, stores them into a file
 * 
 **/

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
#include "../../../axisfifo/axis-fifo.h"
#include "../asic.h"
#include "asic_control.h"
#include <gpiod.h>
/*----------------------------------------------------------------------------
 * Internal Definitions
 *----------------------------------------------------------------------------*/
//#define DEF_DEV_TX "/dev/axis_fifo_0x00000000a0070000"
#define DEF_DEV_RX "/dev/axis_fifo_0x00000000a0030000"
#define MAX_BUF_SIZE_BYTES 512

#define DEBUG_PRINT(fmt, args...) printf("DEBUG %s:%d(): " fmt, \
        __func__, __LINE__, ##args)

#define DEBUG 1
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
char rx_values[4*512] = {0};
//file handle to save the values
FILE *fp;

//led values indicating running
int led_values[8] = {0,0,0,0,0,0,0,1};
//shift 8 bit array;

int runtime = 200; //run for n minutes
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

    printf("initiate tdc\n");
    tdc_test(chip, &gpios);

    printf("setting leds to 0x55\n");
    
    set_gpio_array(chipled, &leds, led_values);
    
    //open file
    fp = fopen("tdc_values.txt","w");
    if (fp == NULL) {
        perror("Failed to open file");
        return;
    }
    /*****************/
    /* start threads */
    /*****************/

    /* start thread listening for fifo receive packets */
    rc = pthread_create(&read_from_fifo_thread, NULL, read_from_fifo_thread_fn,
            (void *)NULL);

    /* perform noops */
    while (running) {
       sleep(600); //run for n minutes
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
        while(rx_occupancy < 512 && running){
            ioctl(readFifoFd, AXIS_FIFO_GET_RX_OCCUPANCY, &rx_occupancy);
            DEBUG_PRINT("rx_occupancy %d\n",rx_occupancy);
            usleep(100000);
        }
        // stop serial tx
        tdc_serializer(0,chip);
        tdc_reset(chip);
        // empty fifo

        while (packets_rx < rx_occupancy)
        {
            bytesFifo = read(readFifoFd, buf, MAX_BUF_SIZE_BYTES);
            if (bytesFifo > 0) {
                if(DEBUG){
                    DEBUG_PRINT("bytes from fifo %d\n",bytesFifo);
                    DEBUG_PRINT("Read : \n\r");
                }
                for (int memidx = 0; memidx < 4; memidx++)
                {
                    if(DEBUG){
                        if(memidx == 0)
                            printf("0x%02x",buf[bytesFifo-1-memidx]);
                        else
                            printf("%02x",buf[bytesFifo-1-memidx]);
                    }
                    rx_values[packets_rx] = buf[bytesFifo-1-memidx];
                    packets_rx = packets_rx + 1;
                }
                printf("\n\r");
            }
        }
        DEBUG_PRINT("%d packets read\n\r", packets_rx-4);
        //write to file
        fwrite(rx_values,sizeof(char),packets_rx-4,fp);
        packets_rx = 0;
        //restart tdc
        DEBUG_PRINT("restart tdc\n\r");
        tdc_unreset(chip);
        tdc_serializer(1,chip);
        usleep(10000); //necessary to let the fifo fill up        
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
