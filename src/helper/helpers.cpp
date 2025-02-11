
#include "helpers.h"
#include <iostream>
#include <cmath>
#include <string>


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