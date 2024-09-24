#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "asic.h"
#include "asic_control.h"

void set_gpio_value(struct gpiod_chip *chip, int gpio, int value) {
    struct gpiod_line *line = gpiod_chip_get_line(chip, gpio);
    if (!line) {
        perror("Get line failed");
        exit(EXIT_FAILURE);
    }
    if (gpiod_line_request_output(line, "example", value) < 0) {
        perror("Request line as output failed");
        exit(EXIT_FAILURE);
    }
    if (gpiod_line_set_value(line, value) < 0) {
        perror("Set line value failed");
        exit(EXIT_FAILURE);
    }
    gpiod_line_release(line);
}

void set_gpio_array(struct gpiod_chip *chip, struct gpiod_line_bulk *gpios, int *value) {
    int line = gpiod_chip_get_all_lines(chip, gpios);
    if (line < 0) {
        perror("Get line failed");
        exit(EXIT_FAILURE);
    }
    if (gpiod_line_request_bulk_output(gpios, "example", value) < 0) {
        perror("Request line as output failed");
        exit(EXIT_FAILURE);
    }
    if (gpiod_line_set_value_bulk(gpios, value) < 0) {
        perror("Set line value failed");
        exit(EXIT_FAILURE);
    }
    gpiod_line_release_bulk(gpios);
}


int get_gpio_value(struct gpiod_chip *chip, int gpio){
    struct gpiod_line *line = gpiod_chip_get_line(chip, gpio);
    int value;
    if (!line) {
        perror("Get line failed");
        exit(EXIT_FAILURE);
    }
    value = gpiod_line_get_value(line);
    if ( value == -1) {
        perror("Get line value failed");
        exit(EXIT_FAILURE);
    }
    gpiod_line_release(line);
    return value;
}

void get_gpio_array(struct gpiod_chip *chip, struct gpiod_line_bulk *gpios, int *value){
    int line = gpiod_chip_get_all_lines(chip, gpios);
    if (line < 0) {
        perror("Get line failed");
        exit(EXIT_FAILURE);
    }

    if (gpiod_line_get_value_bulk(gpios, value) < 0) {
        perror("Get line value failed");
        exit(EXIT_FAILURE);
    }
    gpiod_line_release_bulk(gpios);
}

int tdc_test(struct gpiod_chip *chip, struct gpiod_line_bulk *gpios){
    int gpio_values[HPC1_NUM_GPIO] = {0};
    set_gpio_array(chip, gpios, gpio_values);
        
    gpio_values[TDC_ARESETN] = 1; 	 //reset not
    gpio_values[TDC_START_MODE] = 1; 	 //enable start sych mode
    gpio_values[START_SIGNAL] = 1;
    gpio_values[TDC_EN] = 1;		 //enable tdc
    gpio_values[DIGITAL_EN] = 1;		 //enable tdc
    gpio_values[DSP_ARESETN] = 1;
    gpio_values[COARSE_EN] = 1;		 //enable tdc
    gpio_values[TDC_DEBUG_RD_EN] = 1;	 //enable serializer
    gpio_values[TDC_STOP_DEBUG_MODE] = 1;//set this one to get external signal;
    
    set_gpio_array(chip, gpios, gpio_values);
    printf("waiting....\n");
    sleep(5);
    gpio_values[START_SIGNAL] = 0;
    printf("set start to 0\n");
    set_gpio_array(chip, gpios, gpio_values);
    printf("done\n");
    
    return 0;
}

int tdc_serializer(int power_state,struct gpiod_chip *chip){

    //set power state 
    set_gpio_value(chip, TDC_DEBUG_RD_EN, power_state);
    
    return 0;

}

int tdc_reset(struct gpiod_chip *chip){
    set_gpio_value(chip, TDC_ARESETN, 0);
    return 0;
}

int tdc_unreset(struct gpiod_chip *chip){
    set_gpio_value(chip, TDC_ARESETN, 1);
    return 0;
}
// int main(){
//     struct gpiod_chip *chip = gpiod_chip_open_by_name(HPC1_CHIP_NAME);
//     struct gpiod_chip *chipled = gpiod_chip_open_by_name(LED_CHIP_NAME);
//     struct gpiod_line_bulk gpios;
//     struct gpiod_line_bulk leds;
//     if (!chip) {
//         perror("Open chip failed");
//         exit(EXIT_FAILURE);
//     }
//     int hpc1_values[40] = {0};
//     set_gpio_array(chip, &gpios, hpc1_values);

//     tdc_test(chip, &gpios);
//     int led_values[8] = {0,1,0,1,0,1,0,1};
//     set_gpio_array(chipled, &leds, led_values);
    
//     gpiod_chip_close(chip);
//     gpiod_chip_close(chipled);
    

// }