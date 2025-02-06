// Description: Helper functions for the UofT LiDAR Test System

// Include guard
#ifndef HELPERS_H
#define HELPERS_H

// processes runtime arguments from terminal
void process_config(int argc, char** argv);

// shifts an array to the right and inserts a value at the beginning
void shift_array(int *array, int size, int insert);

// generates a random number with a given mean and standard deviation using the Box-Muller transform
int generate_random_number(double mean, double std_dev);

#endif // HELPERS_H