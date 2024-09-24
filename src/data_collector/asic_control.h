#ifndef ASIC_CONTROL_H
#define ASIC_CONTROL_H
#include <gpiod.h>

int tdc_test(struct gpiod_chip *chip, struct gpiod_line_bulk *gpios);
int tdc_reset(struct gpiod_chip *chip);
int tdc_unreset(struct gpiod_chip *chip);
int tdc_serializer(int power_state,struct gpiod_chip *chip);
void set_gpio_array(struct gpiod_chip *chip, struct gpiod_line_bulk *gpios, int *value);
void set_gpio_value(struct gpiod_chip *chip, int gpio, int value);
int get_gpio_value(struct gpiod_chip *chip, int gpio);
void get_gpio_array(struct gpiod_chip *chip, struct gpiod_line_bulk *gpios, int *value);


#endif /* ASIC_CONTROL_H */