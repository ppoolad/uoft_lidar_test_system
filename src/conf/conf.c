#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "conf.h"

void create_tdc_chain(int* enables, int* offsets, int* chain) {
    //int* chain = (int*) calloc(4, sizeof(int)); //configurere is 4 regs long (128 bits)
    // TDC chain is only 36 bits long, so it only uses 2 LSB registers of the chain from 4 (2 & 3) 
    // enables are MSB bits of that chain
    
    chain[2] = ((enables[5]&1) << 3) | ((enables[4]&1) << 2) | ((enables[3]&1) << 1) | ((enables[2]&1) << 0);
    chain[3] = ((enables[1]&1) << 31) | ((enables[0]&1) << 30) | ((offsets[4]&0x3F) << 24) | ((offsets[3]&0x3F )<< 18) | ((offsets[2]&0x3F) << 12) | ((offsets[1]&0x3F) << 6) | (offsets[0]&0x3F);
    chain[0] = 0;
    chain[1] = 0;
    printf("Chain: 0x%08x 0x%08x 0x%08x 0x%08x\n", chain[0], chain[1], chain[2], chain[3]);
    //print binary for debug
    printf("Binary: ");
    for (int i = 0; i < 4; i++) {
        for (int j = 31; j >= 0; j--) {
            printf("%d", (chain[i] >> j) & 1);
            if (j % 4 == 0) {
                printf(" ");
            }
        }
    }
    printf("\n");

}

void create_dsp_tof_chain(int number, int* chain){
    chain[0] = 0;
    chain[1] = 0;
    chain[2] = 0;
    chain[3] = number;
    
    printf("Chain: 0x%08x 0x%08x 0x%08x 0x%08x\n", chain[0], chain[1], chain[2], chain[3]);

}

int configure_chain(int* data, int num_words, int num_bits, int time_out) {
    int fd;
    void *map_base;
    volatile unsigned int *reg;

    // Open the UIO device file
    fd = open(CONF_UIO_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("Failed to open UIO device");
        return EXIT_FAILURE;
    }

    // Memory map the UIO device
    map_base = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map_base == MAP_FAILED) {
        perror("Failed to mmap UIO device");
        close(fd);
        return EXIT_FAILURE;
    }

    // Write data to registers 0 to 6
    reg = (volatile unsigned int *)map_base;
    for (int i = 0; i < num_words; i++) {
        printf("Writing 0x%08x to register %d\n", data[i], i + 2);
        reg[i + 2] = data[i];
    }
    // reg[2] = 0xFF00FF00; //data [127:96]
    // reg[3] = 0xF0F0F0F0; //data [95:64]
    // reg[4] = 0x33333333; //data [63:32]
    // reg[5] = 0xAAAAAAAA; //data [31:0]
    // write the number of bits
    printf("Writing %d to register 1 (COUNT)\n", num_bits);
    reg[1] = num_bits; // Set the number of data words to 128
    
    // Start the configuration process
    printf("Load in data\n");
    reg[0] = 0x01; // load the data
    //sleep for a tiny bit
    
    usleep(1);
    printf("Start configuration\n");
    reg[0] = 0x02; // start the configuration
    
    // Read data from register 7
    unsigned int status_read = 0;
    int time_out_ctr = 0;
    while (status_read != 1 && time_out_ctr < time_out) {
        status_read = reg[7];
        usleep(1);
        time_out_ctr++;
    }
    printf("Read 0x%08x from register 7\n", status_read);

    // Clean up
    if (munmap(map_base, MAP_SIZE) == -1) {
        perror("Failed to munmap UIO device");
    }
    close(fd);

    return EXIT_SUCCESS;
}
