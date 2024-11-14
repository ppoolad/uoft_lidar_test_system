// simple RX is a uio device that has 1 register where bit0 enable and disable the rx
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include "simple_rx.h"

volatile uint32_t *rx_register;

int init_rx() {
    int fd = open(UIO_DEVICE, O_RDWR);
    int status;
    if (fd < 0) {
        perror("Failed to open UIO device");
        return -1;
    }

    rx_register = (volatile uint32_t *)mmap(NULL, sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (rx_register == MAP_FAILED) {
        perror("Failed to mmap UIO device");
        close(fd);
        return -1;
    }

    status = rx_register[0] & RX_ENABLE_MASK;
    if (status) {
        printf("RX is enabled\n");
    } else {
        printf("RX is disabled\n");
    }

    return 0;
}

void enable_rx() {
    *rx_register |= RX_ENABLE_MASK;
}

void disable_rx() {
    *rx_register &= ~RX_ENABLE_MASK;
}

void set_rx_nbits(int nbits) {
    // 8 MSB bits of the 32 bit register are used to set the number of bits to read from the fifo
    *rx_register &= 0x00FFFFFF;
    *rx_register |= (nbits << 24);
}

void cleanup_rx() {
    munmap((void *)rx_register, sizeof(uint32_t));
}