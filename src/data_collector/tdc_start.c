#include <gpiod.h>
#include "asic_control.h"
#include "../asic.h"
int main(int argc, char **argv)
{
    //globlas
//gpio stuff
    struct gpiod_chip *chip;// = gpiod_chip_open_by_name(HPC1_CHIP_NAME);
    struct gpiod_chip *chipled;// = gpiod_chip_open_by_name(LED_CHIP_NAME);
    struct gpiod_line_bulk gpios;
    struct gpiod_line_bulk leds;
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
    tdc_test(chip, &gpios);
    int led_values[8] = {1,0,1,0,1,0,1,0};
    set_gpio_array(chipled, &leds, led_values);
}