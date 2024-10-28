/**
 * @file conf.h
 * @brief Configuration header for the GPIO data collector application.
 *
 * This header file contains macro definitions and function declarations
 * used for configuring the data collection chain in the GPIO application.
 *
 * @author Pooya Poolad
 * @date 23/10/2024
 */


#ifndef CONF_H
#define CONF_H

#define CONF_UIO_DEVICE "/dev/uio6"
#define MAP_SIZE 0x1000

/**
 * @brief Configures the data collection chain.
 *
 * This function sets up the data collection chain with the specified parameters.
 *
 * @param data Pointer to the data array to be configured.
 * @param num_words Number of words in the data array.
 * @param num_bits Number of bits in chain.
 * @param time_out Timeout value for the configuration process un us.
 * @return int Status code indicating success or failure of the configuration.
 */
void create_tdc_chain(int* enables, int* offsets, int* chain);
int configure_chain(int* data, int num_words, int num_bits, int time_out);

#endif
